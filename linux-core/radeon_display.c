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

#include "atom.h"
#include <asm/div64.h>

#include "drm_crtc_helper.h"

#define CURSOR_WIDTH 64
#define CURSOR_HEIGHT 64

int radeon_ddc_dump(struct drm_connector *connector);



static void avivo_crtc_load_lut(struct drm_crtc *crtc)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i;

	DRM_DEBUG("%d\n", radeon_crtc->crtc_id);
	RADEON_WRITE(AVIVO_DC_LUTA_CONTROL + radeon_crtc->crtc_offset, 0);

	RADEON_WRITE(AVIVO_DC_LUTA_BLACK_OFFSET_BLUE + radeon_crtc->crtc_offset, 0);
	RADEON_WRITE(AVIVO_DC_LUTA_BLACK_OFFSET_GREEN + radeon_crtc->crtc_offset, 0);
	RADEON_WRITE(AVIVO_DC_LUTA_BLACK_OFFSET_RED + radeon_crtc->crtc_offset, 0);

	RADEON_WRITE(AVIVO_DC_LUTA_WHITE_OFFSET_BLUE + radeon_crtc->crtc_offset, 0xffff);
	RADEON_WRITE(AVIVO_DC_LUTA_WHITE_OFFSET_GREEN + radeon_crtc->crtc_offset, 0xffff);
	RADEON_WRITE(AVIVO_DC_LUTA_WHITE_OFFSET_RED + radeon_crtc->crtc_offset, 0xffff);

	RADEON_WRITE(AVIVO_DC_LUT_RW_SELECT, radeon_crtc->crtc_id);
	RADEON_WRITE(AVIVO_DC_LUT_RW_MODE, 0);
	RADEON_WRITE(AVIVO_DC_LUT_WRITE_EN_MASK, 0x0000003f);

	for (i = 0; i < 256; i++) {
		RADEON_WRITE8(AVIVO_DC_LUT_RW_INDEX, i);
		RADEON_WRITE(AVIVO_DC_LUT_30_COLOR,
			     (radeon_crtc->lut_r[i] << 22) |
			     (radeon_crtc->lut_g[i] << 12) |
			     (radeon_crtc->lut_b[i] << 2));
	}

	RADEON_WRITE(AVIVO_D1GRPH_LUT_SEL + radeon_crtc->crtc_offset, radeon_crtc->crtc_id);
}


void radeon_crtc_load_lut(struct drm_crtc *crtc)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	u32 temp;
	int i;
	if (!crtc->enabled)
		return;

	if (radeon_is_avivo(dev_priv)) {
		avivo_crtc_load_lut(crtc);
		return;
	}

	temp = RADEON_READ(RADEON_DAC_CNTL2);
	if (radeon_crtc->crtc_id == 0)
		temp &= (uint32_t)~RADEON_DAC2_PALETTE_ACC_CTL;
	else
		temp |= RADEON_DAC2_PALETTE_ACC_CTL;
	RADEON_WRITE(RADEON_DAC_CNTL2, temp);

	for (i = 0; i < 256; i++) {
//		OUTPAL(i, radeon_crtc->lut_r[i], radeon_crtc->lut_g[i], radeon_crtc->lut_b[i]);
	}

}

/** Sets the color ramps on behalf of RandR */
void radeon_crtc_fb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green,
			      u16 blue, int regno)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);

	if (regno==0)
		DRM_DEBUG("gamma set %d\n", radeon_crtc->crtc_id);
	radeon_crtc->lut_r[regno] = red >> 8;
	radeon_crtc->lut_g[regno] = green >> 8;
	radeon_crtc->lut_b[regno] = blue >> 8;
}



static void avivo_lock_cursor(struct drm_crtc *crtc, bool lock)
{
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);

	uint32_t tmp;

	tmp = RADEON_READ(AVIVO_D1CUR_UPDATE + radeon_crtc->crtc_offset);
	if (lock)
		tmp |= AVIVO_D1CURSOR_UPDATE_LOCK;
	else
		tmp &= ~AVIVO_D1CURSOR_UPDATE_LOCK;

	RADEON_WRITE(AVIVO_D1CUR_UPDATE + radeon_crtc->crtc_offset, tmp);
}

