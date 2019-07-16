/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2017, 2018 General Electric Company
 * Copyright © 2012, 2017-2019 Collabora, Ltd.
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

#ifndef LIBWESTON_BACKEND_INTERNAL_H
#define LIBWESTON_BACKEND_INTERNAL_H

/* weston_head */

void
weston_head_init(struct weston_head *head, const char *name);

void
weston_head_release(struct weston_head *head);

void
weston_head_set_connection_status(struct weston_head *head, bool connected);

void
weston_head_set_internal(struct weston_head *head);

void
weston_head_set_monitor_strings(struct weston_head *head,
				const char *make,
				const char *model,
				const char *serialno);
void
weston_head_set_non_desktop(struct weston_head *head, bool non_desktop);

void
weston_head_set_physical_size(struct weston_head *head,
			      int32_t mm_width, int32_t mm_height);

void
weston_head_set_subpixel(struct weston_head *head,
			 enum wl_output_subpixel sp);
/* weston_output */

void
weston_output_init(struct weston_output *output,
		   struct weston_compositor *compositor,
		   const char *name);
void
weston_output_damage(struct weston_output *output);

void
weston_output_move(struct weston_output *output, int x, int y);

void
weston_output_release(struct weston_output *output);

void
weston_output_init_zoom(struct weston_output *output);

void
weston_output_finish_frame(struct weston_output *output,
			   const struct timespec *stamp,
			   uint32_t presented_flags);
int
weston_output_mode_set_native(struct weston_output *output,
			      struct weston_mode *mode,
			      int32_t scale);
void
weston_output_transform_coordinate(struct weston_output *output,
				   double device_x, double device_y,
				   double *x, double *y);

/* weston_seat */

void
notify_axis(struct weston_seat *seat, const struct timespec *time,
	    struct weston_pointer_axis_event *event);
void
notify_axis_source(struct weston_seat *seat, uint32_t source);

void
notify_button(struct weston_seat *seat, const struct timespec *time,
	      int32_t button, enum wl_pointer_button_state state);

void
notify_key(struct weston_seat *seat, const struct timespec *time, uint32_t key,
	   enum wl_keyboard_key_state state,
	   enum weston_key_state_update update_state);
void
notify_keyboard_focus_in(struct weston_seat *seat, struct wl_array *keys,
			 enum weston_key_state_update update_state);
void
notify_keyboard_focus_out(struct weston_seat *seat);

void
notify_motion(struct weston_seat *seat, const struct timespec *time,
	      struct weston_pointer_motion_event *event);
void
notify_motion_absolute(struct weston_seat *seat, const struct timespec *time,
		       double x, double y);
void
notify_modifiers(struct weston_seat *seat, uint32_t serial);

void
notify_pointer_frame(struct weston_seat *seat);

void
notify_pointer_focus(struct weston_seat *seat, struct weston_output *output,
		     double x, double y);

/* weston_touch_device */

void
notify_touch_normalized(struct weston_touch_device *device,
			const struct timespec *time,
			int touch_id,
			double x, double y,
			const struct weston_point2d_device_normalized *norm,
			int touch_type);

/** Feed in touch down, motion, and up events, non-calibratable device.
 *
 * @sa notify_touch_cal
 */
static inline void
notify_touch(struct weston_touch_device *device, const struct timespec *time,
	     int touch_id, double x, double y, int touch_type)
{
	notify_touch_normalized(device, time, touch_id, x, y, NULL, touch_type);
}

void
notify_touch_frame(struct weston_touch_device *device);

void
notify_touch_cancel(struct weston_touch_device *device);

void
notify_touch_calibrator(struct weston_touch_device *device,
			const struct timespec *time, int32_t slot,
			const struct weston_point2d_device_normalized *norm,
			int touch_type);
void
notify_touch_calibrator_cancel(struct weston_touch_device *device);
void
notify_touch_calibrator_frame(struct weston_touch_device *device);

#endif
