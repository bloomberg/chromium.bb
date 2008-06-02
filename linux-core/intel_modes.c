/*
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 * Copyright (c) 2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 */

#include <linux/i2c.h>
#include <linux/fb.h>
#include "drmP.h"
#include "intel_drv.h"

/**
 * intel_ddc_probe
 *
 */
bool intel_ddc_probe(struct intel_output *intel_output)
{
	u8 out_buf[] = { 0x0, 0x0};
	u8 buf[2];
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr = 0x50,
			.flags = 0,
			.len = 1,
			.buf = out_buf,
		},
		{
			.addr = 0x50,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = buf,
		}
	};

	ret = i2c_transfer(&intel_output->ddc_bus->adapter, msgs, 2);
	if (ret == 2)
		return true;

	return false;
}

/**
 * intel_ddc_get_modes - get modelist from monitor
 * @connector: DRM connector device to use
 *
 * Fetch the EDID information from @connector using the DDC bus.
 */
int intel_ddc_get_modes(struct intel_output *intel_output)
{
	struct edid *edid;
	int ret = 0;

	edid = drm_get_edid(&intel_output->base, &intel_output->ddc_bus->adapter);
	if (edid) {
		drm_mode_connector_update_edid_property(&intel_output->base, edid);
		ret = drm_add_edid_modes(&intel_output->base, edid);
		kfree(edid);
	}
	return ret;
}