static int radeon_crtc_cursor_set(struct drm_crtc *crtc,
				  struct drm_file *file_priv,
				  uint32_t handle,
				  uint32_t width,
				  uint32_t height)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;

	if (!handle) {
		RADEON_WRITE(AVIVO_D1CUR_CONTROL + radeon_crtc->crtc_offset, 0);
		return 0;
		/* turn off cursor */
	}

	obj = drm_gem_object_lookup(crtc->dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("Cannot find cursor object %x for crtc %d\n", handle, radeon_crtc->crtc_id);
		return -EINVAL;
	}

	obj_priv = obj->driver_private;

	RADEON_WRITE(AVIVO_D1CUR_CONTROL + radeon_crtc->crtc_offset, 0);
	if (radeon_is_avivo(dev_priv)) {
		RADEON_WRITE(AVIVO_D1CUR_SURFACE_ADDRESS + radeon_crtc->crtc_offset,
			     dev_priv->fb_location + obj_priv->bo->offset);
		RADEON_WRITE(AVIVO_D1CUR_SIZE + radeon_crtc->crtc_offset,
			     (CURSOR_WIDTH - 1) << 16 | (CURSOR_HEIGHT - 1));
		RADEON_WRITE(AVIVO_D1CUR_CONTROL + radeon_crtc->crtc_offset,
			     AVIVO_D1CURSOR_EN | (AVIVO_D1CURSOR_MODE_24BPP << AVIVO_D1CURSOR_MODE_SHIFT));
	}

	mutex_lock(&crtc->dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&crtc->dev->struct_mutex);

	return 0;
}

static int radeon_crtc_cursor_move(struct drm_crtc *crtc,
				   int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	int xorigin = 0, yorigin = 0;

	if (x < 0) xorigin = -x+1;
	if (y < 0) yorigin = -x+1;
	if (xorigin >= CURSOR_WIDTH) xorigin = CURSOR_WIDTH - 1;
	if (yorigin >= CURSOR_WIDTH) yorigin = CURSOR_WIDTH - 1;

	if (crtc->mode.flags & DRM_MODE_FLAG_INTERLACE)
		y /= 2;
	else if (crtc->mode.flags & DRM_MODE_FLAG_DBLSCAN)
		y *= 2;

	if (radeon_is_avivo(dev_priv)) {
		avivo_lock_cursor(crtc, true);

		RADEON_WRITE(AVIVO_D1CUR_POSITION + radeon_crtc->crtc_offset,
			     ((xorigin ? 0: x) << 16) |
			     (yorigin ? 0 : y));
		RADEON_WRITE(AVIVO_D1CUR_HOT_SPOT + radeon_crtc->crtc_offset, (xorigin << 16) | yorigin);
		avivo_lock_cursor(crtc, false);
	}

	return 0;
}

static void radeon_crtc_gamma_set(struct drm_crtc *crtc, u16 *red, u16 *green,
				  u16 *blue, uint32_t size)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	int i;

	if (size != 256)
		return;

	for (i = 0; i < 256; i++) {
		radeon_crtc->lut_r[i] = red[i] >> 8;
		radeon_crtc->lut_g[i] = green[i] >> 8;
		radeon_crtc->lut_b[i] = blue[i] >> 8;
	}

	radeon_crtc_load_lut(crtc);
}

static void radeon_crtc_destroy(struct drm_crtc *crtc)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(radeon_crtc);
}

static const struct drm_crtc_funcs radeon_crtc_funcs = {
	.cursor_set = radeon_crtc_cursor_set,
	.cursor_move = radeon_crtc_cursor_move,
	.gamma_set = radeon_crtc_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = radeon_crtc_destroy,
};

static void radeon_crtc_init(struct drm_device *dev, int index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc;
	int i;

	radeon_crtc = kzalloc(sizeof(struct radeon_crtc) + (RADEONFB_CONN_LIMIT * sizeof(struct drm_connector *)), GFP_KERNEL);
	//	radeon_crtc = kzalloc(sizeof(struct radeon_crtc), GFP_KERNEL);
	if (radeon_crtc == NULL)
		return;

	drm_crtc_init(dev, &radeon_crtc->base, &radeon_crtc_funcs);

	drm_mode_crtc_set_gamma_size(&radeon_crtc->base, 256);
	radeon_crtc->crtc_id = index;

	radeon_crtc->mode_set.crtc = &radeon_crtc->base;
	radeon_crtc->mode_set.connectors = (struct drm_connector **)(radeon_crtc + 1);
	radeon_crtc->mode_set.num_connectors = 0;

	for (i = 0; i < 256; i++) {
		radeon_crtc->lut_r[i] = i;
		radeon_crtc->lut_g[i] = i;
		radeon_crtc->lut_b[i] = i;
	}

	if (dev_priv->is_atom_bios && dev_priv->chip_family > CHIP_RS690)
		radeon_atombios_init_crtc(dev, radeon_crtc);
	else
		radeon_legacy_init_crtc(dev, radeon_crtc);
}

