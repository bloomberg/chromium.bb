/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
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
 * Original Authors:
 *   Kevin E. Martin, Rickard E. Faith, Alan Hourihane
 *
 * Kernel port Author: Dave Airlie
 */

#ifndef RADEON_MODE_H
#define RADEON_MODE_H

#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/i2c-algo-bit.h>

#define to_radeon_crtc(x) container_of(x, struct radeon_crtc, base)
#define to_radeon_connector(x) container_of(x, struct radeon_connector, base)
#define to_radeon_encoder(x) container_of(x, struct radeon_encoder, base)
#define to_radeon_framebuffer(x) container_of(x, struct radeon_framebuffer, base)

enum radeon_connector_type {
	CONNECTOR_NONE,
	CONNECTOR_VGA,
	CONNECTOR_DVI_I,
	CONNECTOR_DVI_D,
	CONNECTOR_DVI_A,
	CONNECTOR_STV,
	CONNECTOR_CTV,
	CONNECTOR_LVDS,
	CONNECTOR_DIGITAL,
	CONNECTOR_SCART,
	CONNECTOR_HDMI_TYPE_A,
	CONNECTOR_HDMI_TYPE_B,
	CONNECTOR_0XC,
	CONNECTOR_0XD,
	CONNECTOR_DIN,
	CONNECTOR_DISPLAY_PORT,
	CONNECTOR_UNSUPPORTED
};

enum radeon_dac_type {
	DAC_NONE = 0,
	DAC_PRIMARY = 1,
	DAC_TVDAC = 2,
	DAC_EXT = 3
};

enum radeon_tmds_type {
	TMDS_NONE = 0,
	TMDS_INT = 1,
	TMDS_EXT = 2,
	TMDS_LVTMA = 3,
	TMDS_DDIA = 4,
	TMDS_UNIPHY = 5
};

enum radeon_dvi_type {
	DVI_AUTO,
	DVI_DIGITAL,
	DVI_ANALOG
};

enum radeon_rmx_type {
	RMX_OFF,
	RMX_FULL,
	RMX_CENTER,
};

enum radeon_tv_std {
	TV_STD_NTSC,
	TV_STD_PAL,
	TV_STD_PAL_M,
	TV_STD_PAL_60,
	TV_STD_NTSC_J,
	TV_STD_SCART_PAL,
	TV_STD_SECAM,
	TV_STD_PAL_CN,
};

struct radeon_i2c_bus_rec {
	bool valid;
	uint32_t mask_clk_reg;
	uint32_t mask_data_reg;
	uint32_t a_clk_reg;
	uint32_t a_data_reg;
	uint32_t put_clk_reg;
	uint32_t put_data_reg;
	uint32_t get_clk_reg;
	uint32_t get_data_reg;
	uint32_t mask_clk_mask;
	uint32_t mask_data_mask;
	uint32_t put_clk_mask;
	uint32_t put_data_mask;
	uint32_t get_clk_mask;
	uint32_t get_data_mask;
	uint32_t a_clk_mask;
	uint32_t a_data_mask;
};

struct radeon_bios_connector {
	enum radeon_dac_type dac_type;
	enum radeon_tmds_type tmds_type;
	enum radeon_connector_type connector_type;
	bool valid;
	int output_id;
	int devices;
	int hpd_mask;
	struct radeon_i2c_bus_rec ddc_i2c;
	int igp_lane_info;
};

struct radeon_tmds_pll {
    uint32_t freq;
    uint32_t value;
};

#define RADEON_MAX_BIOS_CONNECTOR 16

#define RADEON_PLL_USE_BIOS_DIVS   (1 << 0)
#define RADEON_PLL_NO_ODD_POST_DIV (1 << 1)
#define RADEON_PLL_USE_REF_DIV     (1 << 2)
#define RADEON_PLL_LEGACY          (1 << 3)
#define RADEON_PLL_PREFER_LOW_REF_DIV (1 << 4)

