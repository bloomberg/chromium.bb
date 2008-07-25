/*
 * Copyright 2007-8 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 */
#include "drmP.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

#include "drm_crtc_helper.h"
#include "atom.h"
#include "atom-bits.h"

static void atombios_enable_crtc(struct drm_crtc *crtc, int state)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int index = GetIndexIntoMasterTable(COMMAND, EnableCRTC);
	ENABLE_CRTC_PS_ALLOCATION args;

	memset(&args, 0, sizeof(args));

	args.ucCRTC = radeon_crtc->crtc_id;
	args.ucEnable = state;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

static void atombios_enable_crtc_memreq(struct drm_crtc *crtc, int state)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int index = GetIndexIntoMasterTable(COMMAND, EnableCRTCMemReq);
	ENABLE_CRTC_PS_ALLOCATION args;

	memset(&args, 0, sizeof(args));

	args.ucCRTC = radeon_crtc->crtc_id;
	args.ucEnable = state;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

static void atombios_blank_crtc(struct drm_crtc *crtc, int state)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int index = GetIndexIntoMasterTable(COMMAND, BlankCRTC);
	BLANK_CRTC_PS_ALLOCATION args;

	memset(&args, 0, sizeof(args));

	args.ucCRTC = radeon_crtc->crtc_id;
	args.ucBlanking = state;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

void atombios_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;

	switch(mode) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
		if (radeon_is_dce3(dev_priv))
			atombios_enable_crtc_memreq(crtc, 1);
		atombios_enable_crtc(crtc, 1);
		atombios_blank_crtc(crtc, 0);

		radeon_crtc_load_lut(crtc);
		break;
	case DRM_MODE_DPMS_OFF:
		atombios_blank_crtc(crtc, 1);
		atombios_enable_crtc(crtc, 0);
		if (radeon_is_dce3(dev_priv))
			atombios_enable_crtc_memreq(crtc, 0);
		break;
	}
}


void atombios_crtc_set_timing(struct drm_crtc *crtc, SET_CRTC_TIMING_PARAMETERS_PS_ALLOCATION *crtc_param)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	SET_CRTC_TIMING_PARAMETERS_PS_ALLOCATION conv_param;
	int index = GetIndexIntoMasterTable(COMMAND, SetCRTC_Timing);

	conv_param.usH_Total                = cpu_to_le16(crtc_param->usH_Total);
	conv_param.usH_Disp                 = cpu_to_le16(crtc_param->usH_Disp);
	conv_param.usH_SyncStart            = cpu_to_le16(crtc_param->usH_SyncStart);
	conv_param.usH_SyncWidth            = cpu_to_le16(crtc_param->usH_SyncWidth);
	conv_param.usV_Total                = cpu_to_le16(crtc_param->usV_Total);
	conv_param.usV_Disp                 = cpu_to_le16(crtc_param->usV_Disp);
	conv_param.usV_SyncStart            = cpu_to_le16(crtc_param->usV_SyncStart);
	conv_param.usV_SyncWidth            = cpu_to_le16(crtc_param->usV_SyncWidth);
	conv_param.susModeMiscInfo.usAccess = cpu_to_le16(crtc_param->susModeMiscInfo.usAccess);
	conv_param.ucCRTC                   = crtc_param->ucCRTC;
	conv_param.ucOverscanRight          = crtc_param->ucOverscanRight;
	conv_param.ucOverscanLeft           = crtc_param->ucOverscanLeft;
	conv_param.ucOverscanBottom         = crtc_param->ucOverscanBottom;
	conv_param.ucOverscanTop            = crtc_param->ucOverscanTop; 
	conv_param.ucReserved               = crtc_param->ucReserved;

	printk("executing set crtc timing\n");
	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&conv_param);
}

