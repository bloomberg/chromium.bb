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

#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>

#include "wayland.h"
#include "wayland-system-compositor.h"

struct evdev_input_device {
	struct wlsc_input_device *device;
	struct wl_event_source *source;
	int tool, new_x, new_y;
	int base_x, base_y;
	int fd;
};

static void evdev_input_device_data(int fd, uint32_t mask, void *data)
{
	struct evdev_input_device *device = data;
	struct input_event ev[8], *e, *end;
	int len, value, dx, dy, absolute_event;
	int x, y;

	dx = 0;
	dy = 0;
	absolute_event = 0;
	wlsc_device_get_position(device->device, &x, &y);

	len = read(fd, &ev, sizeof ev);
	if (len < 0 || len % sizeof e[0] != 0) {
		/* FIXME: handle error... reopen device? */;
		return;
	}

	e = ev;
	end = (void *) ev + len;
	for (e = ev; e < end; e++) {
		/* Get the signed value, earlier kernels had this as unsigned */
		value = e->value;

		switch (e->type) {
		case EV_REL:
			switch (e->code) {
			case REL_X:
				dx += value;
				break;

			case REL_Y:
				dy += value;
				break;
			}
			break;

		case EV_ABS:
		        absolute_event = 1;
			switch (e->code) {
			case ABS_X:
				if (device->new_x) {
					device->base_x = x - value;
					device->new_x = 0;
				}
				x = device->base_x + value;
				break;
			case ABS_Y:
				if (device->new_y) {
					device->base_y = y - value;
					device->new_y = 0;
				}
				y = device->base_y + value;
				break;
			}
			break;

		case EV_KEY:
			if (value == 2)
				break;

			switch (e->code) {
			case BTN_TOUCH:
			case BTN_TOOL_PEN:
			case BTN_TOOL_RUBBER:
			case BTN_TOOL_BRUSH:
			case BTN_TOOL_PENCIL:
			case BTN_TOOL_AIRBRUSH:
			case BTN_TOOL_FINGER:
			case BTN_TOOL_MOUSE:
			case BTN_TOOL_LENS:
				if (device->tool == 0 && value) {
					device->new_x = 1;
					device->new_y = 1;
				}
				device->tool = value ? e->code : 0;
				break;

			case BTN_LEFT:
			case BTN_RIGHT:
			case BTN_MIDDLE:
			case BTN_SIDE:
			case BTN_EXTRA:
			case BTN_FORWARD:
			case BTN_BACK:
			case BTN_TASK:
				notify_button(device->device, e->code, value);
				break;

			default:
				notify_key(device->device, e->code, value);
				break;
			}
		}
	}

	if (dx != 0 || dy != 0)
		notify_motion(device->device, x + dx, y + dy);
	if (absolute_event && device->tool)
		notify_motion(device->device, x, y);
}

struct evdev_input_device *
evdev_input_device_create(struct wlsc_input_device *master,
			  struct wl_display *display, const char *path)
{
	struct evdev_input_device *device;
	struct wl_event_loop *loop;

	device = malloc(sizeof *device);
	if (device == NULL)
		return NULL;

	device->tool = 1;
	device->new_x = 1;
	device->new_y = 1;
	device->device = master;

	device->fd = open(path, O_RDONLY);
	if (device->fd < 0) {
		free(device);
		fprintf(stderr, "couldn't create pointer for %s: %m\n", path);
		return NULL;
	}

	loop = wl_display_get_event_loop(display);
	device->source = wl_event_loop_add_fd(loop, device->fd,
					      WL_EVENT_READABLE,
					      evdev_input_device_data, device);
	if (device->source == NULL) {
		close(device->fd);
		free(device);
		return NULL;
	}

	return device;
}
