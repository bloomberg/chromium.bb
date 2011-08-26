/*
 * Copyright © 2008 Kristian Høgsberg
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

#include <stdlib.h>

#include "compositor.h"
#include "screenshooter-server-protocol.h"

struct screenshooter {
	struct wl_object base;
	struct wlsc_compositor *ec;
};

static void
screenshooter_shoot(struct wl_client *client,
		    struct wl_resource *resource,
		    struct wl_resource *output_resource,
		    struct wl_resource *buffer_resource)
{
	struct wlsc_output *output = output_resource->data;
	struct wl_buffer *buffer = buffer_resource->data;

	if (!wl_buffer_is_shm(buffer))
		return;

	if (buffer->width < output->current->width ||
	    buffer->height < output->current->height)
		return;

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, output->current->width, output->current->height,
		     GL_RGBA, GL_UNSIGNED_BYTE,
		     wl_shm_buffer_get_data(buffer));
}

struct screenshooter_interface screenshooter_implementation = {
	screenshooter_shoot
};

static void
bind_shooter(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id)
{
	wl_client_add_object(client, &screenshooter_interface,
			     &screenshooter_implementation, id, data);
}

void
screenshooter_create(struct wlsc_compositor *ec)
{
	struct screenshooter *shooter;

	shooter = malloc(sizeof *shooter);
	if (shooter == NULL)
		return;

	shooter->base.interface = &screenshooter_interface;
	shooter->base.implementation =
		(void(**)(void)) &screenshooter_implementation;
	shooter->ec = ec;

	wl_display_add_global(ec->wl_display,
			      &screenshooter_interface, shooter, bind_shooter);
};
