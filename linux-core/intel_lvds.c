/*
 * Copyright © 2006-2007 Intel Corporation
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
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
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */

#include <linux/i2c.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_edid.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"

/**
 * Sets the backlight level.
 *
 * \param level backlight level, from 0 to intel_lvds_get_max_backlight().
 */
static void intel_lvds_set_backlight(struct drm_device *dev, int level)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 blc_pwm_ctl;

	blc_pwm_ctl = I915_READ(BLC_PWM_CTL) & ~BACKLIGHT_DUTY_CYCLE_MASK;
	I915_WRITE(BLC_PWM_CTL, (blc_pwm_ctl |
				 (level << BACKLIGHT_DUTY_CYCLE_SHIFT)));
}

/**
 * Returns the maximum level of the backlight duty cycle field.
 */
static u32 intel_lvds_get_max_backlight(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
    
	return ((I915_READ(BLC_PWM_CTL) & BACKLIGHT_MODULATION_FREQ_MASK) >>
		BACKLIGHT_MODULATION_FREQ_SHIFT) * 2;
}

/**
 * Sets the power state for the panel.
 */
static void intel_lvds_set_power(struct drm_device *dev, bool on)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 pp_status;

	if (on) {
		I915_WRITE(PP_CONTROL, I915_READ(PP_CONTROL) |
			   POWER_TARGET_ON);
		do {
			pp_status = I915_READ(PP_STATUS);
		} while ((pp_status & PP_ON) == 0);

		intel_lvds_set_backlight(dev, dev_priv->backlight_duty_cycle);
	} else {
		intel_lvds_set_backlight(dev, 0);

		I915_WRITE(PP_CONTROL, I915_READ(PP_CONTROL) &
			   ~POWER_TARGET_ON);
		do {
			pp_status = I915_READ(PP_STATUS);
		} while (pp_status & PP_ON);
	}
}

static void intel_lvds_dpms(struct drm_output *output, int mode)
{
	struct drm_device *dev = output->dev;

	if (mode == DPMSModeOn)
		intel_lvds_set_power(dev, true);
	else
		intel_lvds_set_power(dev, false);

	/* XXX: We never power down the LVDS pairs. */
}

static void intel_lvds_save(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	dev_priv->savePP_ON = I915_READ(LVDSPP_ON);
	dev_priv->savePP_OFF = I915_READ(LVDSPP_OFF);
	dev_priv->savePP_CONTROL = I915_READ(PP_CONTROL);
	dev_priv->savePP_CYCLE = I915_READ(PP_CYCLE);
	dev_priv->saveBLC_PWM_CTL = I915_READ(BLC_PWM_CTL);
	dev_priv->backlight_duty_cycle = (dev_priv->saveBLC_PWM_CTL &
				       BACKLIGHT_DUTY_CYCLE_MASK);

	/*
	 * If the light is off at server startup, just make it full brightness
	 */
	if (dev_priv->backlight_duty_cycle == 0)
		dev_priv->backlight_duty_cycle =
			intel_lvds_get_max_backlight(dev);
}

static void intel_lvds_restore(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(BLC_PWM_CTL, dev_priv->saveBLC_PWM_CTL);
	I915_WRITE(LVDSPP_ON, dev_priv->savePP_ON);
	I915_WRITE(LVDSPP_OFF, dev_priv->savePP_OFF);
	I915_WRITE(PP_CYCLE, dev_priv->savePP_CYCLE);
	I915_WRITE(PP_CONTROL, dev_priv->savePP_CONTROL);
	if (dev_priv->savePP_CONTROL & POWER_TARGET_ON)
		intel_lvds_set_power(dev, true);
	else
		intel_lvds_set_power(dev, false);
}

