/*
 * Copyright 2006 Dave Airlie <airlied@linux.ie>
 * Copyright © 2006 Intel Corporation
 *   Eric Anholt <eric@anholt.net>
 *   Jesse Barnes <jesse.barnes@intel.com>
 */

#include <linux/i2c.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"

/**
 * Sets the backlight level.
 *
 * \param level backlight level, from 0 to i830_lvds_get_max_backlight().
 */
static void lvds_set_backlight(drm_device_t *dev, u32 level)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long blc_pwm_ctl;

	level &= BACKLIGHT_DUTY_CYCLE_MASK;
	blc_pwm_ctl = I915_READ(BLC_PWM_CTL) & ~BACKLIGHT_DUTY_CYCLE_MASK;
	I915_WRITE(BLC_PWM_CTL, blc_pwm_ctl |
		   (level << BACKLIGHT_DUTY_CYCLE_SHIFT));
}

/**
 * Returns the maximum level of the backlight duty cycle field.
 */
static u32 lvds_get_max_backlight(drm_device_t *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
    
	return ((I915_READ(BLC_PWM_CTL) & BACKLIGHT_MODULATION_FREQ_MASK) >>
		BACKLIGHT_MODULATION_FREQ_SHIFT) * 2;
}

int lvds_backlight(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	unsigned long dvoa_enabled, dvob_enabled, dvoc_enabled, lvds_enabled;
	drm_i915_private_t *dev_priv = dev->dev_private;

	printk(KERN_ERR "max backlight value: %d\n",
	       lvds_get_max_backlight(dev));
	dvoa_enabled = I915_READ(DVOA);
	dvob_enabled = I915_READ(DVOB);
	dvoc_enabled = I915_READ(DVOC);
	lvds_enabled = I915_READ(LVDS);

	printk(KERN_ERR "dvoa_enabled: 0x%08lx\n", dvoa_enabled);
	printk(KERN_ERR "dvob_enabled: 0x%08lx\n", dvob_enabled);
	printk(KERN_ERR "dvoc_enabled: 0x%08lx\n", dvoc_enabled);
	printk(KERN_ERR "lvds_enabled: 0x%08lx\n", lvds_enabled);
	printk(KERN_ERR "BLC_PWM_CTL: 0x%08x\n", I915_READ(BLC_PWM_CTL));
	
	return 0;
}

static const struct drm_output_funcs intel_lvds_output_funcs;

/**
 * intel_lvds_init - setup LVDS outputs on this device
 * @dev: drm device
 *
 * Create the output, register the LVDS DDC bus, and try to figure out what
 * modes we can display on the LVDS panel (if present).
 */
void intel_lvds_init(drm_device_t *dev)
{
	struct drm_output *output;
	struct intel_output *intel_output;
	int modes;

	output = drm_output_create(dev, &intel_lvds_output_funcs, "LVDS");
	if (!output)
		return;

	intel_output = kmalloc(sizeof(struct intel_output), GFP_KERNEL);
	if (!intel_output) {
		drm_output_destroy(output);
		return;
	}

	intel_output->type = INTEL_OUTPUT_LVDS;
	output->driver_private = intel_output;
	output->subpixel_order = SubPixelHorizontalRGB;
	output->interlace_allowed = 0;
	output->doublescan_allowed = 0;

	intel_output->ddc_bus = intel_i2c_create(dev, GPIOC, "LVDSDDC_C");
	if (!intel_output->ddc_bus) {
		dev_printk(KERN_ERR, &dev->pdev->dev, "DDC bus registration "
			   "failed.\n");
		return;
	}

	modes = intel_ddc_get_modes(output);
	printk(KERN_ERR "LVDS: added %d modes from EDID.\n", modes);
	intel_i2c_destroy(intel_output->ddc_bus);
	drm_output_destroy(output);
}