struct radeon_pll {
	uint16_t reference_freq;
	uint16_t reference_div;
	uint32_t pll_in_min;
	uint32_t pll_in_max;
	uint32_t pll_out_min;
	uint32_t pll_out_max;
	uint16_t xclk;

	uint32_t min_ref_div;
	uint32_t max_ref_div;
	uint32_t min_post_div;
	uint32_t max_post_div;
	uint32_t min_feedback_div;
	uint32_t max_feedback_div;
	uint32_t best_vco;
};

struct radeon_mode_info {
	struct atom_context *atom_context;
	struct radeon_bios_connector bios_connector[RADEON_MAX_BIOS_CONNECTOR];
	struct radeon_pll pll;
};

struct radeon_crtc {
	struct drm_crtc base;
	int crtc_id;
	u8 lut_r[256], lut_g[256], lut_b[256];
	bool enabled;
	bool can_tile;
	uint32_t crtc_offset;
	struct radeon_framebuffer *fbdev_fb;
	struct drm_mode_set mode_set;
};

struct radeon_i2c_chan {
	struct drm_device *dev;
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data algo;
	struct radeon_i2c_bus_rec rec;
};


#define RADEON_USE_RMX 1

struct radeon_encoder {
	struct drm_encoder base;
	uint32_t encoder_mode;
	uint32_t flags;
	enum radeon_rmx_type rmx_type;
	union {
		enum radeon_dac_type dac;
		enum radeon_tmds_type tmds;
	} type;
	int atom_device; /* atom devices */

	/* preferred mode */
	uint32_t panel_xres, panel_yres;
	uint32_t hoverplus, hsync_width;
	uint32_t hblank;
	uint32_t voverplus, vsync_width;
	uint32_t vblank;
	uint32_t dotclock;

	/* legacy lvds */
	uint16_t panel_vcc_delay;
	uint16_t panel_pwr_delay;
	uint16_t panel_digon_delay;
	uint16_t panel_blon_delay;
	uint32_t panel_ref_divider;
	uint32_t panel_post_divider;
	uint32_t panel_fb_divider;
	bool use_bios_dividers;
	uint32_t lvds_gen_cntl;

	/* legacy primary dac */
	uint32_t ps2_pdac_adj;

	/* legacy tv dac */
	uint32_t ps2_tvdac_adj;
	uint32_t ntsc_tvdac_adj;
	uint32_t pal_tvdac_adj;
	enum radeon_tv_std tv_std;

	/* legacy int tmds */
	struct radeon_tmds_pll tmds_pll[4];
};

struct radeon_connector {
	struct drm_connector base;
	struct radeon_i2c_chan *ddc_bus;
	int use_digital;
};

struct radeon_framebuffer {
	struct drm_framebuffer base;
	struct drm_gem_object *obj;
	struct drm_bo_kmap_obj kmap_obj;
};

extern struct radeon_i2c_chan *radeon_i2c_create(struct drm_device *dev,
						 struct radeon_i2c_bus_rec *rec,
						 const char *name);
extern void radeon_i2c_destroy(struct radeon_i2c_chan *i2c);
extern bool radeon_ddc_probe(struct radeon_connector *radeon_connector);
extern int radeon_ddc_get_modes(struct radeon_connector *radeon_connector);
extern struct drm_connector *radeon_connector_add(struct drm_device *dev, int bios_index);

extern struct drm_encoder *radeon_best_encoder(struct drm_connector *connector);

extern void radeon_compute_pll(struct radeon_pll *pll,
			       uint64_t freq,
			       uint32_t *dot_clock_p,
			       uint32_t *fb_div_p,
			       uint32_t *ref_div_p,
			       uint32_t *post_div_p,
			       int flags);

