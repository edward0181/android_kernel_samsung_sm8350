// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/interconnect.h>
#include "cam_soc_bus.h"
/**
 * struct cam_soc_bus_client_data : Bus client data
 *
 * @icc_data: Bus icc path information
 */
struct cam_soc_bus_client_data {
	struct icc_path *icc_data;
#if defined(CONFIG_SAMSUNG_SBI_QOS_TUNE)
	uint32_t num_additional_paths;
	struct icc_path *additional_paths[2];
#endif
};

int cam_soc_bus_client_update_request(void *client, unsigned int idx)
{
	int rc = 0;
	uint64_t ab = 0, ib = 0;
	struct cam_soc_bus_client *bus_client =
		(struct cam_soc_bus_client *) client;
	struct cam_soc_bus_client_data *bus_client_data =
		(struct cam_soc_bus_client_data *) bus_client->client_data;

	if (idx >= bus_client->common_data->num_usecases) {
		CAM_ERR(CAM_UTIL, "Invalid vote level=%d, usecases=%d", idx,
			bus_client->common_data->num_usecases);
		rc = -EINVAL;
		goto end;
	}

	ab = bus_client->common_data->bw_pair[idx].ab;
	ib = bus_client->common_data->bw_pair[idx].ib;

	CAM_DBG(CAM_PERF, "Bus client=[%s] index[%d] ab[%llu] ib[%llu]",
		bus_client->common_data->name, idx, ab, ib);

	rc = icc_set_bw(bus_client_data->icc_data, Bps_to_icc(ab),
		Bps_to_icc(ib));
	if (rc) {
		CAM_ERR(CAM_UTIL,
			"Update request failed, client[%s], idx: %d",
			bus_client->common_data->name, idx);
		goto end;
	}

end:
	return rc;
}
#if defined(CONFIG_SAMSUNG_SBI_QOS_TUNE)
int cam_soc_bus_client_update_bw(void *client, uint64_t ab, uint64_t ib,
	uint64_t additional_ib)
#else
int cam_soc_bus_client_update_bw(void *client, uint64_t ab, uint64_t ib)
#endif
{
	struct cam_soc_bus_client *bus_client =
		(struct cam_soc_bus_client *) client;
	struct cam_soc_bus_client_data *bus_client_data =
		(struct cam_soc_bus_client_data *) bus_client->client_data;
	int rc = 0;

	CAM_DBG(CAM_PERF, "Bus client=[%s] :ab[%llu] ib[%llu]",
		bus_client->common_data->name, ab, ib);
	rc = icc_set_bw(bus_client_data->icc_data, Bps_to_icc(ab),
		Bps_to_icc(ib));
	if (rc) {
		CAM_ERR(CAM_UTIL, "Update request failed, client[%s]",
			bus_client->common_data->name);
		goto end;
	}
#if defined(CONFIG_SAMSUNG_SBI_QOS_TUNE)
	if (bus_client_data->num_additional_paths > 0) {
		CAM_DBG(CAM_PERF, "Bus client=[%s] : Additional path :ab[%llu] ib[%llu]",
			bus_client->common_data->name, 0, additional_ib);
		rc = icc_set_bw(bus_client_data->additional_paths[0], 0,
			Bps_to_icc(additional_ib));
		if (rc) {
			CAM_ERR(CAM_UTIL, "Update request failed, client[%s]",
				bus_client->common_data->name);
			rc = 0;
		}
	}
#endif
end:
	return rc;
}

int cam_soc_bus_client_register(struct platform_device *pdev,
	struct device_node *dev_node, void **client,
	struct cam_soc_bus_client_common_data *common_data)
{
	struct cam_soc_bus_client *bus_client = NULL;
	struct cam_soc_bus_client_data *bus_client_data = NULL;
	int rc = 0;

	bus_client = kzalloc(sizeof(struct cam_soc_bus_client), GFP_KERNEL);
	if (!bus_client) {
		CAM_ERR(CAM_UTIL, "soc bus client is NULL");
		rc = -ENOMEM;
		goto end;
	}

	*client = bus_client;

	bus_client_data = kzalloc(sizeof(struct cam_soc_bus_client_data),
		GFP_KERNEL);
	if (!bus_client_data) {
		kfree(bus_client);
		*client = NULL;
		rc = -ENOMEM;
		goto end;
	}

	bus_client->client_data = bus_client_data;
	bus_client->common_data = common_data;
	bus_client_data->icc_data = icc_get(&pdev->dev,
		bus_client->common_data->src_id,
		bus_client->common_data->dst_id);
	if (!bus_client_data->icc_data) {
		CAM_ERR(CAM_UTIL, "failed in register bus client");
		rc = -EINVAL;
		goto error;
	}

	rc = icc_set_bw(bus_client_data->icc_data, 0, 0);
	if (rc) {
		CAM_ERR(CAM_UTIL, "Bus client update request failed, rc = %d",
			rc);
		goto fail_unregister_client;
	}

	CAM_DBG(CAM_PERF, "Register Bus Client=[%s] : src=%d, dst=%d",
		bus_client->common_data->name, bus_client->common_data->src_id,
		bus_client->common_data->dst_id);
#if defined(CONFIG_SAMSUNG_SBI_QOS_TUNE)
	if (bus_client->common_data->num_additional_paths > 0) {
		bus_client_data->additional_paths[0] = icc_get(&pdev->dev,
			bus_client->common_data->add_src_id[0],
			bus_client->common_data->add_dst_id[0]);
		if (!bus_client_data->additional_paths[0]) {
			CAM_ERR(CAM_UTIL, "failed in register bus client");
			rc = 0;
			goto done;
		}

		rc = icc_set_bw(bus_client_data->additional_paths[0], 0, 0);
		if (rc) {
			CAM_ERR(CAM_UTIL, "Bus client update request failed, rc = %d",
				rc);
			goto fail_unregister_client;
		}

		CAM_DBG(CAM_PERF, "Register Bus Client=[%s] : src=%d, dst=%d",
			bus_client->common_data->name, bus_client->common_data->add_src_id[0],
			bus_client->common_data->add_dst_id[0]);

		bus_client_data->num_additional_paths = 1;
	}

done:
#endif
	return 0;

fail_unregister_client:
	icc_put(bus_client_data->icc_data);
error:
	kfree(bus_client_data);
	bus_client->client_data = NULL;
	kfree(bus_client);
	*client = NULL;
end:
	return rc;

}

void cam_soc_bus_client_unregister(void **client)
{
	struct cam_soc_bus_client *bus_client =
		(struct cam_soc_bus_client *) (*client);
	struct cam_soc_bus_client_data *bus_client_data =
		(struct cam_soc_bus_client_data *) bus_client->client_data;

	icc_put(bus_client_data->icc_data);
#if defined(CONFIG_SAMSUNG_SBI_QOS_TUNE)
	if (bus_client_data->num_additional_paths > 0)
		icc_put(bus_client_data->additional_paths[0]);
#endif
	kfree(bus_client_data);
	bus_client->client_data = NULL;
	kfree(bus_client);
	*client = NULL;

}
