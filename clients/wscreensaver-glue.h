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

#ifndef WSCREENSAVER_GLUE_H
#define WSCREENSAVER_GLUE_H

/*
 * This file is glue, that tries to avoid changing glmatrix.c from the
 * original too much, hopefully easing the porting of other (GL)
 * xscreensaver "hacks".
 */

#include "wscreensaver.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "window.h"

#define ENTRYPOINT static

typedef bool Bool;
#define True true
#define False false

typedef struct ModeInfo ModeInfo;

#define MI_DISPLAY(mi) NULL
#define MI_WINDOW(mi) (mi)
#define MI_SCREEN(mi) ((mi)->instance_number)
#define MI_WIDTH(mi) ((mi)->width)
#define MI_HEIGHT(mi) ((mi)->height)
#define MI_IS_WIREFRAME(mi) 0
#define MI_NUM_SCREENS(mi) 16

typedef EGLContext GLXContext;

double frand(double f);
void clear_gl_error(void);
void check_gl_error(const char *msg);

static inline void
glXMakeCurrent(void *dummy, ModeInfo *mi, EGLContext ctx)
{
	assert(mi->eglctx == ctx);
}

static inline void
glXSwapBuffers(void *dummy, ModeInfo *mi)
{
	mi->swap_buffers = 1;
}

static inline void
do_fps(ModeInfo *mi)
{
}

/* just enough XImage to satisfy glmatrix.c */

typedef struct _XImage {
	int width;
	int height;
	char *data;
	int bytes_per_line;
} XImage;

XImage *xpm_to_ximage(char **xpm_data);
void XDestroyImage(XImage *xi);

static inline unsigned long
XGetPixel(XImage *xi, int x, int y)
{
	return *(uint32_t *)(xi->data + xi->bytes_per_line * y + 4 * x);
}

static inline void
XPutPixel(XImage *xi, int x, int y, unsigned long pixel)
{
	*(uint32_t *)(xi->data + xi->bytes_per_line * y + 4 * x) = pixel;
}

/*
 * override glViewport from the plugin, so we can set it up properly
 * rendering to a regular decorated Wayland window.
 */
#ifdef glViewport
#undef glViewport
#endif
#define glViewport(x,y,w,h) do {} while (0)

#endif /* WSCREENSAVER_GLUE_H */
