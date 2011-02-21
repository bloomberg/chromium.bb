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

struct wlsc_drm_buffer {
	struct wl_buffer buffer;
	EGLImageKHR image;
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

static struct wlsc_drm_buffer *
wlsc_drm_buffer_create_for_image(struct wlsc_compositor *compositor,
				 EGLImageKHR *image,
				 int32_t width, int32_t height,
				 struct wl_visual *visual)
{
	struct wlsc_drm_buffer *buffer;

	buffer = malloc(sizeof *buffer);
	if (buffer == NULL)
		return NULL;

	buffer->buffer.compositor = &compositor->compositor;
	buffer->buffer.width = width;
	buffer->buffer.height = height;
	buffer->buffer.visual = visual;
	buffer->buffer.attach = drm_buffer_attach;
	buffer->buffer.damage = drm_buffer_damage;
	buffer->image = image;

	return buffer;
}

struct wl_buffer *
wlsc_drm_buffer_create(struct wlsc_compositor *ec, int width, int height,
		       struct wl_visual *visual, const void *data)
{
	struct wlsc_drm_buffer *buffer;
	EGLImageKHR image;
	GLuint texture;

	EGLint image_attribs[] = {
		EGL_WIDTH,		0,
		EGL_HEIGHT,		0,
		EGL_DRM_BUFFER_FORMAT_MESA,	EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA,	EGL_DRM_BUFFER_USE_SCANOUT_MESA,
		EGL_NONE
	};

	image_attribs[1] = width;
	image_attribs[3] = height;

	image = eglCreateDRMImageMESA(ec->display, image_attribs);
	if (image == NULL)
		return NULL;

	buffer = wlsc_drm_buffer_create_for_image(ec, image,
						  width, height, visual);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buffer->image);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
			GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);

	glDeleteTextures(1, &texture);

	return &buffer->buffer;
}