struct drm_encoder *radeon_encoder_lvtma_add(struct drm_device *dev, int bios_index);
struct drm_encoder *radeon_encoder_atom_dac_add(struct drm_device *dev, int bios_index, int dac_id, int with_tv);
struct drm_encoder *radeon_encoder_atom_tmds_add(struct drm_device *dev, int bios_index, int tmds_type);
struct drm_encoder *radeon_encoder_legacy_lvds_add(struct drm_device *dev, int bios_index);
struct drm_encoder *radeon_encoder_legacy_primary_dac_add(struct drm_device *dev, int bios_index, int with_tv);
struct drm_encoder *radeon_encoder_legacy_tv_dac_add(struct drm_device *dev, int bios_index, int with_tv);
struct drm_encoder *radeon_encoder_legacy_tmds_int_add(struct drm_device *dev, int bios_index);
struct drm_encoder *radeon_encoder_legacy_tmds_ext_add(struct drm_device *dev, int bios_index);

extern void radeon_crtc_load_lut(struct drm_crtc *crtc);
extern void atombios_crtc_set_base(struct drm_crtc *crtc, int x, int y);
extern void atombios_crtc_mode_set(struct drm_crtc *crtc,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode,
				   int x, int y);
extern void atombios_crtc_dpms(struct drm_crtc *crtc, int mode);

extern int radeon_crtc_cursor_set(struct drm_crtc *crtc,
				  struct drm_file *file_priv,
				  uint32_t handle,
				  uint32_t width,
				  uint32_t height);
extern int radeon_crtc_cursor_move(struct drm_crtc *crtc,
				   int x, int y);

extern bool radeon_atom_get_clock_info(struct drm_device *dev);
extern bool radeon_combios_get_clock_info(struct drm_device *dev);
extern void radeon_atombios_get_lvds_info(struct radeon_encoder *encoder);
extern void radeon_atombios_get_tmds_info(struct radeon_encoder *encoder);
extern bool radeon_combios_get_lvds_info(struct radeon_encoder *encoder);
extern bool radeon_combios_get_tmds_info(struct radeon_encoder *encoder);
extern void radeon_combios_get_ext_tmds_info(struct radeon_encoder *encoder);
extern bool radeon_combios_get_tv_info(struct radeon_encoder *encoder);
extern bool radeon_combios_get_tv_dac_info(struct radeon_encoder *encoder);
extern bool radeon_combios_get_primary_dac_info(struct radeon_encoder *encoder);
extern void radeon_combios_output_lock(struct drm_encoder *encoder, bool lock);
extern void radeon_combios_initialize_bios_scratch_regs(struct drm_device *dev);
extern void radeon_atom_output_lock(struct drm_encoder *encoder, bool lock);
extern void radeon_atom_initialize_bios_scratch_regs(struct drm_device *dev);
extern void radeon_crtc_fb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green,
				     u16 blue, int regno);
struct drm_framebuffer *radeon_user_framebuffer_create(struct drm_device *dev,
						       struct drm_file *filp,
						       struct drm_mode_fb_cmd *mode_cmd);

int radeonfb_probe(struct drm_device *dev);

int radeonfb_remove(struct drm_device *dev, struct drm_framebuffer *fb);
bool radeon_get_legacy_connector_info_from_bios(struct drm_device *dev);
void radeon_atombios_init_crtc(struct drm_device *dev,
			       struct radeon_crtc *radeon_crtc);
void radeon_legacy_init_crtc(struct drm_device *dev,
			     struct radeon_crtc *radeon_crtc);
void radeon_i2c_do_lock(struct radeon_connector *radeon_connector, int lock_state);

void radeon_atom_static_pwrmgt_setup(struct drm_device *dev, int enable);
void radeon_atom_dyn_clk_setup(struct drm_device *dev, int enable);
void radeon_combios_dyn_clk_setup(struct drm_device *dev, int enable);
void radeon_get_clock_info(struct drm_device *dev);
extern bool radeon_get_atom_connector_info_from_bios_connector_table(struct drm_device *dev);

void radeon_rmx_mode_fixup(struct drm_encoder *encoder,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
void radeon_enc_destroy(struct drm_encoder *encoder);

#endif