void atombios_crtc_set_pll(struct drm_crtc *crtc, struct drm_display_mode *mode,
			   int pll_flags)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint8_t frev, crev;
	int index = GetIndexIntoMasterTable(COMMAND, SetPixelClock);
	SET_PIXEL_CLOCK_PS_ALLOCATION spc_param;
	PIXEL_CLOCK_PARAMETERS_V2 *spc2_ptr;
	PIXEL_CLOCK_PARAMETERS_V3 *spc3_ptr;
	uint32_t sclock = mode->clock;
	uint32_t ref_div = 0, fb_div = 0, post_div = 0;

	memset(&spc_param, 0, sizeof(SET_PIXEL_CLOCK_PS_ALLOCATION));

	if (radeon_is_avivo(dev_priv)) {
		uint32_t temp;

		pll_flags |= RADEON_PLL_PREFER_LOW_REF_DIV;

		radeon_compute_pll(&dev_priv->mode_info.pll, mode->clock,
				   &temp, &fb_div, &ref_div, &post_div, pll_flags);
		sclock = temp;

		if (radeon_crtc->crtc_id == 0) {
			temp = RADEON_READ(AVIVO_P1PLL_INT_SS_CNTL);
			RADEON_WRITE(AVIVO_P1PLL_INT_SS_CNTL, temp & ~1);
		} else {
			temp = RADEON_READ(AVIVO_P2PLL_INT_SS_CNTL);
			RADEON_WRITE(AVIVO_P2PLL_INT_SS_CNTL, temp & ~1);
		}
	} else {
#if 0 // TODO r400
		sclock = save->dot_clock_freq;
		fb_div = save->feedback_div;
		post_div = save->post_div;
		ref_div = save->ppll_ref_div;
#endif
	}

	/* */

	atom_parse_cmd_header(dev_priv->mode_info.atom_context, index, &frev, &crev);

	switch(frev) {
	case 1:
		switch(crev) {
		case 1:
		case 2:
			spc2_ptr = (PIXEL_CLOCK_PARAMETERS_V2*)&spc_param.sPCLKInput;
			spc2_ptr->usPixelClock = cpu_to_le16(sclock);
			spc2_ptr->usRefDiv = cpu_to_le16(ref_div);
			spc2_ptr->usFbDiv = cpu_to_le16(fb_div);
			spc2_ptr->ucPostDiv = post_div;
			spc2_ptr->ucPpll = radeon_crtc->crtc_id ? ATOM_PPLL2 : ATOM_PPLL1;
			spc2_ptr->ucCRTC = radeon_crtc->crtc_id;
			spc2_ptr->ucRefDivSrc = 1;
			break;
		case 3:
			spc3_ptr = (PIXEL_CLOCK_PARAMETERS_V3*)&spc_param.sPCLKInput;
			spc3_ptr->usPixelClock = cpu_to_le16(sclock);
			spc3_ptr->usRefDiv = cpu_to_le16(ref_div);
			spc3_ptr->usFbDiv = cpu_to_le16(fb_div);
			spc3_ptr->ucPostDiv = post_div;
			spc3_ptr->ucPpll = radeon_crtc->crtc_id ? ATOM_PPLL2 : ATOM_PPLL1;
			spc3_ptr->ucMiscInfo = (radeon_crtc->crtc_id << 2);
			
			/* TODO insert output encoder object stuff herre for r600 */
			break;
		default:
			DRM_ERROR("Unknown table version %d %d\n", frev, crev);
			return;
		}
		break;
	default:
		DRM_ERROR("Unknown table version %d %d\n", frev, crev);
		return;
	}

	printk("executing set pll\n");
	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&spc_param);
}

void atombios_crtc_set_base(struct drm_crtc *crtc, int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);	
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_framebuffer *radeon_fb;
	struct drm_radeon_gem_object *obj_priv;
	uint32_t fb_location, fb_format, fb_pitch_pixels;

	if (!crtc->fb)
		return;

	radeon_fb = to_radeon_framebuffer(crtc->fb);

	obj_priv = radeon_fb->obj->driver_private;

	fb_location = obj_priv->bo->offset + dev_priv->fb_location;

	switch(crtc->fb->bits_per_pixel) {
	case 15:
		fb_format = AVIVO_D1GRPH_CONTROL_DEPTH_16BPP | AVIVO_D1GRPH_CONTROL_16BPP_ARGB1555;
		break;
	case 16:
		fb_format = AVIVO_D1GRPH_CONTROL_DEPTH_16BPP | AVIVO_D1GRPH_CONTROL_16BPP_RGB565;
		break;
	case 24:
	case 32:
		fb_format = AVIVO_D1GRPH_CONTROL_DEPTH_32BPP | AVIVO_D1GRPH_CONTROL_32BPP_ARGB8888;
		break;
	default:
		DRM_ERROR("Unsupported screen depth %d\n", crtc->fb->bits_per_pixel);
		return;
	}
	
	/* TODO tiling */
	if (radeon_crtc->crtc_id == 0)
		RADEON_WRITE(AVIVO_D1VGA_CONTROL, 0);
	else
		RADEON_WRITE(AVIVO_D2VGA_CONTROL, 0);
	
	RADEON_WRITE(AVIVO_D1GRPH_UPDATE + radeon_crtc->crtc_offset, AVIVO_D1GRPH_UPDATE_LOCK);
	
	RADEON_WRITE(AVIVO_D1GRPH_PRIMARY_SURFACE_ADDRESS + radeon_crtc->crtc_offset, fb_location);
	RADEON_WRITE(AVIVO_D1GRPH_SECONDARY_SURFACE_ADDRESS + radeon_crtc->crtc_offset, fb_location);
	RADEON_WRITE(AVIVO_D1GRPH_CONTROL + radeon_crtc->crtc_offset, fb_format);
	
	RADEON_WRITE(AVIVO_D1GRPH_SURFACE_OFFSET_X + radeon_crtc->crtc_offset, 0);
	RADEON_WRITE(AVIVO_D1GRPH_SURFACE_OFFSET_Y + radeon_crtc->crtc_offset, 0);
	RADEON_WRITE(AVIVO_D1GRPH_X_START + radeon_crtc->crtc_offset, x);
	RADEON_WRITE(AVIVO_D1GRPH_Y_START + radeon_crtc->crtc_offset, y);
	RADEON_WRITE(AVIVO_D1GRPH_X_END + radeon_crtc->crtc_offset, x + crtc->mode.hdisplay);
	RADEON_WRITE(AVIVO_D1GRPH_Y_END + radeon_crtc->crtc_offset, y + crtc->mode.vdisplay);

	fb_pitch_pixels = crtc->fb->pitch / (crtc->fb->bits_per_pixel / 8);
	RADEON_WRITE(AVIVO_D1GRPH_PITCH + radeon_crtc->crtc_offset, fb_pitch_pixels);
	RADEON_WRITE(AVIVO_D1GRPH_ENABLE + radeon_crtc->crtc_offset, 1);
	
	/* unlock the grph regs */
	RADEON_WRITE(AVIVO_D1GRPH_UPDATE + radeon_crtc->crtc_offset, 0);
	
	/* lock the mode regs */
	RADEON_WRITE(AVIVO_D1SCL_UPDATE + radeon_crtc->crtc_offset, AVIVO_D1SCL_UPDATE_LOCK);
	
	RADEON_WRITE(AVIVO_D1MODE_DESKTOP_HEIGHT + radeon_crtc->crtc_offset,
		     crtc->mode.vdisplay);
	RADEON_WRITE(AVIVO_D1MODE_VIEWPORT_START + radeon_crtc->crtc_offset, (x << 16) | y);
	RADEON_WRITE(AVIVO_D1MODE_VIEWPORT_SIZE + radeon_crtc->crtc_offset,
		     (crtc->mode.hdisplay << 16) | crtc->mode.vdisplay);
	/* unlock the mode regs */
	RADEON_WRITE(AVIVO_D1SCL_UPDATE + radeon_crtc->crtc_offset, 0);
}

