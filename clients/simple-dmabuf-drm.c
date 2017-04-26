/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
 * Copyright © 2014 Collabora Ltd.
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
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdint.h>
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

#ifdef HAVE_LIBDRM_INTEL
#include <i915_drm.h>
#include <intel_bufmgr.h>
#elif HAVE_LIBDRM_FREEDRENO
#include <freedreno/freedreno_drmif.h>
#endif
#include <drm_fourcc.h>

#include <wayland-client.h>
#include "shared/zalloc.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"

struct buffer;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zxdg_shell_v6 *shell;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct zwp_linux_dmabuf_v1 *dmabuf;
	int xrgb8888_format_found;
	int req_dmabuf_immediate;
};

struct drm_device {
	int fd;
	char *name;

	int (*alloc_bo)(struct buffer *buf);
	void (*free_bo)(struct buffer *buf);
	int (*export_bo_to_prime)(struct buffer *buf);
	int (*map_bo)(struct buffer *buf);
	void (*unmap_bo)(struct buffer *buf);
};

struct buffer {
	struct wl_buffer *buffer;
	int busy;

	struct drm_device *dev;
	int drm_fd;

#ifdef HAVE_LIBDRM_INTEL
	drm_intel_bufmgr *bufmgr;
	drm_intel_bo *bo;
#elif HAVE_LIBDRM_FREEDRENO
	struct fd_device *fd_dev;
	struct fd_bo *bo;
#endif /* HAVE_LIBDRM_FREEDRENO */

	uint32_t gem_handle;
	int dmabuf_fd;
	uint8_t *mmap;

	int width;
	int height;
	int bpp;
	unsigned long stride;
};

#define NUM_BUFFERS 3

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct buffer buffers[NUM_BUFFERS];
	struct buffer *prev_buffer;
	struct wl_callback *callback;
	bool initialized;
	bool wait_for_configure;
};

static int running = 1;

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


#ifdef HAVE_LIBDRM_INTEL
static int
intel_alloc_bo(struct buffer *my_buf)
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
intel_free_bo(struct buffer *my_buf)
{
	drm_intel_bo_unreference(my_buf->bo);
}

static int
intel_map_bo(struct buffer *my_buf)
{
	if (drm_intel_gem_bo_map_gtt(my_buf->bo) != 0)
		return 0;

	my_buf->mmap = my_buf->bo->virtual;

	return 1;
}

static int
intel_bo_export_to_prime(struct buffer *buffer)
{
	return drm_intel_bo_gem_export_to_prime(buffer->bo, &buffer->dmabuf_fd);
}

static void
intel_unmap_bo(struct buffer *my_buf)
{
	drm_intel_gem_bo_unmap_gtt(my_buf->bo);
}
#elif HAVE_LIBDRM_FREEDRENO
#define ALIGN(v, a) ((v + a - 1) & ~(a - 1))

static
int fd_alloc_bo(struct buffer *buf)
{
	int flags = DRM_FREEDRENO_GEM_CACHE_WCOMBINE;
	int size = buf->width * buf->height * buf->bpp / 8;
	buf->fd_dev = fd_device_new(buf->drm_fd);

	buf->bo = fd_bo_new(buf->fd_dev, size, flags);

	if (!buf->bo)
		return 0;
	buf->stride = ALIGN(buf->width, 32) * buf->bpp / 8;
	return 1;
}

static
void fd_free_bo(struct buffer *buf)
{
	fd_bo_del(buf->bo);
}

static
int fd_bo_export_to_prime(struct buffer *buf)
{
	buf->dmabuf_fd = fd_bo_dmabuf(buf->bo);
	if (buf->dmabuf_fd > 0)
		return 0;

	return 1;
}

static
int fd_map_bo(struct buffer *buf)
{
	buf->mmap = fd_bo_map(buf->bo);

	if (buf->mmap != NULL)
		return 1;

	return 0;
}

