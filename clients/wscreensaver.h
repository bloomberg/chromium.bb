/*
 * Copyright © 2011 Collabora, Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
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
