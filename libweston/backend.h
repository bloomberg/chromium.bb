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

#endif