static int intel_lvds_mode_valid(struct drm_output *output,
				 struct drm_display_mode *mode)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_display_mode *fixed_mode = dev_priv->panel_fixed_mode;

	if (fixed_mode)	{
		if (mode->hdisplay > fixed_mode->hdisplay)
			return MODE_PANEL;
		if (mode->vdisplay > fixed_mode->vdisplay)
			return MODE_PANEL;
	}

	return MODE_OK;
}

static bool intel_lvds_mode_fixup(struct drm_output *output,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = output->crtc->driver_private;
	struct drm_output *tmp_output;

	/* Should never happen!! */
	if (!IS_I965G(dev) && intel_crtc->pipe == 0) {
		printk(KERN_ERR "Can't support LVDS on pipe A\n");
		return false;
	}

	/* Should never happen!! */
	list_for_each_entry(tmp_output, &dev->mode_config.output_list, head) {
		if (tmp_output != output && tmp_output->crtc == output->crtc) {
			printk(KERN_ERR "Can't enable LVDS and another "
			       "output on the same pipe\n");
			return false;
		}
	}

	/*
	 * If we have timings from the BIOS for the panel, put them in
	 * to the adjusted mode.  The CRTC will be set up for this mode,
	 * with the panel scaling set up to source from the H/VDisplay
	 * of the original mode.
	 */
	if (dev_priv->panel_fixed_mode != NULL) {
		adjusted_mode->hdisplay = dev_priv->panel_fixed_mode->hdisplay;
		adjusted_mode->hsync_start =
			dev_priv->panel_fixed_mode->hsync_start;
		adjusted_mode->hsync_end =
			dev_priv->panel_fixed_mode->hsync_end;
		adjusted_mode->htotal = dev_priv->panel_fixed_mode->htotal;
		adjusted_mode->vdisplay = dev_priv->panel_fixed_mode->vdisplay;
		adjusted_mode->vsync_start =
			dev_priv->panel_fixed_mode->vsync_start;
		adjusted_mode->vsync_end =
			dev_priv->panel_fixed_mode->vsync_end;
		adjusted_mode->vtotal = dev_priv->panel_fixed_mode->vtotal;
		adjusted_mode->clock = dev_priv->panel_fixed_mode->clock;
		drm_mode_set_crtcinfo(adjusted_mode, CRTC_INTERLACE_HALVE_V);
	}

	/*
	 * XXX: It would be nice to support lower refresh rates on the
	 * panels to reduce power consumption, and perhaps match the
	 * user's requested refresh rate.
	 */

	return true;
}

static void intel_lvds_prepare(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	dev_priv->saveBLC_PWM_CTL = I915_READ(BLC_PWM_CTL);
	dev_priv->backlight_duty_cycle = (dev_priv->saveBLC_PWM_CTL &
				       BACKLIGHT_DUTY_CYCLE_MASK);

	intel_lvds_set_power(dev, false);
}

static void intel_lvds_commit( struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (dev_priv->backlight_duty_cycle == 0)
		dev_priv->backlight_duty_cycle =
			intel_lvds_get_max_backlight(dev);

	intel_lvds_set_power(dev, true);
}

static void intel_lvds_mode_set(struct drm_output *output,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = output->crtc->driver_private;
	u32 pfit_control;

	/*
	 * The LVDS pin pair will already have been turned on in the
	 * intel_crtc_mode_set since it has a large impact on the DPLL
	 * settings.
	 */

	/*
	 * Enable automatic panel scaling so that non-native modes fill the
	 * screen.  Should be enabled before the pipe is enabled, according to
	 * register description and PRM.
	 */
	if (mode->hdisplay != adjusted_mode->hdisplay ||
	    mode->vdisplay != adjusted_mode->vdisplay)
		pfit_control = (PFIT_ENABLE | VERT_AUTO_SCALE |
				HORIZ_AUTO_SCALE | VERT_INTERP_BILINEAR |
				HORIZ_INTERP_BILINEAR);
	else
		pfit_control = 0;

	if (!IS_I965G(dev)) {
		if (dev_priv->panel_wants_dither)
			pfit_control |= PANEL_8TO6_DITHER_ENABLE;
	}
	else
		pfit_control |= intel_crtc->pipe << PFIT_PIPE_SHIFT;

	I915_WRITE(PFIT_CONTROL, pfit_control);
}

