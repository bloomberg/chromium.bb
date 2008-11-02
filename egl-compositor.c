#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>

#include "wayland.h"

#include <GL/gl.h>
#include <eagle.h>

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct egl_compositor {
	struct wl_compositor base;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	struct wl_display *wl_display;
	int gem_fd;
};

struct surface_data {
	uint32_t handle;
	int32_t width, height, stride;
	GLuint texture;
	struct wl_map map;
};

static void
repaint(void *data)
{
	struct egl_compositor *ec = data;
	struct wl_surface_iterator *iterator;
	struct wl_surface *surface;
	struct surface_data *sd;
	GLint vertices[12];
	GLint tex_coords[12] = { 0, 0,  0, 1,  1, 0,  1, 1 };
	GLuint indices[4] = { 0, 1, 2, 3 };
	       
	/* This part is where we actually copy the buffer to screen.
	 * Needs to be part of the repaint loop, not called from the
	 * notify_map handler. */

	glClear(GL_COLOR_BUFFER_BIT); 

	iterator = wl_surface_iterator_create(ec->wl_display, 0);
	while (wl_surface_iterator_next(iterator, &surface)) {
		sd = wl_surface_get_data(surface);
		if (sd == NULL)
			continue;

		vertices[0] = sd->map.x;
		vertices[1] = sd->map.y;
		vertices[2] = 0;

		vertices[3] = sd->map.x;
		vertices[4] = sd->map.y + sd->map.height;
		vertices[5] = 0;

		vertices[6] = sd->map.x + sd->map.width;
		vertices[7] = sd->map.y;
		vertices[8] = 0;

		vertices[9] = sd->map.x + sd->map.width;
		vertices[10] = sd->map.y + sd->map.height;
		vertices[11] = 0;

		glBindTexture(GL_TEXTURE_2D, sd->texture);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(3, GL_INT, 0, vertices);
		glTexCoordPointer(2, GL_INT, 0, tex_coords);
		glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, indices);
	}
	wl_surface_iterator_destroy(iterator);

	glFlush();

	eglSwapBuffers(ec->display, ec->surface);
}

static void schedule_repaint(struct egl_compositor *ec)
{
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(ec->wl_display);
	wl_event_loop_add_idle(loop, repaint, ec);
}

void notify_surface_create(struct wl_compositor *compositor,
			   struct wl_surface *surface)
{
	struct surface_data *sd;

	sd = malloc(sizeof *sd);
	if (sd == NULL)
		return;

	sd->handle = 0;
	wl_surface_set_data(surface, sd);

	glGenTextures(1, &sd->texture);
}
				   
void notify_surface_destroy(struct wl_compositor *compositor,
			    struct wl_surface *surface)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct surface_data *sd;
	struct drm_gem_close close_arg;
	int ret;

	sd = wl_surface_get_data(surface);
	if (sd == NULL || sd->handle == 0)
		return;
	
	close_arg.handle = sd->handle;
	ret = ioctl(ec->gem_fd, DRM_IOCTL_GEM_CLOSE, &close_arg);
	if (ret != 0) {
		fprintf(stderr, "failed to gem_close handle %d: %m\n", sd->handle);
	}

	glDeleteTextures(1, &sd->texture);

	free(sd);

	schedule_repaint(ec);
}

void notify_surface_attach(struct wl_compositor *compositor,
			   struct wl_surface *surface, uint32_t name, 
			   uint32_t width, uint32_t height, uint32_t stride)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct surface_data *sd;
	struct drm_gem_open open_arg;
	struct drm_gem_close close_arg;
	struct drm_i915_gem_pread pread;
	void *data;
	uint32_t size;
	int ret;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	if (sd->handle != 0) {
		close_arg.handle = sd->handle;
		ret = ioctl(ec->gem_fd, DRM_IOCTL_GEM_CLOSE, &close_arg);
		if (ret != 0) {
			fprintf(stderr, "failed to gem_close name %d: %m\n", name);
		}
	}

	open_arg.name = name;
	ret = ioctl(ec->gem_fd, DRM_IOCTL_GEM_OPEN, &open_arg);
	if (ret != 0) {
		fprintf(stderr, "failed to gem_open name %d, fd=%d: %m\n", name, ec->gem_fd);
		return;
	}

	sd->handle = open_arg.handle;
	sd->width = width;
	sd->height = height;
	sd->stride = stride;

	size = sd->height * sd->stride;
	data = malloc(size);
	if (data == NULL) {
		fprintf(stderr, "swap buffers malloc failed\n");
		return;
	}

	pread.handle = sd->handle;
	pread.pad = 0;
	pread.offset = 0;
	pread.size = size;
	pread.data_ptr = (long) data;

	if (ioctl(ec->gem_fd, DRM_IOCTL_I915_GEM_PREAD, &pread)) {
		fprintf(stderr, "gem pread failed\n");
		return;
	}

	glBindTexture(GL_TEXTURE_2D, sd->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		     GL_BGRA, GL_UNSIGNED_BYTE, data);

	free(data);

	schedule_repaint(ec);
}

void notify_surface_map(struct wl_compositor *compositor,
			struct wl_surface *surface, struct wl_map *map)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct surface_data *sd;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	sd->map = *map;

	schedule_repaint(ec);
}

struct wl_compositor_interface interface = {
	notify_surface_create,
	notify_surface_destroy,
	notify_surface_attach,
	notify_surface_map
};

static const char gem_device[] = "/dev/dri/card0";

struct wl_compositor *
wl_compositor_create(struct wl_display *display)
{
	EGLConfig configs[64];
	EGLint major, minor, count;
	struct egl_compositor *ec;
	const int width = 1024, height = 768;

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	ec->base.interface = &interface;
	ec->wl_display = display;

	ec->display = eglCreateDisplayNative(gem_device, "i965");
	if (ec->display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return NULL;
	}

	if (!eglInitialize(ec->display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return NULL;
	}

	if (!eglGetConfigs(ec->display, configs, ARRAY_LENGTH(configs), &count)) {
		fprintf(stderr, "failed to get configs\n");
		return NULL;
	}

	ec->surface = eglCreateSurfaceNative(ec->display, configs[24],
					     0, 0, width, height);
	if (ec->surface == NULL) {
		fprintf(stderr, "failed to create surface\n");
		return NULL;
	}

	ec->context = eglCreateContext(ec->display, configs[24], NULL, NULL);
	if (ec->context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return NULL;
	}

	if (!eglMakeCurrent(ec->display, ec->surface, ec->surface, ec->context)) {
		fprintf(stderr, "failed to make context current\n");
		return NULL;
	}

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, 0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glClearColor(0.0, 0.05, 0.2, 0.0);

	ec->gem_fd = open(gem_device, O_RDWR);
	if (ec->gem_fd < 0) {
		fprintf(stderr, "failed to open drm device\n");
		return NULL;
	}

	schedule_repaint(ec);

	return &ec->base;
}
