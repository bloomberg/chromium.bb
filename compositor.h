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

#ifndef _WAYLAND_SYSTEM_COMPOSITOR_H_
#define _WAYLAND_SYSTEM_COMPOSITOR_H_

struct wlsc_input_device;
void
wlsc_device_get_position(struct wlsc_input_device *device, int32_t *x, int32_t *y);
void
notify_motion(struct wlsc_input_device *device, int x, int y);
void
notify_button(struct wlsc_input_device *device, int32_t button, int32_t state);
void
notify_key(struct wlsc_input_device *device, uint32_t key, uint32_t state);

struct evdev_input_device *
evdev_input_device_create(struct wlsc_input_device *device,
			  struct wl_display *display, const char *path);

#endif
