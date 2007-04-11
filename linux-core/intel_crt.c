/*
 * Copyright © 2006-2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 */

#include <linux/i2c.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"

static void intel_crt_dpms(struct drm_output *output, int mode)
{
	drm_device_t *dev = output->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 temp;
	
	temp = I915_READ(ADPA);
	temp &= ~(ADPA_HSYNC_CNTL_DISABLE | ADPA_VSYNC_CNTL_DISABLE);
	temp &= ~ADPA_DAC_ENABLE;
	
	switch(mode) {
	case DPMSModeOn:
		temp |= ADPA_DAC_ENABLE;
		break;
	case DPMSModeStandby:
		temp |= ADPA_DAC_ENABLE | ADPA_HSYNC_CNTL_DISABLE;
		break;
	case DPMSModeSuspend:
		temp |= ADPA_DAC_ENABLE | ADPA_VSYNC_CNTL_DISABLE;
		break;
	case DPMSModeOff:
		temp |= ADPA_HSYNC_CNTL_DISABLE | ADPA_VSYNC_CNTL_DISABLE;
		break;
	}
	
	I915_WRITE(ADPA, temp);
}

static void intel_crt_save(struct drm_output *output)
{
	
}

static void intel_crt_restore(struct drm_output *output)
{

}

static int intel_crt_mode_valid(struct drm_output *output,
				struct drm_display_mode *mode)
{
	if (mode->flags & V_DBLSCAN)
		return MODE_NO_DBLESCAN;

	if (mode->clock > 400000 || mode->clock < 25000)
		return MODE_CLOCK_RANGE;

	return MODE_OK;
}

static bool intel_crt_mode_fixup(struct drm_output *output,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void intel_crt_mode_set(struct drm_output *output,
			       struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode)
{
	drm_device_t *dev = output->dev;
	struct drm_crtc *crtc = output->crtc;
	struct intel_crtc *intel_crtc = crtc->driver_private;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int dpll_md_reg;
	u32 adpa, dpll_md;

	if (intel_crtc->pipe == 0) 
		dpll_md_reg = DPLL_A_MD;
	else
		dpll_md_reg = DPLL_B_MD;

	/*
	 * Disable separate mode multiplier used when cloning SDVO to CRT
	 * XXX this needs to be adjusted when we really are cloning
	 */
	if (IS_I965G(dev)) {
		dpll_md = I915_READ(dpll_md_reg);
		I915_WRITE(dpll_md_reg,
			   dpll_md & ~DPLL_MD_UDI_MULTIPLIER_MASK);
	}
	
	adpa = 0;
	if (adjusted_mode->flags & V_PHSYNC)
		adpa |= ADPA_HSYNC_ACTIVE_HIGH;
	if (adjusted_mode->flags & V_PVSYNC)
		adpa |= ADPA_VSYNC_ACTIVE_HIGH;
	
	if (intel_crtc->pipe == 0)
		adpa |= ADPA_PIPE_A_SELECT;
	else
		adpa |= ADPA_PIPE_B_SELECT;
	
	I915_WRITE(ADPA, adpa);
}

/**
 * Uses CRT_HOTPLUG_EN and CRT_HOTPLUG_STAT to detect CRT presence.
 *
 * Only for I945G/GM.
 *
 * \return TRUE if CRT is connected.
 * \return FALSE if CRT is disconnected.
 */
static bool intel_crt_detect_hotplug(struct drm_output *output)
{
	drm_device_t *dev = output->dev;
//	struct intel_output *intel_output = output->driver_private;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 temp;
//	const int timeout_ms = 1000;
//	int starttime, curtime;

	temp = I915_READ(PORT_HOTPLUG_EN);

	I915_WRITE(PORT_HOTPLUG_EN, temp | CRT_HOTPLUG_FORCE_DETECT | (1 << 5));
#if 0	
	for (curtime = starttime = GetTimeInMillis();
	     (curtime - starttime) < timeout_ms; curtime = GetTimeInMillis())
	{
		if ((I915_READ(PORT_HOTPLUG_EN) & CRT_HOTPLUG_FORCE_DETECT) == 0)
			break;
	}
#endif
	if ((I915_READ(PORT_HOTPLUG_STAT) & CRT_HOTPLUG_MONITOR_MASK) ==
	    CRT_HOTPLUG_MONITOR_COLOR)
	{
		return true;
	} else {
		return false;
	}
}

static bool intel_crt_detect_ddc(struct drm_output *output)
{
	struct intel_output *intel_output = output->driver_private;

	/* CRT should always be at 0, but check anyway */
	if (intel_output->type != INTEL_OUTPUT_ANALOG)
		return false;
	
	return intel_ddc_probe(output);
}

static enum drm_output_status intel_crt_detect(struct drm_output *output)
{
	drm_device_t *dev = output->dev;
	struct intel_output *intel_output = output->driver_private;
	
	if (IS_I945G(dev)| IS_I945GM(dev) || IS_I965G(dev)) {
		if (intel_crt_detect_hotplug(output))
			return output_status_connected;
		else
			return output_status_disconnected;
	}

	/* Set up the DDC bus. */
	intel_output->ddc_bus = intel_i2c_create(dev, GPIOA, "CRTDDC_A");
	if (!intel_output->ddc_bus) {
		dev_printk(KERN_ERR, &dev->pdev->dev, "DDC bus registration "
			   "failed.\n");
		return 0;
	}

	if (intel_crt_detect_ddc(output)) {
		intel_i2c_destroy(intel_output->ddc_bus);
		return output_status_connected;
	}

	intel_i2c_destroy(intel_output->ddc_bus);
	/* TODO use load detect */
	return output_status_unknown;
}

static void intel_crt_destroy(struct drm_output *output)
{
	if (output->driver_private)
		kfree(output->driver_private);
}

static int intel_crt_get_modes(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct intel_output *intel_output = output->driver_private;
	int ret;

	/* Set up the DDC bus. */
	intel_output->ddc_bus = intel_i2c_create(dev, GPIOA, "CRTDDC_A");
	if (!intel_output->ddc_bus) {
		dev_printk(KERN_ERR, &dev->pdev->dev, "DDC bus registration "
			   "failed.\n");
		return 0;
	}

	ret = intel_ddc_get_modes(output);
	intel_i2c_destroy(intel_output->ddc_bus);
	return ret;
}

/*
 * Routines for controlling stuff on the analog port
 */
static const struct drm_output_funcs intel_crt_output_funcs = {
	.dpms = intel_crt_dpms,
	.save = intel_crt_save,
	.restore = intel_crt_restore,
	.mode_valid = intel_crt_mode_valid,
	.mode_fixup = intel_crt_mode_fixup,
	.prepare = intel_output_prepare,
	.mode_set = intel_crt_mode_set,
	.commit = intel_output_commit,
	.detect = intel_crt_detect,
	.get_modes = intel_crt_get_modes,
	.cleanup = intel_crt_destroy,
};

void intel_crt_init(drm_device_t *dev)
{
	struct drm_output *output;
	struct intel_output *intel_output;

	output = drm_output_create(dev, &intel_crt_output_funcs, "VGA");

	intel_output = kmalloc(sizeof(struct intel_output), GFP_KERNEL);
	if (!intel_output) {
		drm_output_destroy(output);
		return;
	}

	intel_output->type = INTEL_OUTPUT_ANALOG;
	output->driver_private = intel_output;
	output->interlace_allowed = 0;
	output->doublescan_allowed = 0;
}
