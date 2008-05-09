/*
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
 * Copyright (c) 2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 */
#ifndef __INTEL_DRV_H__
#define __INTEL_DRV_H__

#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/i2c-algo-bit.h>
#include "drm_crtc.h"

/*
 * Display related stuff
 */

/* store information about an Ixxx DVO */
/* The i830->i865 use multiple DVOs with multiple i2cs */
/* the i915, i945 have a single sDVO i2c bus - which is different */
#define MAX_OUTPUTS 6

#define INTEL_I2C_BUS_DVO 1
#define INTEL_I2C_BUS_SDVO 2

/* these are outputs from the chip - integrated only 
   external chips are via DVO or SDVO output */
#define INTEL_OUTPUT_UNUSED 0
#define INTEL_OUTPUT_ANALOG 1
#define INTEL_OUTPUT_DVO 2
#define INTEL_OUTPUT_SDVO 3
#define INTEL_OUTPUT_LVDS 4
#define INTEL_OUTPUT_TVOUT 5

#define INTEL_DVO_CHIP_NONE 0
#define INTEL_DVO_CHIP_LVDS 1
#define INTEL_DVO_CHIP_TMDS 2
#define INTEL_DVO_CHIP_TVOUT 4

struct intel_i2c_chan {
	struct drm_device *drm_dev; /* for getting at dev. private (mmio etc.) */
	u32 reg; /* GPIO reg */
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data algo;
        u8 slave_addr;
};

struct intel_output {
	int type;
	struct intel_i2c_chan *i2c_bus; /* for control functions */
	struct intel_i2c_chan *ddc_bus; /* for DDC only stuff */
	bool load_detect_temp;
	void *dev_priv;
};

struct intel_crtc {
	int pipe;
	int plane;
	uint32_t cursor_addr;
	u8 lut_r[256], lut_g[256], lut_b[256];
	int dpms_mode;
};

struct intel_i2c_chan *intel_i2c_create(struct drm_device *dev, const u32 reg,
					const char *name);
void intel_i2c_destroy(struct intel_i2c_chan *chan);
int intel_ddc_get_modes(struct drm_output *output);
extern bool intel_ddc_probe(struct drm_output *output);

extern void intel_crt_init(struct drm_device *dev);
extern void intel_sdvo_init(struct drm_device *dev, int output_device);
extern void intel_dvo_init(struct drm_device *dev);
extern void intel_tv_init(struct drm_device *dev);
extern void intel_lvds_init(struct drm_device *dev);

extern void intel_crtc_load_lut(struct drm_crtc *crtc);
extern void intel_output_prepare (struct drm_output *output);
extern void intel_output_commit (struct drm_output *output);
extern struct drm_display_mode *intel_crtc_mode_get(struct drm_device *dev,
 						    struct drm_crtc *crtc);
extern void intel_wait_for_vblank(struct drm_device *dev);
extern struct drm_crtc *intel_get_crtc_from_pipe(struct drm_device *dev, int pipe);
extern struct drm_crtc *intel_get_load_detect_pipe(struct drm_output *output,
						   struct drm_display_mode *mode,
						   int *dpms_mode);
extern void intel_release_load_detect_pipe(struct drm_output *output,
					   int dpms_mode);

extern struct drm_output* intel_sdvo_find(struct drm_device *dev, int sdvoB);
extern int intel_sdvo_supports_hotplug(struct drm_output *output);
extern void intel_sdvo_set_hotplug(struct drm_output *output, int enable);

extern int intelfb_probe(struct drm_device *dev, struct drm_crtc *crtc, struct drm_output *output);
extern int intelfb_remove(struct drm_device *dev, struct drm_framebuffer *fb);
extern int intelfb_resize(struct drm_device *dev, struct drm_crtc *crtc);

#endif /* __INTEL_DRV_H__ */
