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
#include "drm_crtc_helper.h"
#include "radeon_drm.h"
#include "radeon_drv.h"


void radeon_restore_dac_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (radeon_is_r300(dev_priv))
		RADEON_WRITE_P(RADEON_GPIOPAD_A, state->gpiopad_a, ~1);

	RADEON_WRITE_P(RADEON_DAC_CNTL,
		       state->dac_cntl,
		       RADEON_DAC_RANGE_CNTL |
		       RADEON_DAC_BLANKING);

	RADEON_WRITE(RADEON_DAC_CNTL2, state->dac2_cntl);

	if ((dev_priv->chip_family != CHIP_R100) &&
	    (dev_priv->chip_family != CHIP_R200))
		RADEON_WRITE (RADEON_TV_DAC_CNTL, state->tv_dac_cntl);

	RADEON_WRITE(RADEON_DISP_OUTPUT_CNTL, state->disp_output_cntl);
	
	if ((dev_priv->chip_family == CHIP_R200) ||
	    radeon_is_r300(dev_priv)) {
		RADEON_WRITE(RADEON_DISP_TV_OUT_CNTL, state->disp_tv_out_cntl);
	} else {
		RADEON_WRITE(RADEON_DISP_HW_DEBUG, state->disp_hw_debug);
	}

	RADEON_WRITE(RADEON_DAC_MACRO_CNTL, state->dac_macro_cntl);

	/* R200 DAC connected via DVO */
	if (dev_priv->chip_family == CHIP_R200)
		RADEON_WRITE(RADEON_FP2_GEN_CNTL, state->fp2_gen_cntl);
}


/* Write TMDS registers */
void radeon_restore_fp_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	RADEON_WRITE(RADEON_TMDS_PLL_CNTL,        state->tmds_pll_cntl);
	RADEON_WRITE(RADEON_TMDS_TRANSMITTER_CNTL,state->tmds_transmitter_cntl);
	RADEON_WRITE(RADEON_FP_GEN_CNTL,          state->fp_gen_cntl);

	if ((dev_priv->chip_family == CHIP_RS400) ||
	    (dev_priv->chip_family == CHIP_RS480)) {
		RADEON_WRITE(RS400_FP_2ND_GEN_CNTL, state->fp_2nd_gen_cntl);
		/*RADEON_WRITE(RS400_TMDS2_CNTL, state->tmds2_cntl);*/
		RADEON_WRITE(RS400_TMDS2_TRANSMITTER_CNTL, state->tmds2_transmitter_cntl);
	}

	/* old AIW Radeon has some BIOS initialization problem
	 * with display buffer underflow, only occurs to DFP
	 */
	if (dev_priv->flags & RADEON_SINGLE_CRTC)
		RADEON_WRITE(RADEON_GRPH_BUFFER_CNTL,
			     RADEON_READ(RADEON_GRPH_BUFFER_CNTL) & ~0x7f0000);

}

/* Write FP2 registers */
void radeon_restore_fp2_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	RADEON_WRITE(RADEON_FP2_GEN_CNTL, state->fp2_gen_cntl);

	if ((dev_priv->chip_family == CHIP_RS400) ||
	    (dev_priv->chip_family == CHIP_RS480))
		RADEON_WRITE(RS400_FP2_2_GEN_CNTL, state->fp2_2_gen_cntl);
}

/* Write RMX registers */
void radeon_state_rmx_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	RADEON_WRITE(RADEON_FP_HORZ_STRETCH,      state->fp_horz_stretch);
	RADEON_WRITE(RADEON_FP_VERT_STRETCH,      state->fp_vert_stretch);
	RADEON_WRITE(RADEON_CRTC_MORE_CNTL,       state->crtc_more_cntl);
	RADEON_WRITE(RADEON_FP_HORZ_VERT_ACTIVE,  state->fp_horz_vert_active);
	RADEON_WRITE(RADEON_FP_H_SYNC_STRT_WID,   state->fp_h_sync_strt_wid);
	RADEON_WRITE(RADEON_FP_V_SYNC_STRT_WID,   state->fp_v_sync_strt_wid);
	RADEON_WRITE(RADEON_FP_CRTC_H_TOTAL_DISP, state->fp_crtc_h_total_disp);
	RADEON_WRITE(RADEON_FP_CRTC_V_TOTAL_DISP, state->fp_crtc_v_total_disp);

}

/* Write LVDS registers */
void radeon_restore_lvds_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (dev_priv->flags & RADEON_IS_MOBILITY) {
		RADEON_WRITE(RADEON_LVDS_GEN_CNTL,  state->lvds_gen_cntl);
		/*RADEON_WRITE(RADEON_LVDS_PLL_CNTL,  state->lvds_pll_cntl);*/

		if (dev_priv->chip_family == CHIP_RV410) {
			RADEON_WRITE(RADEON_CLOCK_CNTL_INDEX, 0);
		}
	}

}

static void radeon_init_fp_registers(struct drm_encoder *encoder, struct drm_display_mode *mode,
				     bool is_primary)
{
	

}

