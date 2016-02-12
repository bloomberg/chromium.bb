/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
 * Copyright © 2014 Collabora Ltd.
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>

#include <xf86drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>
#include <drm_fourcc.h>

#include <wayland-client.h>
#include "shared/zalloc.h"
#include "xdg-shell-unstable-v5-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct xdg_shell *shell;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct zwp_linux_dmabuf_v1 *dmabuf;
	int xrgb8888_format_found;
};

struct buffer {
	struct wl_buffer *buffer;
	int busy;

	int drm_fd;

	drm_intel_bufmgr *bufmgr;
	drm_intel_bo *bo;

	uint32_t gem_handle;
	int dmabuf_fd;
	uint8_t *mmap;

	int width;
	int height;
	int bpp;
	unsigned long stride;
};

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct buffer buffers[2];
	struct buffer *prev_buffer;
	struct wl_callback *callback;
};

static int running = 1;

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
drm_connect(struct buffer *my_buf)
{
	/* This won't work with card0 as we need to be authenticated; instead,
	 * boot with drm.rnodes=1 and use that. */
	my_buf->drm_fd = open("/dev/dri/renderD128", O_RDWR);
	if (my_buf->drm_fd < 0)
		return 0;

	my_buf->bufmgr = drm_intel_bufmgr_gem_init(my_buf->drm_fd, 32);
	if (!my_buf->bufmgr)
		return 0;

	return 1;
}

static void
drm_shutdown(struct buffer *my_buf)
{
	drm_intel_bufmgr_destroy(my_buf->bufmgr);
	close(my_buf->drm_fd);
}

static int
alloc_bo(struct buffer *my_buf)
{
	/* XXX: try different tiling modes for testing FB modifiers. */
	uint32_t tiling = I915_TILING_NONE;

	assert(my_buf->bufmgr);

	my_buf->bo = drm_intel_bo_alloc_tiled(my_buf->bufmgr, "test",
					      my_buf->width, my_buf->height,
					      (my_buf->bpp / 8), &tiling,
					      &my_buf->stride, 0);

	printf("buffer allocated w %d, h %d, stride %lu, size %lu\n",
	       my_buf->width, my_buf->height, my_buf->stride, my_buf->bo->size);

	if (!my_buf->bo)
		return 0;

	if (tiling != I915_TILING_NONE)
		return 0;

	return 1;
}

static void
free_bo(struct buffer *my_buf)
{
	drm_intel_bo_unreference(my_buf->bo);
}

static int
map_bo(struct buffer *my_buf)
{
	if (drm_intel_gem_bo_map_gtt(my_buf->bo) != 0)
		return 0;

	my_buf->mmap = my_buf->bo->virtual;

	return 1;
}

static void
fill_content(struct buffer *my_buf)
{
	int x = 0, y = 0;
	uint32_t *pix;

	assert(my_buf->mmap);

	for (y = 0; y < my_buf->height; y++) {
		pix = (uint32_t *)(my_buf->mmap + y * my_buf->stride);
		for (x = 0; x < my_buf->width; x++) {
			*pix++ = (0xff << 24) | ((x % 256) << 16) |
			         ((y % 256) << 8) | 0xf0;
		}
	}
}

static void
unmap_bo(struct buffer *my_buf)
{
	drm_intel_gem_bo_unmap_gtt(my_buf->bo);
}

static void
create_succeeded(void *data,
		 struct zwp_linux_buffer_params_v1 *params,
		 struct wl_buffer *new_buffer)
{
	struct buffer *buffer = data;

	buffer->buffer = new_buffer;
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);

	zwp_linux_buffer_params_v1_destroy(params);
}

static void
create_failed(void *data, struct zwp_linux_buffer_params_v1 *params)
{
	struct buffer *buffer = data;

	buffer->buffer = NULL;
	running = 0;

	zwp_linux_buffer_params_v1_destroy(params);

	fprintf(stderr, "Error: zwp_linux_buffer_params.create failed.\n");
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
	create_succeeded,
	create_failed
};

static int
create_dmabuf_buffer(struct display *display, struct buffer *buffer,
		     int width, int height)
{
	struct zwp_linux_buffer_params_v1 *params;
	uint64_t modifier;
	uint32_t flags;

	if (!drm_connect(buffer)) {
		fprintf(stderr, "drm_connect failed\n");
		goto error;
	}

	buffer->width = width;
	buffer->height = height;
	buffer->bpp = 32; /* hardcoded XRGB8888 format */

	if (!alloc_bo(buffer)) {
		fprintf(stderr, "alloc_bo failed\n");
		goto error1;
	}

	if (!map_bo(buffer)) {
		fprintf(stderr, "map_bo failed\n");
		goto error2;
	}
	fill_content(buffer);
	unmap_bo(buffer);

	if (drm_intel_bo_gem_export_to_prime(buffer->bo, &buffer->dmabuf_fd) != 0) {
		fprintf(stderr, "drm_intel_bo_gem_export_to_prime failed\n");
		goto error2;
	}
	if (buffer->dmabuf_fd < 0) {
		fprintf(stderr, "error: dmabuf_fd < 0\n");
		goto error2;
	}

	/* We now have a dmabuf! It should contain 2x2 tiles (i.e. each tile
	 * is 256x256) of misc colours, and be mappable, either as ARGB8888, or
	 * XRGB8888. */
	modifier = 0;
	flags = 0;

	params = zwp_linux_dmabuf_v1_create_params(display->dmabuf);
	zwp_linux_buffer_params_v1_add(params,
				       buffer->dmabuf_fd,
				       0, /* plane_idx */
				       0, /* offset */
				       buffer->stride,
				       modifier >> 32,
				       modifier & 0xffffffff);
	zwp_linux_buffer_params_v1_add_listener(params, &params_listener, buffer);
	zwp_linux_buffer_params_v1_create(params,
					  buffer->width,
					  buffer->height,
					  DRM_FORMAT_XRGB8888,
					  flags);

	return 0;

error2:
	free_bo(buffer);
error1:
	drm_shutdown(buffer);
error:
	return -1;
}

