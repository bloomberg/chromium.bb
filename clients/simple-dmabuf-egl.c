/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
 * Copyright © 2014,2018 Collabora Ltd.
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

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <gbm.h>

#include <wayland-client.h>
#include "shared/platform.h"
#include "shared/zalloc.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "shared/weston-egl-ext.h"

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

/* Possible options that affect the displayed image */
#define OPT_IMMEDIATE  1  /* create wl_buffer immediately */

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zxdg_shell_v6 *shell;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct zwp_linux_dmabuf_v1 *dmabuf;
	int xrgb8888_format_found;
	int req_dmabuf_immediate;
	struct {
		EGLDisplay display;
		EGLContext context;
		PFNEGLCREATEIMAGEKHRPROC create_image;
		PFNEGLDESTROYIMAGEKHRPROC destroy_image;
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
	} egl;
	struct {
		int drm_fd;
		struct gbm_device *device;
	} gbm;
};

struct buffer {
	struct display *display;
	struct wl_buffer *buffer;
	int busy;

	struct gbm_bo *bo;

	int dmabuf_fd;

	int width;
	int height;
	uint32_t stride;
	int format;

	EGLImageKHR egl_image;
	GLuint gl_texture;
	GLuint gl_fbo;
};

#define NUM_BUFFERS 3

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct buffer buffers[NUM_BUFFERS];
	struct wl_callback *callback;
	bool initialized;
	bool wait_for_configure;
};

static sig_atomic_t running = 1;

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

