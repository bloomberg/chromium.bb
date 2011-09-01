/*
 * Copyright © 2010 Intel Corporation
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

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>

#include "compositor.h"

struct tty {
	struct wlsc_compositor *compositor;
	int fd;
	struct termios terminal_attributes;

	struct wl_event_source *input_source;
	struct wl_event_source *enter_vt_source;
	struct wl_event_source *leave_vt_source;
	tty_vt_func_t vt_func;
};

static int on_enter_vt(int signal_number, void *data)
{
	struct tty *tty = data;
	int ret;

	tty->vt_func(tty->compositor, TTY_ENTER_VT);

	ioctl(tty->fd, VT_RELDISP, VT_ACKACQ);
	ret = ioctl(tty->fd, KDSETMODE, KD_GRAPHICS);
	if (ret)
		fprintf(stderr, "failed to set KD_GRAPHICS mode on console: %m\n");

	return 1;
}

static int
on_leave_vt(int signal_number, void *data)
{
	struct tty *tty = data;
	int ret;

	ioctl (tty->fd, VT_RELDISP, 1);
	ret = ioctl(tty->fd, KDSETMODE, KD_TEXT);
	if (ret)
		fprintf(stderr,
			"failed to set KD_TEXT mode on console: %m\n");

	tty->vt_func(tty->compositor, TTY_LEAVE_VT);

	return 1;
}

static int
on_tty_input(int fd, uint32_t mask, void *data)
{
	struct tty *tty = data;

	/* Ignore input to tty.  We get keyboard events from evdev
	 */
	tcflush(tty->fd, TCIFLUSH);

	return 1;
}

struct tty *
tty_create(struct wlsc_compositor *compositor, tty_vt_func_t vt_func,
           int tty_nr)
{
	struct termios raw_attributes;
	struct vt_mode mode = { 0 };
	int ret;
	struct tty *tty;
	struct wl_event_loop *loop;
	char filename[16];

	tty = malloc(sizeof *tty);
	if (tty == NULL)
		return NULL;

	snprintf(filename, sizeof filename, "/dev/tty%d", tty_nr);
	fprintf(stderr, "compositor: using %s\n", filename);

	memset(tty, 0, sizeof *tty);
	tty->compositor = compositor;
	tty->vt_func = vt_func;
	tty->fd = open(filename, O_RDWR | O_NOCTTY);
	if (tty->fd <= 0) {
		fprintf(stderr, "failed to open active tty: %m\n");
		return NULL;
	}

	if (tcgetattr(tty->fd, &tty->terminal_attributes) < 0) {
		fprintf(stderr, "could not get terminal attributes: %m\n");
		return NULL;
	}

	/* Ignore control characters and disable echo */
	raw_attributes = tty->terminal_attributes;
	cfmakeraw(&raw_attributes);

	/* Fix up line endings to be normal (cfmakeraw hoses them) */
	raw_attributes.c_oflag |= OPOST | OCRNL;

	if (tcsetattr(tty->fd, TCSANOW, &raw_attributes) < 0)
		fprintf(stderr, "could not put terminal into raw mode: %m\n");

	loop = wl_display_get_event_loop(compositor->wl_display);
	tty->input_source =
		wl_event_loop_add_fd(loop, tty->fd,
				     WL_EVENT_READABLE, on_tty_input, tty);

	ret = ioctl(tty->fd, KDSETMODE, KD_GRAPHICS);
	if (ret) {
		fprintf(stderr, "failed to set KD_GRAPHICS mode on tty: %m\n");
		return NULL;
	}

	tty->compositor->focus = 1;
	mode.mode = VT_PROCESS;
	mode.relsig = SIGUSR1;
	mode.acqsig = SIGUSR2;
	if (!ioctl(tty->fd, VT_SETMODE, &mode) < 0) {
		fprintf(stderr, "failed to take control of vt handling\n");
		return NULL;
	}

	tty->leave_vt_source =
		wl_event_loop_add_signal(loop, SIGUSR1, on_leave_vt, tty);
	tty->enter_vt_source =
		wl_event_loop_add_signal(loop, SIGUSR2, on_enter_vt, tty);

	return tty;
}

void
tty_destroy(struct tty *tty)
{
        if(!tty)
                return;

	if (ioctl(tty->fd, KDSETMODE, KD_TEXT))
		fprintf(stderr,
			"failed to set KD_TEXT mode on tty: %m\n");

	if (tcsetattr(tty->fd, TCSANOW, &tty->terminal_attributes) < 0)
		fprintf(stderr,
			"could not restore terminal to canonical mode\n");

	free(tty);
}