bool radeon_legacy_setup_enc_conn(struct drm_device *dev)
{

	radeon_get_legacy_connector_info_from_bios(dev);
	return false;
}

bool radeon_setup_enc_conn(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	/* do all the mac and stuff */
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	int i;

	if (dev_priv->is_atom_bios)
		radeon_get_atom_connector_info_from_bios_connector_table(dev);
	else
		radeon_get_legacy_connector_info_from_bios(dev);

	for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
		if (!mode_info->bios_connector[i].valid)
			continue;

		/* add a connector for this */
		if (mode_info->bios_connector[i].connector_type == CONNECTOR_NONE)
			continue;

		connector = radeon_connector_add(dev, i);
		if (!connector)
			continue;

		encoder = NULL;
		/* if we find an LVDS connector */
		if (mode_info->bios_connector[i].connector_type == CONNECTOR_LVDS) {
			if (radeon_is_avivo(dev_priv))
				encoder = radeon_encoder_lvtma_add(dev, i);
			else
				encoder = radeon_encoder_legacy_lvds_add(dev, i);
			if (encoder)
				drm_mode_connector_attach_encoder(connector, encoder);
		}

		/* DAC on DVI or VGA */
		if ((mode_info->bios_connector[i].connector_type == CONNECTOR_DVI_I) ||
		    (mode_info->bios_connector[i].connector_type == CONNECTOR_DVI_A) ||
		    (mode_info->bios_connector[i].connector_type == CONNECTOR_VGA)) {
			if (radeon_is_avivo(dev_priv)) {
				encoder = radeon_encoder_atom_dac_add(dev, i, mode_info->bios_connector[i].dac_type, 0);
			} else {
				if (mode_info->bios_connector[i].dac_type == DAC_PRIMARY)
					encoder = radeon_encoder_legacy_primary_dac_add(dev, i, 0);
				else if (mode_info->bios_connector[i].dac_type == DAC_TVDAC)
					encoder = radeon_encoder_legacy_tv_dac_add(dev, i, 0);
			}
			if (encoder)
				drm_mode_connector_attach_encoder(connector, encoder);
		}

		/* TMDS on DVI */
		if ((mode_info->bios_connector[i].connector_type == CONNECTOR_DVI_I) ||
		    (mode_info->bios_connector[i].connector_type == CONNECTOR_DVI_D)) {
			if (radeon_is_avivo(dev_priv))
				encoder = radeon_encoder_atom_tmds_add(dev, i, mode_info->bios_connector[i].tmds_type);
			else {
				if (mode_info->bios_connector[i].tmds_type == TMDS_INT)
					encoder = radeon_encoder_legacy_tmds_int_add(dev, i);
				else if (mode_info->bios_connector[i].dac_type == TMDS_EXT)
					encoder = radeon_encoder_legacy_tmds_ext_add(dev, i);
			}
			if (encoder)
				drm_mode_connector_attach_encoder(connector, encoder);
		}

		/* TVDAC on DIN */
		if (mode_info->bios_connector[i].connector_type == CONNECTOR_DIN) {
			if (radeon_is_avivo(dev_priv))
				encoder = radeon_encoder_atom_dac_add(dev, i, mode_info->bios_connector[i].dac_type, 1);
			else {
				if (mode_info->bios_connector[i].dac_type == DAC_TVDAC)
					encoder = radeon_encoder_legacy_tv_dac_add(dev, i, 0);
			}
			if (encoder)
				drm_mode_connector_attach_encoder(connector, encoder);
		}
	}

	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		radeon_ddc_dump(connector);
	return true;
}

int radeon_ddc_get_modes(struct radeon_connector *radeon_connector)
{
	struct drm_radeon_private *dev_priv = radeon_connector->base.dev->dev_private;
	struct edid *edid;
	int ret = 0;

	if (!radeon_connector->ddc_bus)
		return -1;
	radeon_i2c_do_lock(radeon_connector, 1);
	edid = drm_get_edid(&radeon_connector->base, &radeon_connector->ddc_bus->adapter);
	radeon_i2c_do_lock(radeon_connector, 0);
	if (edid) {
		drm_mode_connector_update_edid_property(&radeon_connector->base, edid);
		ret = drm_add_edid_modes(&radeon_connector->base, edid);
		kfree(edid);
		return ret;
	}
	return -1;
}