static void
buffer_free(struct buffer *buf)
{
	if (buf->gl_fbo)
		glDeleteFramebuffers(1, &buf->gl_fbo);

	if (buf->gl_texture)
		glDeleteTextures(1, &buf->gl_texture);

	if (buf->egl_image) {
		buf->display->egl.destroy_image(buf->display->egl.display,
						buf->egl_image);
	}

	if (buf->buffer)
		wl_buffer_destroy(buf->buffer);

	if (buf->bo)
		gbm_bo_destroy(buf->bo);

	if (buf->dmabuf_fd >= 0)
		close(buf->dmabuf_fd);
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

static bool
create_fbo_for_buffer(struct display *display, struct buffer *buffer)
{
	EGLint attribs[] = {
		EGL_WIDTH, buffer->width,
		EGL_HEIGHT, buffer->height,
		EGL_LINUX_DRM_FOURCC_EXT, buffer->format,
		EGL_DMA_BUF_PLANE0_FD_EXT, buffer->dmabuf_fd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, (int) buffer->stride,
		EGL_NONE
	};

	buffer->egl_image = display->egl.create_image(display->egl.display,
						      EGL_NO_CONTEXT,
						      EGL_LINUX_DMA_BUF_EXT,
						      NULL, attribs);
	if (buffer->egl_image == EGL_NO_IMAGE) {
		fprintf(stderr, "EGLImageKHR creation failed\n");
		return false;
	}

	eglMakeCurrent(display->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
			display->egl.context);

	glGenTextures(1, &buffer->gl_texture);
	glBindTexture(GL_TEXTURE_2D, buffer->gl_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	display->egl.image_target_texture_2d(GL_TEXTURE_2D, buffer->egl_image);

	glGenFramebuffers(1, &buffer->gl_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer->gl_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, buffer->gl_texture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "FBO creation failed\n");
		return false;
	}

	return true;
}


static int
create_dmabuf_buffer(struct display *display, struct buffer *buffer,
		     int width, int height, int format)
{
	static const uint32_t flags = 0;
	static const uint64_t modifier = DRM_FORMAT_MOD_INVALID;
	struct zwp_linux_buffer_params_v1 *params;

	buffer->display = display;
	buffer->width = width;
	buffer->height = height;
	buffer->format = format;

	buffer->bo = gbm_bo_create(display->gbm.device,
				   buffer->width, buffer->height,
				   format,
				   GBM_BO_USE_RENDERING);
	if (!buffer->bo) {
		fprintf(stderr, "create_bo failed\n");
		goto error;
	}

	buffer->stride = gbm_bo_get_stride(buffer->bo);
	buffer->dmabuf_fd = gbm_bo_get_fd(buffer->bo);

	if (buffer->dmabuf_fd < 0) {
		fprintf(stderr, "error: dmabuf_fd < 0\n");
		goto error;
	}

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
		buffer->buffer =
			zwp_linux_buffer_params_v1_create_immed(params,
								buffer->width,
								buffer->height,
								format,
								flags);
		wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
	}
	else {
		zwp_linux_buffer_params_v1_create(params,
						  buffer->width,
						  buffer->height,
						  format,
						  flags);
	}

	if (!create_fbo_for_buffer(display, buffer))
		goto error;

	return 0;

error:
	buffer_free(buffer);
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

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "simple-dmabuf-egl");

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
		                           width, height, DRM_FORMAT_XRGB8888);

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

	for (i = 0; i < NUM_BUFFERS; i++) {
		if (window->buffers[i].buffer)
			buffer_free(&window->buffers[i]);
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
	/* With a 60Hz redraw rate this completes a cycle in 3 seconds */
	static const int MAX_STEP = 180;
	static int step = 0;
	static int step_dir = 1;
	struct window *window = data;
	struct buffer *buffer;

	buffer = window_next_buffer(window);
	if (!buffer) {
		fprintf(stderr,
			!callback ? "Failed to create the first buffer.\n" :
			"All buffers busy at redraw(). Server bug?\n");
		abort();
	}

	/* Direct all GL draws to the buffer through the FBO */
	glBindFramebuffer(GL_FRAMEBUFFER, buffer->gl_fbo);

	/* Cycle between 0 and MAX_STEP */
	step += step_dir;
	if (step == 0 || step == MAX_STEP)
		step_dir = -step_dir;

	glClearColor(0.0,
		     (float) step / MAX_STEP,
		     1.0 - (float) step / MAX_STEP,
		     1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFinish();

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

	switch (format) {
	case DRM_FORMAT_XRGB8888:
		d->xrgb8888_format_found = 1;
		break;
	default:
		break;
	}
}

static void
dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf, uint32_t format)
{
	/* XXX: deprecated */
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
		if (version < 3)
			return;
		d->dmabuf = wl_registry_bind(registry,
					     id, &zwp_linux_dmabuf_v1_interface, 3);
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

static void
destroy_display(struct display *display)
{
	if (display->gbm.device)
		gbm_device_destroy(display->gbm.device);

	if (display->gbm.drm_fd >= 0)
		close(display->gbm.drm_fd);

	if (display->egl.context != EGL_NO_CONTEXT)
		eglDestroyContext(display->egl.display, display->egl.context);

	if (display->egl.display != EGL_NO_DISPLAY)
		eglTerminate(display->egl.display);

	if (display->dmabuf)
		zwp_linux_dmabuf_v1_destroy(display->dmabuf);

	if (display->shell)
		zxdg_shell_v6_destroy(display->shell);

	if (display->fshell)
		zwp_fullscreen_shell_v1_release(display->fshell);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	if (display->registry)
		wl_registry_destroy(display->registry);

	if (display->display) {
		wl_display_flush(display->display);
		wl_display_disconnect(display->display);
	}

	free(display);
}

static bool
display_set_up_egl(struct display *display)
{
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	EGLint major, minor;
	const char *egl_extensions = NULL;
	const char *gl_extensions = NULL;

	display->egl.display =
		weston_platform_get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
						display->display, NULL);
	if (display->egl.display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Failed to create EGLDisplay\n");
		goto error;
	}

	if (eglInitialize(display->egl.display, &major, &minor) == EGL_FALSE) {
		fprintf(stderr, "Failed to initialize EGLDisplay\n");
		goto error;
	}

	if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
		fprintf(stderr, "Failed to bind OpenGL ES API\n");
		goto error;
	}

	egl_extensions = eglQueryString(display->egl.display, EGL_EXTENSIONS);
	assert(egl_extensions != NULL);

	if (!weston_check_egl_extension(egl_extensions,
					"EGL_EXT_image_dma_buf_import")) {
		fprintf(stderr, "EGL_EXT_image_dma_buf_import not supported\n");
		goto error;
	}

	if (!weston_check_egl_extension(egl_extensions,
					"EGL_KHR_surfaceless_context")) {
		fprintf(stderr, "EGL_KHR_surfaceless_context not supported\n");
		goto error;
	}

	if (!weston_check_egl_extension(egl_extensions,
					"EGL_KHR_no_config_context")) {
		fprintf(stderr, "EGL_KHR_no_config_context not supported\n");
		goto error;
	}

	display->egl.context = eglCreateContext(display->egl.display,
						EGL_NO_CONFIG_KHR,
						EGL_NO_CONTEXT,
						context_attribs);
	if (display->egl.context == EGL_NO_CONTEXT) {
		fprintf(stderr, "Failed to create EGLContext\n");
		goto error;
	}

	eglMakeCurrent(display->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       display->egl.context);

	gl_extensions = (const char *) glGetString(GL_EXTENSIONS);
	assert(gl_extensions != NULL);

	if (!weston_check_egl_extension(gl_extensions,
					"GL_OES_EGL_image")) {
		fprintf(stderr, "GL_OES_EGL_image not suported\n");
		goto error;
	}

	display->egl.create_image =
		(void *) eglGetProcAddress("eglCreateImageKHR");
	assert(display->egl.create_image);

	display->egl.destroy_image =
		(void *) eglGetProcAddress("eglDestroyImageKHR");
	assert(display->egl.destroy_image);

	display->egl.image_target_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	assert(display->egl.image_target_texture_2d);

	return true;

