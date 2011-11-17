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

#include "wayland-util.h"
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