int radeon_ddc_dump(struct drm_connector *connector)
{
	struct edid *edid;
	struct radeon_connector *radeon_connector = to_radeon_connector(connector);
	int ret = 0;

	if (!radeon_connector->ddc_bus)
		return -1;
	radeon_i2c_do_lock(radeon_connector, 1);
	edid = drm_get_edid(connector, &radeon_connector->ddc_bus->adapter);
	radeon_i2c_do_lock(radeon_connector, 0);
	if (edid) {
		kfree(edid);
	}
	return ret;
}

static inline uint32_t radeon_div(uint64_t n, uint32_t d)
{
	uint64_t x, y, result;
	uint64_t mod;

	n += d / 2;

	mod = do_div(n, d);
	return n;
}

void radeon_compute_pll(struct radeon_pll *pll,
			uint64_t freq,
			uint32_t *dot_clock_p,
			uint32_t *fb_div_p,
			uint32_t *ref_div_p,
			uint32_t *post_div_p,
			int flags)
{
	uint32_t min_ref_div = pll->min_ref_div;
	uint32_t max_ref_div = pll->max_ref_div;
	uint32_t best_vco = pll->best_vco;
	uint32_t best_post_div = 1;
	uint32_t best_ref_div = 1;
	uint32_t best_feedback_div = 1;
	uint32_t best_freq = -1;
	uint32_t best_error = 0xffffffff;
	uint32_t best_vco_diff = 1;
	uint32_t post_div;

	DRM_DEBUG("PLL freq %llu\n", freq);
	freq = freq * 1000;

	if (flags & RADEON_PLL_USE_REF_DIV)
		min_ref_div = max_ref_div = pll->reference_div;
	else {
		while (min_ref_div < max_ref_div-1) {
			uint32_t mid=(min_ref_div+max_ref_div)/2;
			uint32_t pll_in = pll->reference_freq / mid;
			if (pll_in < pll->pll_in_min)
				max_ref_div = mid;
			else if (pll_in > pll->pll_in_max)
				min_ref_div = mid;
			else
				break;
		}
	}

	for (post_div = pll->min_post_div; post_div <= pll->max_post_div; ++post_div) {
		uint32_t ref_div;

		if ((flags & RADEON_PLL_NO_ODD_POST_DIV) && (post_div & 1))
			continue;

		/* legacy radeons only have a few post_divs */
		if (flags & RADEON_PLL_LEGACY) {
			if ((post_div == 5) ||
			    (post_div == 7) ||
			    (post_div == 9) ||
			    (post_div == 10) ||
			    (post_div == 11))
				continue;
		}

		for (ref_div = min_ref_div; ref_div <= max_ref_div; ++ref_div) {
			uint32_t feedback_div, current_freq, error, vco_diff;
			uint32_t pll_in = pll->reference_freq / ref_div;
			uint32_t min_feed_div = pll->min_feedback_div;
			uint32_t max_feed_div = pll->max_feedback_div+1;

			if (pll_in < pll->pll_in_min || pll_in > pll->pll_in_max)
				continue;

			while (min_feed_div < max_feed_div) {
				uint32_t vco;
				feedback_div = (min_feed_div+max_feed_div)/2;

				vco = radeon_div((uint64_t)pll->reference_freq * feedback_div,
						 ref_div);

				if (vco < pll->pll_out_min) {
					min_feed_div = feedback_div+1;
					continue;
				} else if(vco > pll->pll_out_max) {
					max_feed_div = feedback_div;
					continue;
				}

				current_freq = radeon_div((uint64_t)pll->reference_freq * 10000 * feedback_div,
							  ref_div * post_div);

				error = abs(current_freq - freq);
				vco_diff = abs(vco - best_vco);

				if ((best_vco == 0 && error < best_error) ||
				    (best_vco != 0 &&
				     (error < best_error - 100 ||
				      (abs(error - best_error) < 100 && vco_diff < best_vco_diff )))) {
					best_post_div = post_div;
					best_ref_div = ref_div;
					best_feedback_div = feedback_div;
					best_freq = current_freq;
					best_error = error;
					best_vco_diff = vco_diff;
				} else if (current_freq == freq) {
					if (best_freq == -1) {
						best_post_div = post_div;
						best_ref_div = ref_div;
						best_feedback_div = feedback_div;
						best_freq = current_freq;
						best_error = error;
						best_vco_diff = vco_diff;
					} else if ((flags & RADEON_PLL_PREFER_LOW_REF_DIV) && (ref_div < best_ref_div)) {
						best_post_div = post_div;
						best_ref_div = ref_div;
						best_feedback_div = feedback_div;
						best_freq = current_freq;
						best_error = error;
						best_vco_diff = vco_diff;
					}
				}

				if (current_freq < freq)
					min_feed_div = feedback_div+1;
				else
					max_feed_div = feedback_div;
			}
		}
	}

	*dot_clock_p = best_freq / 10000;
	*fb_div_p = best_feedback_div;
	*ref_div_p = best_ref_div;
	*post_div_p = best_post_div;
}

