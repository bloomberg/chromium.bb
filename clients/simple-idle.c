/*
 * Copyright © 2016 Samsung Electronics Co., Ltd
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
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

/**
 * \file simple-idle.c
 * \brief Demonstrate the use of Wayland's idle behavior inhibition protocol
 *
 * \author Bryce Harrington
 */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <wayland-client.h>
#include "shared/config-parser.h"
#include "shared/helpers.h"
#include "shared/os-compatibility.h"
#include "shared/zalloc.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"


/** display help message */
static bool opt_help = false;

/** print extra output to stdout */
static bool opt_verbose = false;

/** Delay this many milliseconds when the inhibitor is requested */
static int opt_inhibitor_creation_delay = 0;

/** if non-negative, destroy the inhibitor after this many milliseconds have elapsed since program start */
static int opt_inhibitor_destruction_delay = -1;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zxdg_shell_v6 *shell;
	struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
	struct wl_shm *shm;
	bool has_xrgb;
};

struct buffer {
	struct wl_buffer *buffer;
	void *shm_data;
	int busy;
};

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct buffer buffers[2];
	struct buffer *prev_buffer;
	struct wl_callback *callback;
	bool wait_for_configure;
	struct zwp_idle_inhibitor_v1 *inhibitor;
};

static int running = 1;

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time);

static void
redraw(void *data, struct wl_callback *callback, uint32_t time);

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct buffer *mybuf = data;

	mybuf->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static int
create_shm_buffer(struct display *display, struct buffer *buffer,
		  int width, int height, uint32_t format)
{
	struct wl_shm_pool *pool;
	int fd, size, stride;
	void *data;

	stride = width * 4;
	size = stride * height;

	fd = os_create_anonymous_file(size);
	if (fd < 0) {
		fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
			size);
		return -1;
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		return -1;
	}

	pool = wl_shm_create_pool(display->shm, fd, size);
	buffer->buffer = wl_shm_pool_create_buffer(pool, 0,
						   width, height,
						   stride, format);
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
	wl_shm_pool_destroy(pool);
	close(fd);

	buffer->shm_data = data;

	return 0;
}

static void
handle_xdg_surface_configure(void *data, struct zxdg_surface_v6 *surface,
			     uint32_t serial)
{
	struct window *window = data;

	zxdg_surface_v6_ack_configure(surface, serial);

	if (window->wait_for_configure) {
		redraw(window, NULL, 0);
		window->wait_for_configure = false;
	}
}

static const struct zxdg_surface_v6_listener xdg_surface_listener = {
	handle_xdg_surface_configure,
};

static void
handle_xdg_toplevel_configure(void *data, struct zxdg_toplevel_v6 *xdg_toplevel,
			      int32_t width, int32_t height,
			      struct wl_array *state)
{
}

static void
handle_xdg_toplevel_close(void *data, struct zxdg_toplevel_v6 *xdg_toplevel)
{
	running = 0;
}

static const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
	handle_xdg_toplevel_configure,
	handle_xdg_toplevel_close,
};

static struct window *
create_window(struct display *display, int width, int height)
{
	struct window *window;

	window = zalloc(sizeof *window);
	if (!window)
		return NULL;

	window->callback = NULL;
	window->display = display;
	window->width = width;
	window->height = height;
	window->surface = wl_compositor_create_surface(display->compositor);

	if (display->shell) {
		window->xdg_surface =
			zxdg_shell_v6_get_xdg_surface(display->shell,
						  window->surface);
		assert(window->xdg_surface);
		zxdg_surface_v6_add_listener(window->xdg_surface,
					     &xdg_surface_listener, window);

		window->xdg_toplevel =
			zxdg_surface_v6_get_toplevel(window->xdg_surface);
		assert(window->xdg_toplevel);
		zxdg_toplevel_v6_add_listener(window->xdg_toplevel,
					      &xdg_toplevel_listener, window);

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "simple-shm");
		wl_surface_commit(window->surface);
		window->wait_for_configure = true;
	} else {
		assert(0);
	}

	return window;
}

