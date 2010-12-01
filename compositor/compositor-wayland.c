/*
 * Copyright © 2010 Benjamin Franzke
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "wayland-client.h"

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "compositor.h"

struct wayland_compositor {
	struct wlsc_compositor	 base;

	struct {
		struct wl_display *display;
		struct wl_compositor *compositor;
		struct wl_shell *shell;
		struct wl_drm *drm;
		struct wl_output *output;

		struct {
			int32_t x, y, width, height;
		} screen_allocation;

		char *device_name;
		int authenticated;

		struct wl_event_source *wl_source;
		uint32_t event_mask;
	} parent;

	struct wl_list window_list;
	struct wl_list input_list;
};

struct wayland_output {
	struct wlsc_output	base;

	struct {
		struct wl_surface	*surface;
		struct wl_buffer	*buffer[2];
	} parent;
	EGLImageKHR		image[2];
	GLuint			rbo[2];
	uint32_t		fb_id[2];
	uint32_t		current;
};

struct wayland_input {
	struct wayland_compositor *compositor;
	struct wl_input_device *input_device;
	struct wl_list link;
};

static int
wayland_input_create(struct wayland_compositor *c)
{
	struct wlsc_input_device *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return -1;

	memset(input, 0, sizeof *input);
	wlsc_input_device_init(input, &c->base);

	c->base.input_device = input;

	return 0;
}

static int
wayland_compositor_init_egl(struct wayland_compositor *c)
{
	EGLint major, minor;
	const char *extensions;
	drm_magic_t magic;
	int fd;
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	fd = open(c->parent.device_name, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return -1;
	}

	wlsc_drm_init(&c->base, fd, c->parent.device_name);

	if (drmGetMagic(fd, &magic)) {
		fprintf(stderr, "DRI2: failed to get drm magic");
		return -1;
	}

	wl_drm_authenticate(c->parent.drm, magic);
	wl_display_iterate(c->parent.display, WL_DISPLAY_WRITABLE);
	while (!c->parent.authenticated)
		wl_display_iterate(c->parent.display, WL_DISPLAY_READABLE);

	c->base.display = eglGetDRMDisplayMESA(fd);
	if (c->base.display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return -1;
	}

	if (!eglInitialize(c->base.display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return -1;
	}

	extensions = eglQueryString(c->base.display, EGL_EXTENSIONS);
	if (!strstr(extensions, "EGL_KHR_surfaceless_opengl")) {
		fprintf(stderr, "EGL_KHR_surfaceless_opengl not available\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind EGL_OPENGL_ES_API\n");
		return -1;
	}

	c->base.context = eglCreateContext(c->base.display, NULL,
					   EGL_NO_CONTEXT, context_attribs);
	if (c->base.context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return -1;
	}

	if (!eglMakeCurrent(c->base.display, EGL_NO_SURFACE,
			    EGL_NO_SURFACE, c->base.context)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}

	return 0;
}

static void
frame_callback(void *data, uint32_t time)
{
	struct wayland_compositor *c = (struct wayland_compositor *) data;

	wlsc_compositor_finish_frame(&c->base, time);
}

static void
wayland_compositor_present(struct wlsc_compositor *base)
{
	struct wayland_compositor *c = (struct wayland_compositor *) base;
	struct wayland_output *output;

	glFlush();

	wl_list_for_each(output, &base->output_list, base.link) {
		output->current ^= 1;

		glFramebufferRenderbuffer(GL_FRAMEBUFFER,
					  GL_COLOR_ATTACHMENT0,
					  GL_RENDERBUFFER,
					  output->rbo[output->current]);

		wl_surface_attach(output->parent.surface,
				  output->parent.buffer[output->current ^ 1]);
		wl_surface_damage(output->parent.surface, 0, 0,
			          output->base.width, output->base.height);
	}

	wl_display_frame_callback(c->parent.display, frame_callback, c);
}

static int
wayland_authenticate(struct wlsc_compositor *ec, uint32_t id)
{
	struct wayland_compositor *c = (struct wayland_compositor *) ec;

	wl_drm_authenticate(c->parent.drm, id);
	/* FIXME: recv drm_authenticated event from parent? */
	wl_display_iterate(c->parent.display, WL_DISPLAY_WRITABLE);

	return 0;
}