static
void fd_unmap_bo(struct buffer *buf)
{
}
#endif

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
drm_device_destroy(struct buffer *buf)
{
#ifdef HAVE_LIBDRM_INTEL
	drm_intel_bufmgr_destroy(buf->bufmgr);
#elif HAVE_LIBDRM_FREEDRENO
	fd_device_del(buf->fd_dev);
#endif

	close(buf->drm_fd);
}

static int
drm_device_init(struct buffer *buf)
{
	struct drm_device *dev = calloc(1, sizeof(struct drm_device));

	drmVersionPtr version = drmGetVersion(buf->drm_fd);

	dev->fd = buf->drm_fd;
	dev->name = strdup(version->name);
	if (0) {
		/* nothing */
	}
#ifdef HAVE_LIBDRM_INTEL
	else if (!strcmp(dev->name, "i915")) {
		buf->bufmgr = drm_intel_bufmgr_gem_init(buf->drm_fd, 32);
		if (!buf->bufmgr)
			return 0;
		dev->alloc_bo = intel_alloc_bo;
		dev->free_bo = intel_free_bo;
		dev->export_bo_to_prime = intel_bo_export_to_prime;
		dev->map_bo = intel_map_bo;
		dev->unmap_bo = intel_unmap_bo;
	}
#elif HAVE_LIBDRM_FREEDRENO
	else if (!strcmp(dev->name, "msm")) {
		dev->alloc_bo = fd_alloc_bo;
		dev->free_bo = fd_free_bo;
		dev->export_bo_to_prime = fd_bo_export_to_prime;
		dev->map_bo = fd_map_bo;
		dev->unmap_bo = fd_unmap_bo;
	}
#endif
	else {
		fprintf(stderr, "Error: drm device %s unsupported.\n",
			dev->name);
		free(dev);
		return 0;
	}
	buf->dev = dev;
	return 1;
}

static int
drm_connect(struct buffer *my_buf)
{
	/* This won't work with card0 as we need to be authenticated; instead,
	 * boot with drm.rnodes=1 and use that. */
	my_buf->drm_fd = open("/dev/dri/renderD128", O_RDWR);
	if (my_buf->drm_fd < 0)
		return 0;

	return drm_device_init(my_buf);
}