static void
destroy_window(struct window *window)
{
	if (window->callback)
		wl_callback_destroy(window->callback);

	if (window->buffers[0].buffer)
		wl_buffer_destroy(window->buffers[0].buffer);
	if (window->buffers[1].buffer)
		wl_buffer_destroy(window->buffers[1].buffer);

	if (window->xdg_toplevel)
		zxdg_toplevel_v6_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		zxdg_surface_v6_destroy(window->xdg_surface);

	if (window->inhibitor)
		zwp_idle_inhibitor_v1_destroy(window->inhibitor);

	wl_surface_destroy(window->surface);
	free(window);
}

static struct buffer *
window_next_buffer(struct window *window)
{
	struct buffer *buffer;
	int ret = 0;

	if (!window->buffers[0].busy)
		buffer = &window->buffers[0];
	else if (!window->buffers[1].busy)
		buffer = &window->buffers[1];
	else
		return NULL;

	if (!buffer->buffer) {
		ret = create_shm_buffer(window->display, buffer,
					window->width, window->height,
					WL_SHM_FORMAT_XRGB8888);

		if (ret < 0)
			return NULL;

		/* paint the padding */
		memset(buffer->shm_data, 0x00,
		       window->width * window->height * 4);
	}

	return buffer;
}

static void
paint_pixels(void *image, int padding, int width, int height, uint32_t time)
{
	uint32_t *pixel = image;
	int y;
	int last_line_factor = padding/4;
	int tick = time/256;

	pixel += padding * width;
	for (y = padding; y < height - padding; y++) {
		int x;
		double a = 1.0 - (double)(y-padding) / (double)(height - 2*padding);
		int indent = padding + (int)(a * (width/2.0 - padding - last_line_factor));
		int line_height = 2 + 8*(1 - a);
		int fade = (int)(0xFF*(1-a));

		pixel += indent;
		for (x = indent; x < width - indent; x++) {
			if ((y + tick) % line_height > 0) {
				*pixel++ = 0xFF000000;
			} else {
				*pixel++ = 0x00010100 * fade;
			}
		}
		pixel += indent;
	}
	return;
}

static const struct wl_callback_listener frame_listener;

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	struct buffer *buffer;

	buffer = window_next_buffer(window);
	if (!buffer) {
		fprintf(stderr,
			!callback ? "Failed to create the first buffer.\n" :
			"Both buffers busy at redraw(). Server bug?\n");
		abort();
	}

	paint_pixels(buffer->shm_data, 40, window->width, window->height, time);

	wl_surface_attach(window->surface, buffer->buffer, 0, 0);
	wl_surface_damage(window->surface,
			  40, 40, window->width - 80, window->height - 80);

	if (callback)
		wl_callback_destroy(callback);

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);
	wl_surface_commit(window->surface);
	buffer->busy = 1;
}

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	static uint32_t start_time = 0;
	struct window *window = data;
	struct display *display = window->display;
	static bool destroy_done = false;

	if (start_time == 0)
		start_time = time;

	if (opt_verbose)
		printf("time: %u msec\n", time - start_time);

	/* Request the screensaver be inhibited, possibly after a user-specified delay */
	if (time > start_time + opt_inhibitor_creation_delay &&
	    window->inhibitor == NULL &&
	    display->idle_inhibit_manager &&
	    destroy_done == false) {
		if (opt_verbose)
			printf("Requesting inhibitor\n");
		window->inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
			display->idle_inhibit_manager,
			window->surface);
	}

	/* Destroy inhibitor if user has requested an early destruction */
	if (opt_inhibitor_destruction_delay > 0 &&
	    time > start_time + opt_inhibitor_destruction_delay &&
	    window->inhibitor &&
	    destroy_done == false) {
		if (opt_verbose)
			printf("Destroying inhibitor\n");
		zwp_idle_inhibitor_v1_destroy(window->inhibitor);
		window->inhibitor = NULL;
		destroy_done = true;
	}

	redraw(window, callback, time);
}

static const struct wl_callback_listener frame_listener = {
	frame_callback
};

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	struct display *d = data;

	if (format == WL_SHM_FORMAT_XRGB8888)
		d->has_xrgb = true;
}