static int
wayland_compositor_create_output(struct wayland_compositor *c,
				 int width, int height)
{
	struct wayland_output *output;
	struct wl_visual *visual;
	int i;
	EGLint name, stride, attribs[] = {
		EGL_WIDTH,	0,
		EGL_HEIGHT,	0,
		EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA,    EGL_DRM_BUFFER_USE_SCANOUT_MESA,
		EGL_NONE
	};

	output = malloc(sizeof *output);
	if (output == NULL)
		return -1;
	memset(output, 0, sizeof *output);

	wlsc_output_init(&output->base, &c->base, 0, 0, width, height);
	output->parent.surface =
		wl_compositor_create_surface(c->parent.compositor);
	wl_surface_set_user_data(output->parent.surface, output);

	glGenRenderbuffers(2, output->rbo);
	visual = wl_display_get_premultiplied_argb_visual(c->parent.display);
	for (i = 0; i < 2; i++) {
		glBindRenderbuffer(GL_RENDERBUFFER, output->rbo[i]);

		attribs[1] = width;
		attribs[3] = height;
		output->image[i] =
			eglCreateDRMImageMESA(c->base.display, attribs);
		glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
						       output->image[i]);
		eglExportDRMImageMESA(c->base.display, output->image[i],
				      &name, NULL, &stride);
		output->parent.buffer[i] =
			wl_drm_create_buffer(c->parent.drm, name,
					     width, height,
					     stride, visual);
	}

	output->current = 0;
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  output->rbo[output->current]);

	wl_surface_attach(output->parent.surface,
			  output->parent.buffer[output->current]);
	wl_surface_map(output->parent.surface,
		       output->base.x, output->base.y,
		       output->base.width, output->base.height);

	glClearColor(0, 0, 0, 0.5);

	wl_list_insert(c->base.output_list.prev, &output->base.link);

	return 0;
}

static struct wl_buffer *
create_invisible_pointer(struct wayland_compositor *c)
{
	struct wlsc_drm_buffer *wlsc_buffer;
	struct wl_buffer *buffer;
	int name, stride;
	struct wl_visual *visual;
	GLuint texture;
	const int width = 1, height = 1;
	const GLubyte data[] = { 0, 0, 0, 0 };

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	visual = wl_display_get_premultiplied_argb_visual(c->parent.display);
	wlsc_buffer = wlsc_drm_buffer_create(&c->base, width, height, visual);

	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, wlsc_buffer->image);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
			GL_RGBA, GL_UNSIGNED_BYTE, data);

	eglExportDRMImageMESA(c->base.display, wlsc_buffer->image,
			      &name, NULL, &stride);

	buffer = wl_drm_create_buffer(c->parent.drm, name,
				      width, height,
				      stride, visual);
	return buffer;
}

/* Events received from the wayland-server this compositor is client of: */

/* parent output interface */
static void
display_handle_geometry(void *data,
			struct wl_output *output,
			int32_t width, int32_t height)
{
	struct wayland_compositor *c = data;

	c->parent.screen_allocation.x = 0;
	c->parent.screen_allocation.y = 0;
	c->parent.screen_allocation.width = width;
	c->parent.screen_allocation.height = height;
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
};

/* parent shell interface */
static void
handle_configure(void *data, struct wl_shell *shell,
		 uint32_t time, uint32_t edges,
		 struct wl_surface *surface,
		 int32_t x, int32_t y, int32_t width, int32_t height)
{
#if 0
	struct output *output = wl_surface_get_user_data(surface);

	/* FIXME: add resize? */
#endif
}

static const struct wl_shell_listener shell_listener = {
	handle_configure,
};

/* parent drm interface */
static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
	struct wayland_compositor *c = data;

	c->parent.device_name = strdup(device);
}

static void drm_handle_authenticated(void *data, struct wl_drm *drm)
{
	struct wayland_compositor *c = data;

	c->parent.authenticated = 1;
}

static const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_authenticated
};

/* parent input interface */
static void
input_handle_motion(void *data, struct wl_input_device *input_device,
		     uint32_t time,
		     int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;

	notify_motion(c->base.input_device, time, sx, sy);
}

static void
input_handle_button(void *data,
		     struct wl_input_device *input_device,
		     uint32_t time, uint32_t button, uint32_t state)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;

	notify_button(c->base.input_device, time, button, state);
}

static void
input_handle_key(void *data, struct wl_input_device *input_device,
		  uint32_t time, uint32_t key, uint32_t state)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;

	notify_key(c->base.input_device, time, key, state);
}

