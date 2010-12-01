/*
 * Copyright © 2010 Kristian Høgsberg
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compositor.h"

static void
drm_authenticate(struct wl_client *client,
		 struct wl_drm *drm_base, uint32_t id)
{
	struct wlsc_drm *drm = (struct wlsc_drm *) drm_base;
	struct wlsc_compositor *compositor =
		container_of(drm, struct wlsc_compositor, drm);

	if (compositor->authenticate(compositor, id) < 0)
		wl_client_post_event(client,
				     (struct wl_object *) compositor->wl_display,
				     WL_DISPLAY_INVALID_OBJECT, 0);
	else
		wl_client_post_event(client, &drm->object,
				     WL_DRM_AUTHENTICATED);
}

static void
destroy_buffer(struct wl_resource *resource, struct wl_client *client)
{
	struct wlsc_drm_buffer *buffer =
		container_of(resource, struct wlsc_drm_buffer, buffer.resource);
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) buffer->buffer.compositor;

	eglDestroyImageKHR(compositor->display, buffer->image);
	free(buffer);
}

static void
buffer_destroy(struct wl_client *client, struct wl_buffer *buffer)
{
	wl_resource_destroy(&buffer->resource, client);
}

const static struct wl_buffer_interface buffer_interface = {
	buffer_destroy
};

static void
drm_buffer_attach(struct wl_buffer *buffer_base, struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_drm_buffer *buffer =
		(struct wlsc_drm_buffer *) buffer_base;

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buffer->image);
	es->visual = buffer->buffer.visual;
}

static void
drm_buffer_damage(struct wl_buffer *buffer,
		  struct wl_surface *surface,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
}

static void
drm_create_buffer(struct wl_client *client, struct wl_drm *drm_base,
		  uint32_t id, uint32_t name, int32_t width, int32_t height,
		  uint32_t stride, struct wl_visual *visual)
{
	struct wlsc_drm *drm = (struct wlsc_drm *) drm_base;
	struct wlsc_compositor *compositor =
		container_of(drm, struct wlsc_compositor, drm);
	struct wlsc_drm_buffer *buffer;
	EGLint attribs[] = {
		EGL_WIDTH,		0,
		EGL_HEIGHT,		0,
		EGL_DRM_BUFFER_STRIDE_MESA,	0,
		EGL_DRM_BUFFER_FORMAT_MESA,	EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_NONE
	};

	if (visual != &compositor->argb_visual &&
	    visual != &compositor->premultiplied_argb_visual &&
	    visual != &compositor->rgb_visual) {
		/* FIXME: Define a real exception event instead of
		 * abusing this one */
		wl_client_post_event(client,
				     (struct wl_object *) compositor->wl_display,
				     WL_DISPLAY_INVALID_OBJECT, 0);
		fprintf(stderr, "invalid visual in create_buffer\n");
		return;
	}

	buffer = malloc(sizeof *buffer);
	if (buffer == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	attribs[1] = width;
	attribs[3] = height;
	attribs[5] = stride / 4;

	buffer->buffer.compositor = &compositor->compositor;
	buffer->buffer.width = width;
	buffer->buffer.height = height;
	buffer->buffer.visual = visual;
	buffer->buffer.attach = drm_buffer_attach;
	buffer->buffer.damage = drm_buffer_damage;
	buffer->image = eglCreateImageKHR(compositor->display,
					  compositor->context,
					  EGL_DRM_BUFFER_MESA,
					  (EGLClientBuffer) name, attribs);
	if (buffer->image == NULL) {
		/* FIXME: Define a real exception event instead of
		 * abusing this one */
		free(buffer);
		wl_client_post_event(client,
				     (struct wl_object *) compositor->wl_display,
				     WL_DISPLAY_INVALID_OBJECT, 0);
		fprintf(stderr, "failed to create image for name %d\n", name);
		return;
	}

	buffer->buffer.resource.object.id = id;
	buffer->buffer.resource.object.interface = &wl_buffer_interface;
	buffer->buffer.resource.object.implementation = (void (**)(void))
		&buffer_interface;

	buffer->buffer.resource.destroy = destroy_buffer;

	wl_client_add_resource(client, &buffer->buffer.resource);
}

const static struct wl_drm_interface drm_interface = {
	drm_authenticate,
	drm_create_buffer
};

static void
post_drm_device(struct wl_client *client, struct wl_object *global)
{
	struct wlsc_drm *drm = container_of(global, struct wlsc_drm, object);

	wl_client_post_event(client, global, WL_DRM_DEVICE, drm->filename);
}

int
wlsc_drm_init(struct wlsc_compositor *ec, int fd, const char *filename)
{
	struct wlsc_drm *drm = &ec->drm;

	drm->fd = fd;
	drm->filename = strdup(filename);
	if (drm->filename == NULL)
		return -1;

	drm->object.interface = &wl_drm_interface;
	drm->object.implementation = (void (**)(void)) &drm_interface;
	wl_display_add_object(ec->wl_display, &drm->object);
	wl_display_add_global(ec->wl_display, &drm->object, post_drm_device);

	return 0;
}

struct wlsc_drm_buffer *
wlsc_drm_buffer_create(struct wlsc_compositor *ec,
		       int width, int height, struct wl_visual *visual)
{
	struct wlsc_drm_buffer *buffer;

	EGLint image_attribs[] = {
		EGL_WIDTH,		0,
		EGL_HEIGHT,		0,
		EGL_DRM_BUFFER_FORMAT_MESA,	EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA,	EGL_DRM_BUFFER_USE_SCANOUT_MESA,
		EGL_NONE
	};

	image_attribs[1] = width;
	image_attribs[3] = height;

	buffer = malloc(sizeof *buffer);
	if (buffer == NULL)
		return NULL;

	buffer->image =
		eglCreateDRMImageMESA(ec->display, image_attribs);
	if (buffer->image == NULL) {
		free(buffer);
		return NULL;
	}

	buffer->buffer.visual = visual;
	buffer->buffer.width = width;
	buffer->buffer.height = height;
	buffer->buffer.attach = drm_buffer_attach;
	buffer->buffer.damage = drm_buffer_damage;

	return buffer;
}