struct wl_shm_listener shm_listener = {
	shm_format
};

static void
xdg_shell_ping(void *data, struct zxdg_shell_v6 *shell, uint32_t serial)
{
	zxdg_shell_v6_pong(shell, serial);
}

static const struct zxdg_shell_v6_listener xdg_shell_listener = {
	xdg_shell_ping,
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "zxdg_shell_v6") == 0) {
		d->shell = wl_registry_bind(registry,
					    id, &zxdg_shell_v6_interface, 1);
		zxdg_shell_v6_add_listener(d->shell, &xdg_shell_listener, d);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry,
					  id, &wl_shm_interface, 1);
		wl_shm_add_listener(d->shm, &shm_listener, d);
	} else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0) {
		d->idle_inhibit_manager = wl_registry_bind(registry,
							   id, &zwp_idle_inhibit_manager_v1_interface, 1);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static struct display *
create_display(void)
{
	struct display *display;

	display = malloc(sizeof *display);
	if (display == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->has_xrgb = false;
	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);
	wl_display_roundtrip(display->display);
	if (display->shm == NULL) {
		fprintf(stderr, "No wl_shm global\n");
		exit(1);
	}
	wl_display_roundtrip(display->display);

	if (!display->has_xrgb) {
		fprintf(stderr, "WL_SHM_FORMAT_XRGB32 not available\n");
		exit(1);
	}

	return display;
}

static void
destroy_display(struct display *display)
{
	if (display->idle_inhibit_manager)
		zwp_idle_inhibit_manager_v1_destroy(display->idle_inhibit_manager);

	if (display->shm)
		wl_shm_destroy(display->shm);

	if (display->shell)
		zxdg_shell_v6_destroy(display->shell);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	wl_registry_destroy(display->registry);
	wl_display_flush(display->display);
	wl_display_disconnect(display->display);
	free(display);
}

static void
signal_int(int signum)
{
	running = 0;
}

/**
 * \brief command line options for simple-idle
 */
static const struct weston_option simple_idle_options[] = {
        { WESTON_OPTION_BOOLEAN, "help", 'h', &opt_help },
        { WESTON_OPTION_BOOLEAN, "verbose", 'V', &opt_verbose },
	{ WESTON_OPTION_INTEGER, "inhibitor-creation-delay", 'c', &opt_inhibitor_creation_delay },
	{ WESTON_OPTION_INTEGER, "inhibitor-destruction-delay", 'd', &opt_inhibitor_destruction_delay },
};

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window *window;
	int ret = 0;

	ret = parse_options(simple_idle_options,
			    ARRAY_LENGTH(simple_idle_options), &argc, argv);
	if (ret > 1 || opt_help) {
                unsigned k;
                printf("Usage: %s [OPTIONS]\n\n", argv[0]);
                for (k = 0; k < ARRAY_LENGTH(simple_idle_options); k++) {
                        const struct weston_option* p = &simple_idle_options[k];
			if (p->short_name && p->name) {
                                if (p->type != WESTON_OPTION_BOOLEAN)
					printf("  -%c VALUE, --%s=VALUE\n", p->short_name, p->name);
				else
					printf("  -%c, --%s\n", p->short_name, p->name);
			} else if (p->short_name) {
                                printf("  -%c", p->short_name);
                                if (p->type != WESTON_OPTION_BOOLEAN)
                                        printf(" VALUE");
                                putchar('\n');
			} else if (p->name) {
                                printf("  --%s", p->name);
                                if (p->type != WESTON_OPTION_BOOLEAN)
                                        printf("=VALUE");
				putchar('\n');
                        }
                }
                return 1;
	}

	display = create_display();
	window = create_window(display, 250, 250);
	if (!window)
		return 1;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* Initialise damage to full surface, so the padding gets painted */
	wl_surface_damage(window->surface, 0, 0,
			  window->width, window->height);

	if (!window->wait_for_configure)
		redraw(window, NULL, 0);

	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "simple-idle exiting\n");

	destroy_window(window);
	destroy_display(display);

	return 0;
}
