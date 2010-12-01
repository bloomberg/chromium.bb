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
#include <sys/mman.h>
#include <unistd.h>

#include "compositor.h"

static void
destroy_buffer(struct wl_resource *resource, struct wl_client *client)
{
	struct wlsc_shm_buffer *buffer =
		container_of(resource, struct wlsc_shm_buffer, buffer.resource);

	munmap(buffer->data, buffer->stride * buffer->buffer.height);
	free(buffer);
}

static void
buffer_destroy(struct wl_client *client, struct wl_buffer *buffer)
{
	// wl_resource_destroy(&buffer->base, client);
}

const static struct wl_buffer_interface buffer_interface = {
	buffer_destroy
};

static void
shm_buffer_attach(struct wl_buffer *buffer_base, struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_shm_buffer *buffer =
		(struct wlsc_shm_buffer *) buffer_base;

	glBindTexture(GL_TEXTURE_2D, es->texture);

	/* Unbind any EGLImage texture that may be bound, so we don't
	 * overwrite it.*/
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     0, 0, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     buffer->buffer.width, buffer->buffer.height, 0,
		     GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer->data);
	es->visual = buffer->buffer.visual;
}

static void
shm_buffer_damage(struct wl_buffer *buffer_base,
		  struct wl_surface *surface,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_shm_buffer *buffer =
		(struct wlsc_shm_buffer *) buffer_base;

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     buffer->buffer.width, buffer->buffer.height, 0,
		     GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer->data);

	/* Hmm, should use glTexSubImage2D() here but GLES2 doesn't
	 * support any unpack attributes except GL_UNPACK_ALIGNMENT. */
}

static void
shm_create_buffer(struct wl_client *client, struct wl_shm *shm,
		  uint32_t id, int fd, int32_t width, int32_t height,
		  uint32_t stride, struct wl_visual *visual)
{
	struct wlsc_compositor *compositor =
		container_of((struct wlsc_shm *) shm,
			     struct wlsc_compositor, shm);
	struct wlsc_shm_buffer *buffer;

	if (visual != &compositor->argb_visual &&
	    visual != &compositor->premultiplied_argb_visual &&
	    visual != &compositor->rgb_visual) {
		/* FIXME: Define a real exception event instead of
		 * abusing this one */
		wl_client_post_event(client,
				     (struct wl_object *) compositor->wl_display,
				     WL_DISPLAY_INVALID_OBJECT, 0);
		fprintf(stderr, "invalid visual in create_buffer\n");
		close(fd);
		return;
	}

	buffer = malloc(sizeof *buffer);
	if (buffer == NULL) {
		close(fd);
		wl_client_post_no_memory(client);
		return;
	}

	buffer->buffer.compositor = (struct wl_compositor *) compositor;
	buffer->buffer.width = width;
	buffer->buffer.height = height;
	buffer->buffer.visual = visual;
	buffer->buffer.attach = shm_buffer_attach;
	buffer->buffer.damage = shm_buffer_damage;
	buffer->stride = stride;
	buffer->data =
		mmap(NULL, stride * height, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (buffer->data == MAP_FAILED) {
		/* FIXME: Define a real exception event instead of
		 * abusing this one */
		free(buffer);
		wl_client_post_event(client,
				     (struct wl_object *) compositor->wl_display,
				     WL_DISPLAY_INVALID_OBJECT, 0);
		fprintf(stderr, "failed to create image for fd %d\n", fd);
		return;
	}

	buffer->buffer.resource.object.id = id;
	buffer->buffer.resource.object.interface = &wl_buffer_interface;
	buffer->buffer.resource.object.implementation = (void (**)(void))
		&buffer_interface;

	buffer->buffer.resource.destroy = destroy_buffer;

	wl_client_add_resource(client, &buffer->buffer.resource);
}

const static struct wl_shm_interface shm_interface = {
	shm_create_buffer
};

int
wlsc_shm_init(struct wlsc_compositor *ec)
{
	struct wlsc_shm *shm = &ec->shm;

	shm->object.interface = &wl_shm_interface;
	shm->object.implementation = (void (**)(void)) &shm_interface;
	wl_display_add_object(ec->wl_display, &shm->object);
	wl_display_add_global(ec->wl_display, &shm->object, NULL);

	return 0;
}
