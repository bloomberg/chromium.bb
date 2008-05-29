/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 */

/*
 * The DRM mode setting helper functions are common code for drivers to use if they wish.
 * Drivers are not forced to use this code in their implementations but it would be useful
 * if they code they do use at least provides a consistent interface and operation to userspace
 */

#ifndef __DRM_CRTC_HELPER_H__
#define __DRM_CRTC_HELPER_H__

#include <linux/i2c.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/idr.h>

#include <linux/fb.h>

struct drm_crtc_helper_funcs {
  	void (*prepare)(struct drm_crtc *crtc);
	void (*commit)(struct drm_crtc *crtc);

	/* Provider can fixup or change mode timings before modeset occurs */
	bool (*mode_fixup)(struct drm_crtc *crtc,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	/* Actually set the mode */
	void (*mode_set)(struct drm_crtc *crtc, struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode, int x, int y);

	/* Move the crtc on the current fb to the given position *optional* */
	void (*mode_set_base)(struct drm_crtc *crtc, int x, int y);
};

struct drm_output_helper_funcs {
	bool (*mode_fixup)(struct drm_output *output,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	void (*prepare)(struct drm_output *output);
	void (*commit)(struct drm_output *output);
	void (*mode_set)(struct drm_output *output,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode);
};

extern int drm_helper_hotplug_stage_two(struct drm_device *dev, struct drm_output *output,
					bool connected);
extern bool drm_helper_initial_config(struct drm_device *dev, bool can_grow);
extern int drm_crtc_helper_set_config(struct drm_mode_set *set);
extern bool drm_crtc_helper_set_mode(struct drm_crtc *crtc, struct drm_display_mode *mode,
				     int x, int y);

static inline void drm_crtc_helper_add(struct drm_crtc *crtc, const struct drm_crtc_helper_funcs *funcs)
{
	crtc->helper_private = (void *)funcs;
}

static inline void drm_output_helper_add(struct drm_output *output, const struct drm_output_helper_funcs *funcs)
{
	output->helper_private = (void *)funcs;
}



#endif
