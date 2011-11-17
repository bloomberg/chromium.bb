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

#include "config.h"

#include "wscreensaver.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <GL/gl.h>
#include <EGL/eglext.h>

#include "wayland-util.h"
#include "wayland-client.h"

#include "window.h"

extern struct wscreensaver_plugin glmatrix_screensaver;

static const struct wscreensaver_plugin * const plugins[] = {
	&glmatrix_screensaver,
	NULL
};

const char *progname = NULL;

struct wscreensaver {
	struct display *display;

	/* per output, if fullscreen mode */
	struct ModeInfo *modeinfo;

	struct {
		EGLDisplay display;
		EGLConfig config;
	} egl;

	const struct wscreensaver_plugin *plugin;
};

static void
draw_instance(struct ModeInfo *mi)
{
	struct wscreensaver *wscr = mi->priv;
	struct rectangle drawarea;
	struct rectangle winarea;
	int bottom;

	mi->swap_buffers = 0;

	window_draw(mi->window);

	window_get_child_allocation(mi->window, &drawarea);
	window_get_allocation(mi->window, &winarea);

	if (display_acquire_window_surface(wscr->display,
					   mi->window,
					   mi->eglctx) < 0) {
		fprintf(stderr, "%s: unable to acquire window surface",
			progname);
		return;
	}

	bottom = winarea.height - (drawarea.height + drawarea.y);
	glViewport(drawarea.x, bottom, drawarea.width, drawarea.height);
	glScissor(drawarea.x, bottom, drawarea.width, drawarea.height);
	glEnable(GL_SCISSOR_TEST);

	if (mi->width != drawarea.width || mi->height != drawarea.height) {
		mi->width = drawarea.width;
		mi->height = drawarea.height;
		wscr->plugin->reshape(mi, mi->width, mi->height);
	}

	wscr->plugin->draw(mi);

	if (mi->swap_buffers == 0)
		fprintf(stderr, "%s: swapBuffers not called\n", progname);

	display_release_window_surface(wscr->display, mi->window);
	window_flush(mi->window);
}

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	struct ModeInfo *mi = data;
	static const struct wl_callback_listener listener = {
		frame_callback
	};

	draw_instance(mi);

	if (callback)
		wl_callback_destroy(callback);

	callback = wl_surface_frame(window_get_wl_surface(mi->window));
	wl_callback_add_listener(callback, &listener, mi);
}

static void
init_frand(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	srandom(tv.tv_sec * 100 + tv.tv_usec / 10000);
}

WL_EXPORT EGLContext *
init_GL(struct ModeInfo *mi)
{
	struct wscreensaver *wscr = mi->priv;
	EGLContext *pctx;

	pctx = malloc(sizeof *pctx);
	if (!pctx)
		return NULL;

	if (mi->eglctx != EGL_NO_CONTEXT) {
		fprintf(stderr, "%s: multiple GL contexts are not supported",
			progname);
		goto errout;
	}

	mi->eglctx = eglCreateContext(wscr->egl.display, wscr->egl.config,
				      EGL_NO_CONTEXT, NULL);
	if (mi->eglctx == EGL_NO_CONTEXT) {
		fprintf(stderr, "%s: init_GL failed to create EGL context\n",
			progname);
		goto errout;
	}

	if (!eglMakeCurrent(wscr->egl.display, NULL, NULL, mi->eglctx)) {
		fprintf(stderr, "%s: init_GL failed on eglMakeCurrent\n",
			progname);
		goto errout;
	}

	glClearColor(0.0, 0.0, 0.0, 1.0);

	*pctx = mi->eglctx;
	return pctx;

errout:
	free(pctx);
	return NULL;
}

static struct ModeInfo *
create_modeinfo(struct wscreensaver *wscr, struct window *window)
{
	struct ModeInfo *mi;
	struct rectangle drawarea;

	mi = calloc(1, sizeof *mi);
	if (!mi)
		return NULL;

	window_get_child_allocation(window, &drawarea);

	mi->priv = wscr;
	mi->eglctx = EGL_NO_CONTEXT;

	mi->window = window;

	mi->instance_number = 0; /* XXX */
	mi->width = drawarea.width;
	mi->height = drawarea.height;

	return mi;
}

static struct ModeInfo *
create_wscreensaver_instance(struct wscreensaver *wscr)
{
	struct ModeInfo *mi;
	struct window *window;
	
	window = window_create(wscr->display, 400, 300);
	if (!window) {
		fprintf(stderr, "%s: creating a window failed.\n", progname);
		return NULL;
	}

	window_set_transparent(window, 0);
	window_set_title(window, progname);

	mi = create_modeinfo(wscr, window);
	if (!mi)
		return NULL;

	wscr->plugin->init(mi);

	frame_callback(mi, NULL, 0);
	return mi;
}

/* returns error message, or NULL if success */
static const char *
init_wscreensaver(struct wscreensaver *wscr, struct display *display)
{
	int size;
	const char prefix[] = "wscreensaver::";
	char *str;

	wscr->display = display;
	wscr->plugin = plugins[0];

	size = sizeof(prefix) + strlen(wscr->plugin->name);
	str = malloc(size);
	if (!str)
		return "out of memory";
	snprintf(str, size, "%s%s", prefix, wscr->plugin->name);
	progname = str;

	wscr->egl.display = display_get_egl_display(wscr->display);
	if (!wscr->egl.display)
		return "no EGL display";

	eglBindAPI(EGL_OPENGL_API);
	wscr->egl.config = display_get_rgb_egl_config(wscr->display);

	wscr->modeinfo = create_wscreensaver_instance(wscr);

	return NULL;
}

int main(int argc, char *argv[])
{
	struct display *d;
	struct wscreensaver wscr = { 0 };
	const char *msg;

	init_frand();

	d = display_create(&argc, &argv, NULL);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return EXIT_FAILURE;
	}

	msg = init_wscreensaver(&wscr, d);
	if (msg) {
		fprintf(stderr, "wscreensaver init failed: %s\n", msg);
		return EXIT_FAILURE;
	}

	display_run(d);

	free((void *)progname);

	return EXIT_SUCCESS;
}