static void
drm_shutdown(struct buffer *my_buf)
{
	drm_device_destroy(my_buf);
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
	struct drm_device *drm_dev;

	if (!drm_connect(buffer)) {
		fprintf(stderr, "drm_connect failed\n");
		goto error;
	}
	drm_dev = buffer->dev;

	buffer->width = width;
	buffer->height = height;
	buffer->bpp = 32; /* hardcoded XRGB8888 format */

	if (!drm_dev->alloc_bo(buffer)) {
		fprintf(stderr, "alloc_bo failed\n");
		goto error1;
	}

	if (!drm_dev->map_bo(buffer)) {
		fprintf(stderr, "map_bo failed\n");
		goto error2;
	}
	fill_content(buffer);
	drm_dev->unmap_bo(buffer);

	if (drm_dev->export_bo_to_prime(buffer) != 0) {
		fprintf(stderr, "gem_export_to_prime failed\n");
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
	if (display->req_dmabuf_immediate) {
		buffer->buffer = zwp_linux_buffer_params_v1_create_immed(params,
					  buffer->width,
					  buffer->height,
					  DRM_FORMAT_XRGB8888,
					  flags);
		wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
	}
	else
		zwp_linux_buffer_params_v1_create(params,
					  buffer->width,
					  buffer->height,
					  DRM_FORMAT_XRGB8888,
					  flags);

	return 0;

error2:
	drm_dev->free_bo(buffer);
error1:
	drm_shutdown(buffer);
error:
	return -1;
}

static void
xdg_surface_handle_configure(void *data, struct zxdg_surface_v6 *surface,
			     uint32_t serial)
{
	struct window *window = data;

	zxdg_surface_v6_ack_configure(surface, serial);

	if (window->initialized && window->wait_for_configure)
		redraw(window, NULL, 0);
	window->wait_for_configure = false;
}

static const struct zxdg_surface_v6_listener xdg_surface_listener = {
	xdg_surface_handle_configure,
};

static void
xdg_toplevel_handle_configure(void *data, struct zxdg_toplevel_v6 *toplevel,
			      int32_t width, int32_t height,
			      struct wl_array *states)
{
}

static void
xdg_toplevel_handle_close(void *data, struct zxdg_toplevel_v6 *xdg_toplevel)
{
	running = 0;
}

static const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
	xdg_toplevel_handle_configure,
	xdg_toplevel_handle_close,
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

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "simple-dmabuf");

		window->wait_for_configure = true;
		wl_surface_commit(window->surface);
	} else if (display->fshell) {
		zwp_fullscreen_shell_v1_present_surface(display->fshell,
							window->surface,
							ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_DEFAULT,
							NULL);
	} else {
		assert(0);
	}

	for (i = 0; i < NUM_BUFFERS; ++i) {
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
	struct drm_device* dev;
	int i;

	if (window->callback)
		wl_callback_destroy(window->callback);

	for (i = 0; i < NUM_BUFFERS; i++) {
		if (!window->buffers[i].buffer)
			continue;

		wl_buffer_destroy(window->buffers[i].buffer);
		dev = window->buffers[i].dev;
		dev->free_bo(&window->buffers[i]);
		close(window->buffers[i].dmabuf_fd);
		drm_shutdown(&window->buffers[i]);
	}

	if (window->xdg_toplevel)
		zxdg_toplevel_v6_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		zxdg_surface_v6_destroy(window->xdg_surface);
	wl_surface_destroy(window->surface);
	free(window);
}

static struct buffer *
window_next_buffer(struct window *window)
{
	int i;

	for (i = 0; i < NUM_BUFFERS; i++)
		if (!window->buffers[i].busy)
			return &window->buffers[i];

	return NULL;
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
			"All buffers busy at redraw(). Server bug?\n");
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
dmabuf_modifiers(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf,
		 uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo)
{
	struct display *d = data;

	if (format == DRM_FORMAT_XRGB8888)
		d->xrgb8888_format_found = 1;

	/* XXX: do something useful with modifiers */
}

static void
dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf, uint32_t format)
{
	/* XXX: will be deprecated. */
}

static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
	dmabuf_format,
	dmabuf_modifiers
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
	} else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0) {
		d->fshell = wl_registry_bind(registry,
					     id, &zwp_fullscreen_shell_v1_interface, 1);
	} else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
		d->dmabuf = wl_registry_bind(registry,
					     id, &zwp_linux_dmabuf_v1_interface,
					     d->req_dmabuf_immediate ? 2 : 1);
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
create_display(int is_immediate)
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
	display->req_dmabuf_immediate = is_immediate;

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
		zxdg_shell_v6_destroy(display->shell);

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
	int is_immediate = 0;
	int ret = 0;

	if (argc > 1) {
		if (!strcmp(argv[1], "immed")) {
			is_immediate = 1;
		}
		else {
			fprintf(stderr, "usage:\n\tsimple-dmabuf-intel [options]\n"
				"available options:\n\timmed: avoid dmabuf "
				"creation roundtrip and import immediately\n");
			return 1;
		}
	}

	display = create_display(is_immediate);
	window = create_window(display, 250, 250);
	if (!window)
		return 1;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* Here we retrieve the linux-dmabuf objects if executed without immed,
	 * or error */
	wl_display_roundtrip(display->display);

	if (!running)
		return 1;

	window->initialized = true;

	if (!window->wait_for_configure)
		redraw(window, NULL, 0);

	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "simple-dmabuf exiting\n");
	destroy_window(window);
	destroy_display(display);

	return 0;
}
