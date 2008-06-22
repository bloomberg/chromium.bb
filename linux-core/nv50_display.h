/*
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __NV50_DISPLAY_H__
#define __NV50_DISPLAY_H__

#include "drmP.h"
#include "drm.h"
#include "nouveau_dma.h"
#include "nouveau_drv.h"
#include "nouveau_reg.h"
#include "nv50_display_commands.h"

/* for convience, so you can see through the trees. */
#define NV50_DEBUG DRM_ERROR

struct nouveau_hw_mode {
	unsigned int clock;
	unsigned short hdisplay, hblank_start, hsync_start, hsync_end, hblank_end, htotal;
	unsigned short vdisplay, vblank_start, vsync_start, vsync_end, vblank_end, vtotal;

	unsigned int flags;
};

struct nv50_crtc;
struct nv50_output;
struct nv50_connector;

struct nv50_display {
	struct drm_device *dev;

	bool preinit_done;
	bool init_done;

	int last_crtc; /* crtc used for last mode set */

	int (*pre_init) (struct nv50_display *display);
	int (*init) (struct nv50_display *display);
	int (*disable) (struct nv50_display *display);
	int (*update) (struct nv50_display *display);

	struct list_head crtcs;
	struct list_head outputs;
	struct list_head connectors;
};

enum scaling_modes {
	SCALE_PANEL,
	SCALE_FULLSCREEN,
	SCALE_ASPECT,
	SCALE_NOSCALE,
	SCALE_INVALID
};

void nv50_display_command(struct drm_nouveau_private *dev_priv, uint32_t mthd, uint32_t val);
struct nv50_display *nv50_get_display(struct drm_device *dev);
int nv50_display_create(struct drm_device *dev);
int nv50_display_destroy(struct drm_device *dev);

#endif /* __NV50_DISPLAY_H__ */