void atombios_crtc_mode_set(struct drm_crtc *crtc,
			    struct drm_display_mode *mode,
			    struct drm_display_mode *adjusted_mode,
			    int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_encoder *encoder;
	SET_CRTC_TIMING_PARAMETERS_PS_ALLOCATION crtc_timing;
	int pll_flags = 0;
	/* TODO color tiling */

	memset(&crtc_timing, 0, sizeof(crtc_timing));

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		
		

	}

	crtc_timing.ucCRTC = radeon_crtc->crtc_id;
	crtc_timing.usH_Total = adjusted_mode->crtc_htotal;
	crtc_timing.usH_Disp = adjusted_mode->crtc_hdisplay;
	crtc_timing.usH_SyncStart = adjusted_mode->crtc_hsync_start;
	crtc_timing.usH_SyncWidth = adjusted_mode->crtc_hsync_end - adjusted_mode->crtc_hsync_start;

	crtc_timing.usV_Total = adjusted_mode->crtc_vtotal;
	crtc_timing.usV_Disp = adjusted_mode->crtc_vdisplay;
	crtc_timing.usV_SyncStart = adjusted_mode->crtc_vsync_start;
	crtc_timing.usV_SyncWidth = adjusted_mode->crtc_vsync_end - adjusted_mode->crtc_vsync_start;

	if (adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC)
		crtc_timing.susModeMiscInfo.usAccess |= ATOM_VSYNC_POLARITY;
	
	if (adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC)
		crtc_timing.susModeMiscInfo.usAccess |= ATOM_HSYNC_POLARITY;

	if (adjusted_mode->flags & DRM_MODE_FLAG_CSYNC)
		crtc_timing.susModeMiscInfo.usAccess |= ATOM_COMPOSITESYNC;

	if (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE)
		crtc_timing.susModeMiscInfo.usAccess |= ATOM_INTERLACE;

	if (adjusted_mode->flags & DRM_MODE_FLAG_DBLSCAN)
		crtc_timing.susModeMiscInfo.usAccess |= ATOM_DOUBLE_CLOCK_MODE;

	if (radeon_is_avivo(dev_priv)) {
		atombios_crtc_set_base(crtc, x, y);
	}

	atombios_crtc_set_pll(crtc, adjusted_mode, pll_flags);

	atombios_crtc_set_timing(crtc, &crtc_timing);
}

static bool atombios_crtc_mode_fixup(struct drm_crtc *crtc,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	return true;
}


static void atombios_crtc_prepare(struct drm_crtc *crtc)
{
	atombios_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
}

static void atombios_crtc_commit(struct drm_crtc *crtc)
{
	atombios_crtc_dpms(crtc, DRM_MODE_DPMS_ON);
}

static const struct drm_crtc_helper_funcs atombios_helper_funcs = {
	.dpms = atombios_crtc_dpms,
	.mode_fixup = atombios_crtc_mode_fixup,
	.mode_set = atombios_crtc_mode_set,
	.mode_set_base = atombios_crtc_set_base,
	.prepare = atombios_crtc_prepare,
	.commit = atombios_crtc_commit,
};

void radeon_atombios_init_crtc(struct drm_device *dev,
			       struct radeon_crtc *radeon_crtc)
{
	if (radeon_crtc->crtc_id == 1)
		radeon_crtc->crtc_offset = AVIVO_D2CRTC_H_TOTAL - AVIVO_D1CRTC_H_TOTAL;
	drm_crtc_helper_add(&radeon_crtc->base, &atombios_helper_funcs);
}