/**
 * Detect the LVDS connection.
 *
 * This always returns OUTPUT_STATUS_CONNECTED.  This output should only have
 * been set up if the LVDS was actually connected anyway.
 */
static enum drm_output_status intel_lvds_detect(struct drm_output *output)
{
	return output_status_connected;
}

/**
 * Return the list of DDC modes if available, or the BIOS fixed mode otherwise.
 */
static int intel_lvds_get_modes(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;

	ret = intel_ddc_get_modes(output);

	if (ret)
		return ret;

	/* Didn't get an EDID, so
	 * Set wide sync ranges so we get all modes
	 * handed to valid_mode for checking
	 */
	output->display_info.min_vfreq = 0;
	output->display_info.max_vfreq = 200;
	output->display_info.min_hfreq = 0;
	output->display_info.max_hfreq = 200;

	if (dev_priv->panel_fixed_mode != NULL) {
		struct drm_display_mode *mode =
			drm_mode_duplicate(dev, dev_priv->panel_fixed_mode);
		drm_mode_probed_add(output, mode);
		return 1;
	}

	return 0;
}

/**
 * intel_lvds_destroy - unregister and free LVDS structures
 * @output: output to free
 *
 * Unregister the DDC bus for this output then free the driver private
 * structure.
 */
static void intel_lvds_destroy(struct drm_output *output)
{
	struct intel_output *intel_output = output->driver_private;

	intel_i2c_destroy(intel_output->ddc_bus);
	kfree(output->driver_private);
}

static const struct drm_output_funcs intel_lvds_output_funcs = {
	.dpms = intel_lvds_dpms,
	.save = intel_lvds_save,
	.restore = intel_lvds_restore,
	.mode_valid = intel_lvds_mode_valid,
	.mode_fixup = intel_lvds_mode_fixup,
	.prepare = intel_lvds_prepare,
	.mode_set = intel_lvds_mode_set,
	.commit = intel_lvds_commit,
	.detect = intel_lvds_detect,
	.get_modes = intel_lvds_get_modes,
	.cleanup = intel_lvds_destroy
};

/**
 * intel_lvds_init - setup LVDS outputs on this device
 * @dev: drm device
 *
 * Create the output, register the LVDS DDC bus, and try to figure out what
 * modes we can display on the LVDS panel (if present).
 */
void intel_lvds_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_output *output;
	struct intel_output *intel_output;
	struct drm_display_mode *scan; /* *modes, *bios_mode; */
	struct drm_crtc *crtc;
	u32 lvds;
	int pipe;

	output = drm_output_create(dev, &intel_lvds_output_funcs,
				   DRM_MODE_OUTPUT_LVDS);
	if (!output)
		return;

	intel_output = kmalloc(sizeof(struct intel_output), GFP_KERNEL);
	if (!intel_output) {
		drm_output_destroy(output);
		return;
	}

	intel_output->type = INTEL_OUTPUT_LVDS;
	output->driver_private = intel_output;
	output->display_info.subpixel_order = SubPixelHorizontalRGB;
	output->interlace_allowed = FALSE;
	output->doublescan_allowed = FALSE;

	/* Set up the DDC bus. */
	intel_output->ddc_bus = intel_i2c_create(dev, GPIOC, "LVDSDDC_C");
	if (!intel_output->ddc_bus) {
		dev_printk(KERN_ERR, &dev->pdev->dev, "DDC bus registration "
			   "failed.\n");
		return;
	}

	/*
	 * Attempt to get the fixed panel mode from DDC.  Assume that the
	 * preferred mode is the right one.
	 */
	intel_ddc_get_modes(output);

	list_for_each_entry(scan, &output->probed_modes, head) {
		if (scan->type & DRM_MODE_TYPE_PREFERRED) {
			dev_priv->panel_fixed_mode = 
				drm_mode_duplicate(dev, scan);
			goto out; /* FIXME: check for quirks */
		}
	}

	/*
	 * If we didn't get EDID, try checking if the panel is already turned
	 * on.  If so, assume that whatever is currently programmed is the
	 * correct mode.
	 */
	lvds = I915_READ(LVDS);
	pipe = (lvds & LVDS_PIPEB_SELECT) ? 1 : 0;
	crtc = intel_get_crtc_from_pipe(dev, pipe);
		
	if (crtc && (lvds & LVDS_PORT_EN)) {
		dev_priv->panel_fixed_mode = intel_crtc_mode_get(dev, crtc);
		if (dev_priv->panel_fixed_mode) {
			dev_priv->panel_fixed_mode->type |=
				DRM_MODE_TYPE_PREFERRED;
			goto out; /* FIXME: check for quirks */
		}
	}

	/* If we still don't have a mode after all that, give up. */
	if (!dev_priv->panel_fixed_mode)
		goto failed;

	/* FIXME: probe the BIOS for modes and check for LVDS quirks */
