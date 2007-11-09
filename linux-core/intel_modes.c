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
bool intel_ddc_probe(struct drm_output *output)
{
	struct intel_output *intel_output = output->driver_private;
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
 * @output: DRM output device to use
 *
 * Fetch the EDID information from @output using the DDC bus.
 */
int intel_ddc_get_modes(struct drm_output *output)
{
	struct intel_output *intel_output = output->driver_private;
	struct edid *edid;
	int ret = 0;

	edid = drm_get_edid(output, &intel_output->ddc_bus->adapter);
	if (edid) {
		ret = drm_add_edid_modes(output, edid);
		kfree(edid);
	}
	return ret;
}
