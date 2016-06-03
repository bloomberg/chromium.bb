/*
 * Copyright © 2012 Benjamin Franzke
 * Copyright © 2013 Intel Corporation
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/major.h>

#include "launcher-impl.h"

#define DRM_MAJOR 226

#ifndef KDSKBMUTE
#define KDSKBMUTE	0x4B51
#endif

#ifdef HAVE_LIBDRM

#include <xf86drm.h>

static inline int
is_drm_master(int drm_fd)
{
	drm_magic_t magic;

	return drmGetMagic(drm_fd, &magic) == 0 &&
		drmAuthMagic(drm_fd, magic) == 0;
}

#else

static inline int
drmDropMaster(int drm_fd)
{
	return 0;
}

static inline int
drmSetMaster(int drm_fd)
{
	return 0;
}

static inline int
is_drm_master(int drm_fd)
{
	return 0;
}

#endif

struct launcher_direct {
	struct weston_launcher base;
	struct weston_compositor *compositor;
	int kb_mode, tty, drm_fd;
	struct wl_event_source *vt_source;
};

static int
vt_handler(int signal_number, void *data)
{
	struct launcher_direct *launcher = data;
	struct weston_compositor *compositor = launcher->compositor;

	if (compositor->session_active) {
		compositor->session_active = 0;
		wl_signal_emit(&compositor->session_signal, compositor);
		drmDropMaster(launcher->drm_fd);
		ioctl(launcher->tty, VT_RELDISP, 1);
	} else {
		ioctl(launcher->tty, VT_RELDISP, VT_ACKACQ);
		drmSetMaster(launcher->drm_fd);
		compositor->session_active = 1;
		wl_signal_emit(&compositor->session_signal, compositor);
	}

	return 1;
}

static int
setup_tty(struct launcher_direct *launcher, int tty)
{
	struct wl_event_loop *loop;
	struct vt_mode mode = { 0 };
	struct stat buf;
	char tty_device[32] ="<stdin>";
	int ret, kd_mode;

	if (tty == 0) {
		launcher->tty = dup(tty);
		if (launcher->tty == -1) {
			weston_log("couldn't dup stdin: %m\n");
			return -1;
		}
	} else {
		snprintf(tty_device, sizeof tty_device, "/dev/tty%d", tty);
		launcher->tty = open(tty_device, O_RDWR | O_CLOEXEC);
		if (launcher->tty == -1) {
			weston_log("couldn't open tty %s: %m\n", tty_device);
			return -1;
		}
	}

	if (fstat(launcher->tty, &buf) == -1 ||
	    major(buf.st_rdev) != TTY_MAJOR || minor(buf.st_rdev) == 0) {
		weston_log("%s not a vt\n", tty_device);
		weston_log("if running weston from ssh, "
			   "use --tty to specify a tty\n");
		goto err_close;
	}

	ret = ioctl(launcher->tty, KDGETMODE, &kd_mode);
	if (ret) {
		weston_log("failed to get VT mode: %m\n");
		return -1;
	}
	if (kd_mode != KD_TEXT) {
		weston_log("%s is already in graphics mode, "
			   "is another display server running?\n", tty_device);
		goto err_close;
	}

	ioctl(launcher->tty, VT_ACTIVATE, minor(buf.st_rdev));
	ioctl(launcher->tty, VT_WAITACTIVE, minor(buf.st_rdev));

	if (ioctl(launcher->tty, KDGKBMODE, &launcher->kb_mode)) {
		weston_log("failed to read keyboard mode: %m\n");
		goto err_close;
	}

	if (ioctl(launcher->tty, KDSKBMUTE, 1) &&
	    ioctl(launcher->tty, KDSKBMODE, K_OFF)) {
		weston_log("failed to set K_OFF keyboard mode: %m\n");
		goto err_close;
	}

	ret = ioctl(launcher->tty, KDSETMODE, KD_GRAPHICS);
	if (ret) {
		weston_log("failed to set KD_GRAPHICS mode on tty: %m\n");
		goto err_close;
	}

	/*
	 * SIGRTMIN is used as global VT-acquire+release signal. Note that
	 * SIGRT* must be tested on runtime, as their exact values are not
	 * known at compile-time. POSIX requires 32 of them to be available.
	 */
	if (SIGRTMIN > SIGRTMAX) {
		weston_log("not enough RT signals available: %u-%u\n",
			   SIGRTMIN, SIGRTMAX);
		ret = -EINVAL;
		goto err_close;
	}

	mode.mode = VT_PROCESS;
	mode.relsig = SIGRTMIN;
	mode.acqsig = SIGRTMIN;
	if (ioctl(launcher->tty, VT_SETMODE, &mode) < 0) {
		weston_log("failed to take control of vt handling\n");
		goto err_close;
	}

	loop = wl_display_get_event_loop(launcher->compositor->wl_display);
	launcher->vt_source =
		wl_event_loop_add_signal(loop, SIGRTMIN, vt_handler, launcher);
	if (!launcher->vt_source)
		goto err_close;

	return 0;

 err_close:
	close(launcher->tty);
	return -1;
}

