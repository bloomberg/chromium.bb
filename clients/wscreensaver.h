/*
 * Copyright © 2011 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef WSCREENSAVER_H
#define WSCREENSAVER_H

#include "config.h"

#define MESA_EGL_NO_X11_HEADERS
#include <EGL/egl.h>

extern const char *progname;

struct wscreensaver;

struct ModeInfo {
	struct wscreensaver *priv;
	EGLContext eglctx;
	int swap_buffers;

	struct window *window;
	struct widget *widget;

	int instance_number;
	int width;
	int height;

	unsigned long polygon_count;
	int fps_p;
};

struct wscreensaver_plugin {
	const char *name;
	void (*init)(struct ModeInfo *mi);
	void (*draw)(struct ModeInfo *mi);
	void (*reshape)(struct ModeInfo *mi, int w, int h);
/*	void (*refresh)(struct ModeInfo *mi);
	void (*finish)(struct ModeInfo *mi);*/
};

EGLContext *
init_GL(struct ModeInfo *mi);

#endif /* WSCREENSAVER_H */