static void
input_handle_pointer_focus(void *data,
			    struct wl_input_device *input_device,
			    uint32_t time, struct wl_surface *surface,
			    int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;
	static struct wl_buffer *pntr_buffer = NULL;

	if (surface) {
		c->base.focus = 1;
		/* FIXME: extend protocol to allow hiding the cursor? */
		if (pntr_buffer == NULL)
			pntr_buffer = create_invisible_pointer(c);
		wl_input_device_attach(input_device, time, pntr_buffer, x, y);
	} else {
		/* FIXME. hide our own pointer */
		c->base.focus = 0;
	}
}

static void
input_handle_keyboard_focus(void *data,
			     struct wl_input_device *input_device,
			     uint32_t time,
			     struct wl_surface *surface,
			     struct wl_array *keys)
{
	/* FIXME: sth to be done here? */
}

static const struct wl_input_device_listener input_device_listener = {
	input_handle_motion,
	input_handle_button,
	input_handle_key,
	input_handle_pointer_focus,
	input_handle_keyboard_focus,
};

static void
display_add_input(struct wayland_compositor *c, uint32_t id)
{
	struct wayland_input *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);

	input->compositor = c;
	input->input_device = wl_input_device_create(c->parent.display, id);
	wl_list_insert(c->input_list.prev, &input->link);

	wl_input_device_add_listener(input->input_device,
				     &input_device_listener, input);
	wl_input_device_set_user_data(input->input_device, input);
}

static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	struct wayland_compositor *c = data;

	if (strcmp(interface, "compositor") == 0) {
		c->parent.compositor = wl_compositor_create(display, id);
	} else if (strcmp(interface, "output") == 0) {
		c->parent.output = wl_output_create(display, id);
		wl_output_add_listener(c->parent.output, &output_listener, c);
	} else if (strcmp(interface, "input_device") == 0) {
		display_add_input(c, id);
	} else if (strcmp(interface, "shell") == 0) {
		c->parent.shell = wl_shell_create(display, id);
		wl_shell_add_listener(c->parent.shell, &shell_listener, c);
	} else if (strcmp(interface, "drm") == 0) {
		c->parent.drm = wl_drm_create(display, id);
		wl_drm_add_listener(c->parent.drm, &drm_listener, c);
	}
}

static int
update_event_mask(uint32_t mask, void *data)
{
	struct wayland_compositor *c = data;

	c->parent.event_mask = mask;
	if (c->parent.wl_source)
		wl_event_source_fd_update(c->parent.wl_source, mask);

	return 0;
}

static void
wayland_compositor_handle_event(int fd, uint32_t mask, void *data)
{
	struct wayland_compositor *c = data;

	if (mask & WL_EVENT_READABLE)
		wl_display_iterate(c->parent.display, WL_DISPLAY_READABLE);
	if (mask & WL_EVENT_WRITEABLE)
		wl_display_iterate(c->parent.display, WL_DISPLAY_WRITABLE);
}

struct wlsc_compositor *
wayland_compositor_create(struct wl_display *display, int width, int height)
{
	struct wayland_compositor *c;
	struct wl_event_loop *loop;
	int fd;
	char *socket_name;
	int socket_name_size;

	c = malloc(sizeof *c);
	if (c == NULL)
		return NULL;

	memset(c, 0, sizeof *c);

	socket_name_size = 1 + asprintf(&socket_name, "%c%s", '\0',
					getenv("WAYLAND_DISPLAY"));

	c->parent.display = wl_display_connect(socket_name, socket_name_size);
	free(socket_name);

	if (c->parent.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return NULL;
	}

	wl_list_init(&c->input_list);
	wl_display_add_global_listener(c->parent.display,
				display_handle_global, c);

	wl_display_iterate(c->parent.display, WL_DISPLAY_READABLE);

	c->base.wl_display = display;
	if (wayland_compositor_init_egl(c) < 0)
		return NULL;

	/* Can't init base class until we have a current egl context */
	if (wlsc_compositor_init(&c->base, display) < 0)
		return NULL;

	if (wayland_compositor_create_output(c, width, height) < 0)
		return NULL;

	if (wayland_input_create(c) < 0)
		return NULL;

	loop = wl_display_get_event_loop(c->base.wl_display);

	fd = wl_display_get_fd(c->parent.display, update_event_mask, c);
	c->parent.wl_source =
		wl_event_loop_add_fd(loop, fd, c->parent.event_mask,
				     wayland_compositor_handle_event, c);
	if (c->parent.wl_source == NULL)
		return NULL;

	c->base.authenticate = wayland_authenticate;
	c->base.present = wayland_compositor_present;

	return &c->base;
}