#if 0
	/* Get the LVDS fixed mode out of the BIOS.  We should support LVDS
	 * with the BIOS being unavailable or broken, but lack the
	 * configuration options for now.
	 */
	bios_mode = intel_bios_get_panel_mode(pScrn);
	if (bios_mode != NULL) {
		if (dev_priv->panel_fixed_mode != NULL) {
			if (dev_priv->debug_modes &&
			    !xf86ModesEqual(dev_priv->panel_fixed_mode,
					    bios_mode))
			{
				xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
					   "BIOS panel mode data doesn't match probed data, "
					   "continuing with probed.\n");
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIOS mode:\n");
				xf86PrintModeline(pScrn->scrnIndex, bios_mode);
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "probed mode:\n");
				xf86PrintModeline(pScrn->scrnIndex, dev_priv->panel_fixed_mode);
				xfree(bios_mode->name);
				xfree(bios_mode);
			}
		}  else {
			dev_priv->panel_fixed_mode = bios_mode;
		}
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Couldn't detect panel mode.  Disabling panel\n");
		goto disable_exit;
	}

	/*
	 * Blacklist machines with BIOSes that list an LVDS panel without
	 * actually having one.
	 */
	if (dev_priv->PciInfo->chipType == PCI_CHIP_I945_GM) {
		/* aopen mini pc */
		if (dev_priv->PciInfo->subsysVendor == 0xa0a0)
			goto disable_exit;

		if ((dev_priv->PciInfo->subsysVendor == 0x8086) &&
		    (dev_priv->PciInfo->subsysCard == 0x7270)) {
			/* It's a Mac Mini or Macbook Pro.
			 *
			 * Apple hardware is out to get us.  The macbook pro
			 * has a real LVDS panel, but the mac mini does not,
			 * and they have the same device IDs.  We'll
			 * distinguish by panel size, on the assumption
			 * that Apple isn't about to make any machines with an
			 * 800x600 display.
			 */

			if (dev_priv->panel_fixed_mode != NULL &&
			    dev_priv->panel_fixed_mode->HDisplay == 800 &&
			    dev_priv->panel_fixed_mode->VDisplay == 600)
			{
				xf86DrvMsg(pScrn->scrnIndex, X_INFO,
					   "Suspected Mac Mini, ignoring the LVDS\n");
				goto disable_exit;
			}
		}
	}

#endif

out:
	drm_sysfs_output_add(output);
	drm_output_attach_property(output, dev->mode_config.connector_type_property, ConnectorLVDS);
	return;

failed:
        DRM_DEBUG("No LVDS modes found, disabling.\n");
	drm_output_destroy(output); /* calls intel_lvds_destroy above */
}
