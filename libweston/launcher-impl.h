/*
 * Copyright © 2015 Jasper St. Pierre
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include "compositor.h"

struct weston_launcher;

struct launcher_interface {
	int (* connect) (struct weston_launcher **launcher_out, struct weston_compositor *compositor,
			 int tty, const char *seat_id, bool sync_drm);
	void (* destroy) (struct weston_launcher *launcher);
	int (* open) (struct weston_launcher *launcher, const char *path, int flags);
	void (* close) (struct weston_launcher *launcher, int fd);
	int (* activate_vt) (struct weston_launcher *launcher, int vt);
	void (* restore) (struct weston_launcher *launcher);
};

struct weston_launcher {
	struct launcher_interface *iface;
};

extern struct launcher_interface launcher_logind_iface;
extern struct launcher_interface launcher_weston_launch_iface;
extern struct launcher_interface launcher_direct_iface;
