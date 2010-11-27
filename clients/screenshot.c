/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>

#include "wayland-client.h"
#include "wayland-glib.h"
#include "screenshooter-client-protocol.h"

/* The screenshooter is a good example of a custom object exposed by
 * the compositor and serves as a test bed for implementing client
 * side marshalling outside libwayland.so */

static const char socket_name[] = "\0wayland";

static void
handle_global(struct wl_display *display, uint32_t id,
	      const char *interface, uint32_t version, void *data)
{
	struct wl_screenshooter **screenshooter = data;

	if (strcmp(interface, "screenshooter") == 0)
		*screenshooter = wl_screenshooter_create(display, id);
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	GMainLoop *loop;
	GSource *source;
	struct wl_screenshooter *screenshooter;

	display = wl_display_connect(socket_name, sizeof socket_name);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	screenshooter = NULL;
	wl_display_add_global_listener(display, handle_global, &screenshooter);
	wl_display_iterate(display, WL_DISPLAY_READABLE);
	if (screenshooter == NULL) {
		fprintf(stderr, "display doesn't support screenshooter\n");
		return -1;
	}

	loop = g_main_loop_new(NULL, FALSE);
	source = wl_glib_source_new(display);
	g_source_attach(source, NULL);

	wl_screenshooter_shoot(screenshooter);

	g_idle_add((GSourceFunc) g_main_loop_quit, loop);
	g_main_loop_run(loop);

	return 0;
}