void radeon_get_clock_info(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct radeon_pll *pll = &dev_priv->mode_info.pll;
	int ret;

	if (dev_priv->is_atom_bios)
		ret = radeon_atom_get_clock_info(dev);
	else
		ret = radeon_combios_get_clock_info(dev);

	if (ret) {

		if (pll->reference_div < 2) pll->reference_div = 12;
	} else {
		// TODO FALLBACK
	}

	if (radeon_is_avivo(dev_priv)) {
		pll->min_post_div = 2;
		pll->max_post_div = 0x7f;
	} else {
		pll->min_post_div = 1;
		pll->max_post_div = 12; // 16 on crtc 0??
	}

	pll->min_ref_div = 2;
	pll->max_ref_div = 0x3ff;
	pll->min_feedback_div = 4;
	pll->max_feedback_div = 0x7ff;
	pll->best_vco = 0;

}

static void radeon_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct radeon_framebuffer *radeon_fb = to_radeon_framebuffer(fb);
	struct drm_device *dev = fb->dev;

	if (fb->fbdev)
		radeonfb_remove(dev, fb);

	drm_framebuffer_cleanup(fb);
	kfree(radeon_fb);
}

static const struct drm_framebuffer_funcs radeon_fb_funcs = {
	.destroy = radeon_user_framebuffer_destroy,
};

struct drm_framebuffer *radeon_user_framebuffer_create(struct drm_device *dev,
						       struct drm_file *filp,
						       struct drm_mode_fb_cmd *mode_cmd)
{

	struct radeon_framebuffer *radeon_fb;

	radeon_fb = kzalloc(sizeof(*radeon_fb), GFP_KERNEL);
	if (!radeon_fb)
		return NULL;

	drm_framebuffer_init(dev, &radeon_fb->base, &radeon_fb_funcs);
	drm_helper_mode_fill_fb_struct(&radeon_fb->base, mode_cmd);

	if (filp) {
		radeon_fb->obj = drm_gem_object_lookup(dev, filp,
						       mode_cmd->handle);
		if (!radeon_fb->obj) {
			kfree(radeon_fb);
			return NULL;
		}
		drm_gem_object_unreference(radeon_fb->obj);
	}
	return &radeon_fb->base;
}

static const struct drm_mode_config_funcs radeon_mode_funcs = {
	.fb_create = radeon_user_framebuffer_create,
	.fb_changed = radeonfb_probe,
};


int radeon_modeset_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	static struct card_info card;
	size_t size;
	int num_crtc = 2, i;
	int ret;

	drm_mode_config_init(dev);

	dev->mode_config.funcs = (void *)&radeon_mode_funcs;

	if (radeon_is_avivo(dev_priv)) {
		    dev->mode_config.max_width = 8192;
		    dev->mode_config.max_height = 8192;
	} else {
		    dev->mode_config.max_width = 4096;
		    dev->mode_config.max_height = 4096;
	}

	dev->mode_config.fb_base = dev_priv->fb_aper_offset;

	/* allocate crtcs - TODO single crtc */
	for (i = 0; i < num_crtc; i++) {
		radeon_crtc_init(dev, i);
	}

	/* okay we should have all the bios connectors */

	ret = radeon_setup_enc_conn(dev);

	if (!ret)
		return ret;

	drm_helper_initial_config(dev, false);

	return 0;
}


int radeon_load_modeset_init(struct drm_device *dev)
{
	int ret;
	ret = radeon_modeset_init(dev);

	return ret;
}

void radeon_modeset_cleanup(struct drm_device *dev)
{
	drm_mode_config_cleanup(dev);
}