static void radeon_init_dac_registers(struct drm_encoder *encoder, struct radeon_legacy_state *state,
				      struct drm_display_mode *mode, bool is_primary)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (is_primary) {
		if (dev_priv->chip_family == CHIP_R200 || radeon_is_r300(dev_priv)) {
			state->disp_output_cntl = RADEON_READ(RADEON_DISP_OUTPUT_CNTL &
							      ~RADEON_DISP_DAC_SOURCE_MASK);
		} else {
			state->dac2_cntl = RADEON_READ(RADEON_DAC_CNTL2)  & ~(RADEON_DAC2_DAC_CLK_SEL);
		}
	} else {
		if (dev_priv->chip_family == CHIP_R200 || radeon_is_r300(dev_priv)) {
			state->disp_output_cntl = RADEON_READ(RADEON_DISP_OUTPUT_CNTL &
							      ~RADEON_DISP_DAC_SOURCE_MASK);
			state->disp_output_cntl |= RADEON_DISP_DAC_SOURCE_CRTC2;
		} else {
			state->dac2_cntl = RADEON_READ(RADEON_DAC_CNTL2) | RADEON_DAC2_DAC_CLK_SEL;
		}
	}

	state->dac_cntl = (RADEON_DAC_MASK_ALL |
			   RADEON_DAC_VGA_ADR_EN |
			   /* TODO 6-bits */
			   RADEON_DAC_8BIT_EN);
	state->dac_macro_cntl = RADEON_READ(RADEON_DAC_MACRO_CNTL);
}

static void radeon_legacy_lvds_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
}

static void radeon_legacy_lvds_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_lvds_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_lvds_commit(struct drm_encoder *encoder)
{
	radeon_legacy_lvds_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_lvds_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{


}

static bool radeon_legacy_lvds_mode_fixup(struct drm_encoder *encoder,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

	radeon_encoder->flags &= ~RADEON_USE_RMX;

	if (radeon_encoder->rmx_type != RMX_OFF)
		radeon_rmx_mode_fixup(encoder, mode, adjusted_mode);
	
	return true;
}

static const struct drm_encoder_helper_funcs radeon_legacy_lvds_helper_funcs = {
	.dpms = radeon_legacy_lvds_dpms,
	.mode_fixup = radeon_legacy_lvds_mode_fixup,
	.prepare = radeon_legacy_lvds_prepare,
	.mode_set = radeon_legacy_lvds_mode_set,
	.commit = radeon_legacy_lvds_commit,
};


static const struct drm_encoder_funcs radeon_legacy_lvds_enc_funcs = {
	.destroy = radeon_enc_destroy,
};


struct drm_encoder *radeon_encoder_legacy_lvds_add(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;
	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_lvds_enc_funcs,
			 DRM_MODE_ENCODER_LVDS);

	drm_encoder_helper_add(encoder, &radeon_legacy_lvds_helper_funcs);

	/* TODO get the LVDS info from the BIOS for panel size etc. */
	/* get the lvds info from the bios */
	radeon_combios_get_lvds_info(radeon_encoder);

	/* LVDS gets default RMX full scaling */
	radeon_encoder->rmx_type = RMX_FULL;

	return encoder;
}

static bool radeon_legacy_primary_dac_mode_fixup(struct drm_encoder *encoder,
						 struct drm_display_mode *mode,
						 struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_legacy_primary_dac_dpms(struct drm_encoder *encoder, int mode)
{


}

static void radeon_legacy_primary_dac_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_primary_dac_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_primary_dac_commit(struct drm_encoder *encoder)
{
	radeon_legacy_primary_dac_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_primary_dac_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	radeon_init_dac_registers(encoder, &dev_priv->mode_info.legacy_state, adjusted_mode,
				  (radeon_crtc->crtc_id == 1));

	radeon_restore_dac_registers(dev, &dev_priv->mode_info.legacy_state);
}

static const struct drm_encoder_helper_funcs radeon_legacy_primary_dac_helper_funcs = {
	.dpms = radeon_legacy_primary_dac_dpms,
	.mode_fixup = radeon_legacy_primary_dac_mode_fixup,
	.prepare = radeon_legacy_primary_dac_prepare,
	.mode_set = radeon_legacy_primary_dac_mode_set,
	.commit = radeon_legacy_primary_dac_commit,
};


static const struct drm_encoder_funcs radeon_legacy_primary_dac_enc_funcs = {
	.destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_legacy_primary_dac_add(struct drm_device *dev, int bios_index, int has_tv)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;
	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_primary_dac_enc_funcs,
			 DRM_MODE_ENCODER_DAC);

	drm_encoder_helper_add(encoder, &radeon_legacy_primary_dac_helper_funcs);

	return encoder;
}


static bool radeon_legacy_tmds_int_mode_fixup(struct drm_encoder *encoder,
						 struct drm_display_mode *mode,
						 struct drm_display_mode *adjusted_mode)
{

}

static void radeon_legacy_tmds_int_dpms(struct drm_encoder *encoder, int mode)
{
	/* dfp1 */

}

static void radeon_legacy_tmds_int_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_tmds_int_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_tmds_int_commit(struct drm_encoder *encoder)
{
	radeon_legacy_tmds_int_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_tmds_int_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	

}

static const struct drm_encoder_helper_funcs radeon_legacy_tmds_int_helper_funcs = {
	.dpms = radeon_legacy_tmds_int_dpms,
	.mode_fixup = radeon_legacy_tmds_int_mode_fixup,
	.prepare = radeon_legacy_tmds_int_prepare,
	.mode_set = radeon_legacy_tmds_int_mode_set,
	.commit = radeon_legacy_tmds_int_commit,
};


static const struct drm_encoder_funcs radeon_legacy_tmds_int_enc_funcs = {
	.destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_legacy_tmds_int_add(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;
	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_tmds_int_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);

	drm_encoder_helper_add(encoder, &radeon_legacy_tmds_int_helper_funcs);

	radeon_combios_get_tmds_info(radeon_encoder);
	return encoder;
}
