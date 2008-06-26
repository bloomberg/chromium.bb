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

#ifndef __NV50_OUTPUT_H__
#define __NV50_OUTPUT_H__

#include "nv50_crtc.h"

#define OUTPUT_UNKNOWN 0
#define OUTPUT_DAC 1
#define OUTPUT_TMDS 2
#define OUTPUT_LVDS 3
#define OUTPUT_TV 4

struct nv50_output {
	struct list_head head;

	struct drm_device *dev;
	int bus;
	int dcb_entry;
	int type;

	struct nv50_crtc *crtc;
	struct nouveau_hw_mode *native_mode;

	int (*validate_mode) (struct nv50_output *output, struct nouveau_hw_mode *mode);
	int (*execute_mode) (struct nv50_output *output, bool disconnect);
	int (*set_clock_mode) (struct nv50_output *output);
	/* this is not a normal modeset call, it is a direct register write, so it's executed immediately */
	int (*set_power_mode) (struct nv50_output *output, int mode);
	bool (*detect) (struct nv50_output *output);
	int (*destroy) (struct nv50_output *output);
};

int nv50_output_or_offset(struct nv50_output *output);
int nv50_sor_create(struct drm_device *dev, int dcb_entry);
int nv50_dac_create(struct drm_device *dev, int dcb_entry);

#endif /* __NV50_OUTPUT_H__ */