static void
handle_configure(void *data, struct xdg_surface *surface,
		 int32_t width, int32_t height,
		 struct wl_array *states, uint32_t serial)
{
}

static void
handle_delete(void *data, struct xdg_surface *xdg_surface)
{
	running = 0;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_configure,
	handle_delete,
};

static struct window *
create_window(struct display *display, int width, int height)
{
	struct window *window;
	int i;
	int ret;

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
			xdg_shell_get_xdg_surface(display->shell,
						  window->surface);

		assert(window->xdg_surface);

		xdg_surface_add_listener(window->xdg_surface,
					 &xdg_surface_listener, window);

		xdg_surface_set_title(window->xdg_surface, "simple-dmabuf");
	} else if (display->fshell) {
		zwp_fullscreen_shell_v1_present_surface(display->fshell,
							window->surface,
							ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_DEFAULT,
							NULL);
	} else {
		assert(0);
	}

	for (i = 0; i < 2; ++i) {
		ret = create_dmabuf_buffer(display, &window->buffers[i],
		                               width, height);

		if (ret < 0)
			return NULL;
	}

	return window;
}

static void
destroy_window(struct window *window)
{
	int i;

	if (window->callback)
		wl_callback_destroy(window->callback);

	for (i = 0; i < 2; i++) {
		if (!window->buffers[i].buffer)
			continue;

		wl_buffer_destroy(window->buffers[i].buffer);
		free_bo(&window->buffers[i]);
		close(window->buffers[i].dmabuf_fd);
		drm_shutdown(&window->buffers[i]);
	}

	if (window->xdg_surface)
		xdg_surface_destroy(window->xdg_surface);
	wl_surface_destroy(window->surface);
	free(window);
}

static struct buffer *
window_next_buffer(struct window *window)
{
	struct buffer *buffer;

	if (!window->buffers[0].busy)
		buffer = &window->buffers[0];
	else if (!window->buffers[1].busy)
		buffer = &window->buffers[1];
	else
		return NULL;

	return buffer;
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

	/* XXX: would be nice to draw something that changes here... */

	wl_surface_attach(window->surface, buffer->buffer, 0, 0);
	wl_surface_damage(window->surface, 0, 0, window->width, window->height);

	if (callback)
		wl_callback_destroy(callback);

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);
	wl_surface_commit(window->surface);
	buffer->busy = 1;
}

static const struct wl_callback_listener frame_listener = {
	redraw
};

static void
dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf, uint32_t format)
{
	struct display *d = data;

	if (format == DRM_FORMAT_XRGB8888)
		d->xrgb8888_format_found = 1;
}

static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
	dmabuf_format
};

static void
xdg_shell_ping(void *data, struct xdg_shell *shell, uint32_t serial)
{
	xdg_shell_pong(shell, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
	xdg_shell_ping,
};

#define XDG_VERSION 5 /* The version of xdg-shell that we implement */
#ifdef static_assert
static_assert(XDG_VERSION == XDG_SHELL_VERSION_CURRENT,
	      "Interface version doesn't match implementation version");
#endif

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "xdg_shell") == 0) {
		d->shell = wl_registry_bind(registry,
					    id, &xdg_shell_interface, 1);
		xdg_shell_use_unstable_version(d->shell, XDG_VERSION);
		xdg_shell_add_listener(d->shell, &xdg_shell_listener, d);
	} else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0) {
		d->fshell = wl_registry_bind(registry,
					     id, &zwp_fullscreen_shell_v1_interface, 1);
	} else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
		d->dmabuf = wl_registry_bind(registry,
					     id, &zwp_linux_dmabuf_v1_interface, 1);
		zwp_linux_dmabuf_v1_add_listener(d->dmabuf, &dmabuf_listener, d);
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

	/* XXX: fake, because the compositor does not yet advertise anything */
	display->xrgb8888_format_found = 1;

	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);
	wl_display_roundtrip(display->display);
	if (display->dmabuf == NULL) {
		fprintf(stderr, "No zwp_linux_dmabuf global\n");
		exit(1);
	}

	wl_display_roundtrip(display->display);

	if (!display->xrgb8888_format_found) {
		fprintf(stderr, "DRM_FORMAT_XRGB8888 not available\n");
		exit(1);
	}

	return display;
}

static void
destroy_display(struct display *display)
{
	if (display->dmabuf)
		zwp_linux_dmabuf_v1_destroy(display->dmabuf);

	if (display->shell)
		xdg_shell_destroy(display->shell);

	if (display->fshell)
		zwp_fullscreen_shell_v1_release(display->fshell);

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

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window *window;
	int ret = 0;

	display = create_display();
	window = create_window(display, 250, 250);
	if (!window)
		return 1;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* Here we retrieve the linux-dmabuf objects, or error */
	wl_display_roundtrip(display->display);

	if (!running)
		return 1;

	redraw(window, NULL, 0);

	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "simple-dmabuf exiting\n");
	destroy_window(window);
	destroy_display(display);

	return 0;
}
