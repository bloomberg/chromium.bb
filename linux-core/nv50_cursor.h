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

#ifndef __NV50_CURSOR_H__
#define __NV50_CURSOR_H__

#include "nv50_display.h"

struct nv50_crtc;

struct nv50_cursor {
	struct mem_block *block;
	int x, y;
	bool visible;
	bool enabled;

	int (*show) (struct nv50_crtc *crtc);
	int (*hide) (struct nv50_crtc *crtc);
	int (*set_pos) (struct nv50_crtc *crtc, int x, int y);
	int (*set_offset) (struct nv50_crtc *crtc);
	int (*set_bo) (struct nv50_crtc *crtc, drm_handle_t handle);
	int (*enable) (struct nv50_crtc *crtc);
	int (*disable) (struct nv50_crtc *crtc);
};

int nv50_cursor_create(struct nv50_crtc *crtc);
int nv50_cursor_destroy(struct nv50_crtc *crtc);

#endif /* __NV50_CURSOR_H__ */