error:
	return false;
}

static bool
display_set_up_gbm(struct display *display, char const* drm_render_node)
{
	display->gbm.drm_fd = open(drm_render_node, O_RDWR);
	if (display->gbm.drm_fd < 0) {
		fprintf(stderr, "Failed to open drm render node %s\n",
			drm_render_node);
		return false;
	}

	display->gbm.device = gbm_create_device(display->gbm.drm_fd);
	if (display->gbm.device == NULL) {
		fprintf(stderr, "Failed to create gbm device\n");
		return false;
	}

	return true;
}

static struct display *
create_display(char const *drm_render_node, int opts)
{
	struct display *display = NULL;

	display = zalloc(sizeof *display);
	if (display == NULL) {
		fprintf(stderr, "out of memory\n");
		goto error;
	}

	display->gbm.drm_fd = -1;

	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->req_dmabuf_immediate = opts & OPT_IMMEDIATE;

	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);
	wl_display_roundtrip(display->display);
	if (display->dmabuf == NULL) {
		fprintf(stderr, "No zwp_linux_dmabuf global\n");
		goto error;
	}

	wl_display_roundtrip(display->display);

	if (!display->xrgb8888_format_found) {
		fprintf(stderr, "format XRGB8888 is not available\n");
		goto error;
	}

	if (!display_set_up_egl(display))
		goto error;

	if (!display_set_up_gbm(display, drm_render_node))
		goto error;

	return display;

error:
	if (display != NULL)
		destroy_display(display);
	return NULL;
}

static void
signal_int(int signum)
{
	running = 0;
}

static void
print_usage_and_exit(void)
{
	printf("usage flags:\n"
		"\t'-i,--import-immediate=<>'"
		"\n\t\t0 to import dmabuf via roundtrip, "
		"\n\t\t1 to enable import without roundtrip\n"
		"\t'-d,--drm-render-node=<>'"
		"\n\t\tthe full path to the drm render node to use\n");
	exit(0);
}

static int
is_true(const char* c)
{
	if (!strcmp(c, "1"))
		return 1;
	else if (!strcmp(c, "0"))
		return 0;
	else
		print_usage_and_exit();

	return 0;
}

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window *window;
	int opts = 0;
	char const *drm_render_node = "/dev/dri/renderD128";
	int c, option_index, ret = 0;

	static struct option long_options[] = {
		{"import-immediate", required_argument, 0,  'i' },
		{"drm-render-node",  required_argument, 0,  'd' },
		{"help",             no_argument      , 0,  'h' },
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "hi:d:",
				  long_options, &option_index)) != -1) {
		switch (c) {
		case 'i':
			if (is_true(optarg))
				opts |= OPT_IMMEDIATE;
			break;
		case 'd':
			drm_render_node = optarg;
			break;
		default:
			print_usage_and_exit();
		}
	}

	display = create_display(drm_render_node, opts);
	if (!display)
		return 1;
	window = create_window(display, 256, 256);
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

	fprintf(stderr, "simple-dmabuf-egl exiting\n");
	destroy_window(window);
	destroy_display(display);

	return 0;
}