static int
launcher_direct_open(struct weston_launcher *launcher_base, const char *path, int flags)
{
	struct launcher_direct *launcher = wl_container_of(launcher_base, launcher, base);
	struct stat s;
	int fd;

	fd = open(path, flags | O_CLOEXEC);
	if (fd == -1)
		return -1;

	if (fstat(fd, &s) == -1) {
		close(fd);
		return -1;
	}

	if (major(s.st_rdev) == DRM_MAJOR) {
		launcher->drm_fd = fd;
		if (!is_drm_master(fd)) {
			weston_log("drm fd not master\n");
			close(fd);
			return -1;
		}
	}

	return fd;
}

static void
launcher_direct_close(struct weston_launcher *launcher_base, int fd)
{
	close(fd);
}

static void
launcher_direct_restore(struct weston_launcher *launcher_base)
{
	struct launcher_direct *launcher = wl_container_of(launcher_base, launcher, base);
	struct vt_mode mode = { 0 };

	if (ioctl(launcher->tty, KDSKBMUTE, 0) &&
	    ioctl(launcher->tty, KDSKBMODE, launcher->kb_mode))
		weston_log("failed to restore kb mode: %m\n");

	if (ioctl(launcher->tty, KDSETMODE, KD_TEXT))
		weston_log("failed to set KD_TEXT mode on tty: %m\n");

	/* We have to drop master before we switch the VT back in
	 * VT_AUTO, so we don't risk switching to a VT with another
	 * display server, that will then fail to set drm master. */
	drmDropMaster(launcher->drm_fd);

	mode.mode = VT_AUTO;
	if (ioctl(launcher->tty, VT_SETMODE, &mode) < 0)
		weston_log("could not reset vt handling\n");
}

static int
launcher_direct_activate_vt(struct weston_launcher *launcher_base, int vt)
{
	struct launcher_direct *launcher = wl_container_of(launcher_base, launcher, base);
	return ioctl(launcher->tty, VT_ACTIVATE, vt);
}

static int
launcher_direct_connect(struct weston_launcher **out, struct weston_compositor *compositor,
			int tty, const char *seat_id, bool sync_drm)
{
	struct launcher_direct *launcher;

	if (geteuid() != 0)
		return -EINVAL;

	launcher = zalloc(sizeof(*launcher));
	if (launcher == NULL)
		return -ENOMEM;

	launcher->base.iface = &launcher_direct_iface;
	launcher->compositor = compositor;

	if (setup_tty(launcher, tty) == -1) {
		free(launcher);
		return -1;
	}

	* (struct launcher_direct **) out = launcher;
	return 0;
}

static void
launcher_direct_destroy(struct weston_launcher *launcher_base)
{
	struct launcher_direct *launcher = wl_container_of(launcher_base, launcher, base);

	launcher_direct_restore(&launcher->base);
	wl_event_source_remove(launcher->vt_source);

	if (launcher->tty >= 0)
		close(launcher->tty);

	free(launcher);
}

struct launcher_interface launcher_direct_iface = {
	launcher_direct_connect,
	launcher_direct_destroy,
	launcher_direct_open,
	launcher_direct_close,
	launcher_direct_activate_vt,
	launcher_direct_restore,
};
