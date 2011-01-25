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
#include <GLES2/gl2.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "compositor.h"
#include "screenshooter-server-protocol.h"

struct wl_screenshooter {
	struct wl_object base;
	struct wlsc_compositor *ec;
};

static void
screenshooter_shoot(struct wl_client *client, struct wl_screenshooter *shooter)
{
	struct wlsc_compositor *ec = shooter->ec;
	struct wlsc_output *output;
	char buffer[256];
	GdkPixbuf *pixbuf, *normal;
	GError *error = NULL;
	unsigned char *data;
	int i, j;

	i = 0;
	wl_list_for_each(output, &ec->output_list, link) {
		snprintf(buffer, sizeof buffer, "wayland-screenshot-%d.png", i++);
		data = malloc(output->width * output->height * 4);
		if (data == NULL) {
			fprintf(stderr, "couldn't allocate image buffer\n");
			continue;
		}

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, output->width, output->height,
			     GL_RGBA, GL_UNSIGNED_BYTE, data);

		/* FIXME: We should just use a RGB visual for the frontbuffer. */
		for (j = 3; j < output->width * output->height * 4; j += 4)
			data[j] = 0xff;

		pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE,
						  8, output->width, output->height, output->width * 4,
						  NULL, NULL);
		normal = gdk_pixbuf_flip(pixbuf, FALSE);
		gdk_pixbuf_save(normal, buffer, "png", &error, NULL);
		g_object_unref(normal);
		g_object_unref(pixbuf);
		free(data);
	}
}

struct wl_screenshooter_interface screenshooter_implementation = {
	screenshooter_shoot
};

void
screenshooter_create(struct wlsc_compositor *ec)
{
	struct wl_screenshooter *shooter;

	shooter = malloc(sizeof *shooter);
	if (shooter == NULL)
		return;

	shooter->base.interface = &wl_screenshooter_interface;
	shooter->base.implementation =
		(void(**)(void)) &screenshooter_implementation;
	shooter->ec = ec;

	wl_display_add_object(ec->wl_display, &shooter->base);
	wl_display_add_global(ec->wl_display, &shooter->base, NULL);
};
