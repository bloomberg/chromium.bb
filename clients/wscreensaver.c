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

#include "../config.h"

#include "wscreensaver.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <GL/gl.h>
#include <EGL/eglext.h>

#include <wayland-client.h>

#include "desktop-shell-client-protocol.h"
#include "window.h"
#include "shared/helpers.h"

extern struct wscreensaver_plugin glmatrix_screensaver;

static const struct wscreensaver_plugin * const plugins[] = {
	&glmatrix_screensaver,
	NULL
};

const char *progname = NULL;

static int demo_mode;

struct wscreensaver {
	struct screensaver *interface;

	struct display *display;

	struct ModeInfo *demomode;

	struct {
		EGLDisplay display;
		EGLConfig config;
	} egl;

	const struct wscreensaver_plugin *plugin;
};

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	struct ModeInfo *mi = data;

	window_schedule_redraw(mi->window);
	wl_callback_destroy(callback);
}

static const struct wl_callback_listener listener = {
	frame_callback
};

static void
redraw_handler(struct widget *widget, void *data)
{
	struct ModeInfo *mi = data;
	struct wscreensaver *wscr = mi->priv;
	struct rectangle drawarea;
	struct rectangle winarea;
	struct wl_callback *callback;
	int bottom;

	mi->swap_buffers = 0;

	widget_get_allocation(mi->widget, &drawarea);
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
create_wscreensaver_instance(struct wscreensaver *screensaver,
			     struct wl_output *output, int width, int height)
{
	static int instance;
	struct ModeInfo *mi;
	struct rectangle drawarea;

	mi = calloc(1, sizeof *mi);
	if (!mi)
		return NULL;

	if (demo_mode)
		mi->window = window_create(screensaver->display);
	else
		mi->window = window_create_custom(screensaver->display);

	if (!mi->window) {
		fprintf(stderr, "%s: creating a window failed.\n", progname);
		free(mi);
		return NULL;
	}

	window_set_title(mi->window, progname);

	if (screensaver->interface && !demo_mode) {
		mi->widget = window_add_widget(mi->window, mi);
		screensaver_set_surface(screensaver->interface,
					window_get_wl_surface(mi->window),
					output);
	} else {
		mi->widget = window_frame_create(mi->window, mi);
	}
	widget_set_redraw_handler(mi->widget, redraw_handler);

	mi->priv = screensaver;
	mi->eglctx = EGL_NO_CONTEXT;
	mi->instance_number = instance++; /* XXX */

	widget_get_allocation(mi->widget, &drawarea);
	mi->width = drawarea.width;
	mi->height = drawarea.height;

	screensaver->plugin->init(mi);

	window_schedule_resize(mi->window, width, height);
	return mi;
}

static void
handle_output_destroy(struct output *output, void *data)
{
	/* struct ModeInfo *mi = data;
	 * TODO */
}

static void
handle_output_configure(struct output *output, void *data)
{
	struct wscreensaver *screensaver = data;
	struct ModeInfo *mi;
	struct rectangle area;

	/* skip existing outputs */
	if (output_get_user_data(output))
		return;

	output_get_allocation(output, &area);
	mi = create_wscreensaver_instance(screensaver,
					  output_get_wl_output(output),
					  area.width, area.height);
	output_set_user_data(output, mi);
	output_set_destroy_handler(output, handle_output_destroy);
}

static int
init_wscreensaver(struct wscreensaver *wscr, struct display *display)
{
	int size;
	const char prefix[] = "wscreensaver::";
	char *str;

	display_set_user_data(display, wscr);
	wscr->display = display;
	wscr->plugin = plugins[0];

	size = sizeof(prefix) + strlen(wscr->plugin->name);
	str = malloc(size);
	if (!str) {
		fprintf(stderr, "init: out of memory\n");
		return -1;
	}
	snprintf(str, size, "%s%s", prefix, wscr->plugin->name);
	progname = str;

	wscr->egl.display = display_get_egl_display(wscr->display);
	if (!wscr->egl.display) {
		fprintf(stderr, "init: no EGL display\n");
		return -1;
	}

	eglBindAPI(EGL_OPENGL_API);
	wscr->egl.config = display_get_argb_egl_config(wscr->display);

	if (demo_mode) {
		struct wl_output *o =
			output_get_wl_output(display_get_output(display));
		/* only one instance */
		wscr->demomode =
			create_wscreensaver_instance(wscr, o, 400, 300);
		return 0;
	}

	display_set_output_configure_handler(display, handle_output_configure);

	return 0;
}

static void
global_handler(struct display *display, uint32_t name,
	       const char *interface, uint32_t version, void *data)
{
	struct wscreensaver *screensaver = data;

	if (!strcmp(interface, "screensaver")) {
		screensaver->interface =
			display_bind(display, name, &screensaver_interface, 1);
	}
}

static const struct weston_option wscreensaver_options[] = {
	{ WESTON_OPTION_BOOLEAN, "demo", 0, &demo_mode },
};

int main(int argc, char *argv[])
{
	struct display *d;
	struct wscreensaver screensaver = { 0 };

	init_frand();

	if (parse_options(wscreensaver_options,
			ARRAY_LENGTH(wscreensaver_options), &argc, argv) > 1) {
		printf("Usage: %s [OPTIONS]\n  --demo for demo mode\n",
		       argv[0]);
		exit(1);
	}

	d = display_create(&argc, argv);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return EXIT_FAILURE;
	}

	if (!demo_mode) {
		/* iterates already known globals immediately */
		display_set_user_data(d, &screensaver);
		display_set_global_handler(d, global_handler);
		if (!screensaver.interface) {
			fprintf(stderr,
				"Server did not offer screensaver interface,"
				" exiting.\n");
			return EXIT_FAILURE;
		}
	}

	if (init_wscreensaver(&screensaver, d) < 0) {
		fprintf(stderr, "wscreensaver init failed.\n");
		return EXIT_FAILURE;
	}

	display_run(d);

	free((void *)progname);

	return EXIT_SUCCESS;
}
