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

#ifndef __NV50_CRTC_H__
#define __NV50_CRTC_H__

#include "nv50_display.h"

struct nv50_cursor;
struct nv50_lut;
struct nv50_fb;

struct nv50_crtc {
	struct list_head item;

	struct drm_device *dev;
	int index;
	bool enabled;
	bool blanked;

	struct nouveau_hw_mode *mode;
	struct nouveau_hw_mode *native_mode;

	bool use_native_mode;
	bool use_dithering;

	/* Changing scaling modes requires a modeset sometimes. */
	/* We need to know the currently active hw mode, as well as the requested one by the user. */
	int requested_scaling_mode;
	int scaling_mode;

	struct nv50_cursor *cursor;
	struct nv50_lut *lut;
	struct nv50_fb *fb;

	int (*validate_mode) (struct nv50_crtc *crtc, struct nouveau_hw_mode *mode);
	int (*set_mode) (struct nv50_crtc *crtc, struct nouveau_hw_mode *mode);
	int (*execute_mode) (struct nv50_crtc *crtc);
	int (*set_fb) (struct nv50_crtc *crtc);
	int (*blank) (struct nv50_crtc *crtc, bool blanked);
	int (*set_dither) (struct nv50_crtc *crtc);
	int (*set_scale) (struct nv50_crtc *crtc);
	int (*set_clock) (struct nv50_crtc *crtc);
	int (*set_clock_mode) (struct nv50_crtc *crtc);
	int (*destroy) (struct nv50_crtc *crtc);
};

int nv50_crtc_create(struct drm_device *dev, int index);

#endif /* __NV50_CRTC_H__ */
