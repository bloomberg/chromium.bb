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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <pty.h>
#include <ctype.h>
#include <cairo.h>
#include <glib.h>

#include <X11/keysym.h>

#include "wayland-util.h"
#include "wayland-client.h"
#include "wayland-glib.h"

#include "window.h"

static int option_fullscreen;

#define MOD_SHIFT	0x01
#define MOD_ALT		0x02
#define MOD_CTRL	0x04

struct terminal {
	struct window *window;
	struct display *display;
	char *data;
	int width, height, start, row, column;
	int fd, master;
	GIOChannel *channel;
	uint32_t modifiers;
	char escape[64];
	int escape_length;
	int state;
	int margin;
	int fullscreen;
	int focused;
	struct color_scheme *color_scheme;
	cairo_font_extents_t extents;
};

static char *
terminal_get_row(struct terminal *terminal, int row)
{
	int index;

	index = (row + terminal->start) % terminal->height;

	return &terminal->data[index * (terminal->width + 1)];
}

static void
terminal_resize(struct terminal *terminal, int width, int height)
{
	size_t size;
	char *data;
	int i, l, total_rows, start;

	if (terminal->width == width && terminal->height == height)
		return;

	size = (width + 1) * height;
	data = malloc(size);
	memset(data, 0, size);
	if (terminal->data) {
		if (width > terminal->width)
			l = terminal->width;
		else
			l = width;

		if (terminal->height > height) {
			total_rows = height;
			start = terminal->height - height;
		} else {
			total_rows = terminal->height;
			start = 0;
		}

		for (i = 0; i < total_rows; i++)
			memcpy(data + (width + 1) * i,
			       terminal_get_row(terminal, i), l);

		free(terminal->data);
	}

	terminal->width = width;
	terminal->height = height;
	terminal->data = data;

	if (terminal->row >= terminal->height)
		terminal->row = terminal->height - 1;
	if (terminal->column >= terminal->width)
		terminal->column = terminal->width - 1;
	terminal->start = 0;
}

struct color_scheme { struct { double r, g, b, a; } fg, bg; }
	matrix_colors = { { 0, 0.7, 0, 1 }, { 0, 0, 0, 0.9 } },
	jbarnes_colors = { { 1, 1, 1, 1 }, { 0, 0, 0, 1 } };

static void
terminal_draw_contents(struct terminal *terminal)
{
	struct rectangle rectangle;
	cairo_t *cr;
	cairo_font_extents_t extents;
	int i, top_margin, side_margin;
	cairo_surface_t *surface;
	double d;

	window_get_child_rectangle(terminal->window, &rectangle);

	surface = display_create_surface(terminal->display, &rectangle);
	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr,
			      terminal->color_scheme->bg.r,
			      terminal->color_scheme->bg.g,
			      terminal->color_scheme->bg.b,
			      terminal->color_scheme->bg.a);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr,
			      terminal->color_scheme->fg.r,
			      terminal->color_scheme->fg.g,
			      terminal->color_scheme->fg.b,
			      terminal->color_scheme->fg.a);

	cairo_select_font_face (cr, "mono",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 14);

	cairo_font_extents(cr, &extents);
	side_margin = (rectangle.width - terminal->width * extents.max_x_advance) / 2;
	top_margin = (rectangle.height - terminal->height * extents.height) / 2;

	for (i = 0; i < terminal->height; i++) {
		cairo_move_to(cr, side_margin,
			      top_margin + extents.ascent + extents.height * i);
		cairo_show_text(cr, terminal_get_row(terminal, i));
	}

	d = terminal->focused ? 0 : 0.5;

	cairo_set_line_width(cr, 1);
	cairo_move_to(cr, side_margin + terminal->column * extents.max_x_advance + d,
		      top_margin + terminal->row * extents.height + d);
	cairo_rel_line_to(cr, extents.max_x_advance - 2 * d, 0);
	cairo_rel_line_to(cr, 0, extents.height - 2 * d);
	cairo_rel_line_to(cr, -extents.max_x_advance + 2 * d, 0);
	cairo_close_path(cr);

	if (terminal->focused)
		cairo_fill(cr);
	else
		cairo_stroke(cr);

	cairo_destroy(cr);

	window_copy_surface(terminal->window,
			    &rectangle,
			    surface);

	cairo_surface_destroy(surface);
}

static void
terminal_draw(struct terminal *terminal)
{
	struct rectangle rectangle;
	int32_t width, height;

	window_get_child_rectangle(terminal->window, &rectangle);

	width = (rectangle.width - 2 * terminal->margin) /
		(int32_t) terminal->extents.max_x_advance;
	height = (rectangle.height - 2 * terminal->margin) /
		(int32_t) terminal->extents.height;
	terminal_resize(terminal, width, height);

	if (!terminal->fullscreen) {
		rectangle.width = terminal->width *
			terminal->extents.max_x_advance + 2 * terminal->margin;
		rectangle.height = terminal->height *
			terminal->extents.height + 2 * terminal->margin;
		window_set_child_size(terminal->window, &rectangle);
	}

	window_draw(terminal->window);
	terminal_draw_contents(terminal);
	window_flush(terminal->window);
}

static void
redraw_handler(struct window *window, void *data)
{
	struct terminal *terminal = data;

	terminal_draw(terminal);
}

#define STATE_NORMAL 0
#define STATE_ESCAPE 1

static void
terminal_data(struct terminal *terminal, const char *data, size_t length);

static void
handle_escape(struct terminal *terminal)
{
	char *row, *p;
	int i, count;
	int args[10], set[10] = { 0, };

	terminal->escape[terminal->escape_length++] = '\0';
	i = 0;
	p = &terminal->escape[2];
	while ((isdigit(*p) || *p == ';') && i < 10) {
		if (*p == ';') {
			p++;
			i++;
		} else {
			args[i] = strtol(p, &p, 10);
			set[i] = 1;
		}
	}
	
	switch (*p) {
	case 'A':
		count = set[0] ? args[0] : 1;
		if (terminal->row - count >= 0)
			terminal->row -= count;
		else
			terminal->row = 0;
		break;
	case 'B':
		count = set[0] ? args[0] : 1;
		if (terminal->row + count < terminal->height)
			terminal->row += count;
		else
			terminal->row = terminal->height;
		break;
	case 'C':
		count = set[0] ? args[0] : 1;
		if (terminal->column + count < terminal->width)
			terminal->column += count;
		else
			terminal->column = terminal->width;
		break;
	case 'D':
		count = set[0] ? args[0] : 1;
		if (terminal->column - count >= 0)
			terminal->column -= count;
		else
			terminal->column = 0;
		break;
	case 'J':
		row = terminal_get_row(terminal, terminal->row);
		memset(&row[terminal->column], 0, terminal->width - terminal->column);
		for (i = terminal->row + 1; i < terminal->height; i++)
			memset(terminal_get_row(terminal, i), 0, terminal->width);
		break;
	case 'G':
		if (set[0])
			terminal->column = args[0] - 1;
		break;
	case 'H':
	case 'f':
		terminal->row = set[0] ? args[0] - 1 : 0;
		terminal->column = set[1] ? args[1] - 1 : 0;
		break;
	case 'K':
		row = terminal_get_row(terminal, terminal->row);
		memset(&row[terminal->column], 0, terminal->width - terminal->column);
		break;
	case 'm':
		/* color, blink, bold etc*/
		break;
	case '?':
		if (strcmp(p, "?25l") == 0) {
			/* hide cursor */
		} else if (strcmp(p, "?25h") == 0) {
			/* show cursor */
		}
		break;
	default:
		terminal_data(terminal,
			      terminal->escape + 1,
			      terminal->escape_length - 2);
		break;
	}	
}

static void
terminal_data(struct terminal *terminal, const char *data, size_t length)
{
	int i;
	char *row;

	for (i = 0; i < length; i++) {
		row = terminal_get_row(terminal, terminal->row);

		if (terminal->state == STATE_ESCAPE) {
			terminal->escape[terminal->escape_length++] = data[i];
			if (terminal->escape_length == 2 && data[i] != '[') {
				/* Bad escape sequence. */
				terminal->state = STATE_NORMAL;
				goto cancel_escape;
			}

			if (isalpha(data[i])) {
				terminal->state = STATE_NORMAL;
				handle_escape(terminal);
			} 
			continue;
		}

	cancel_escape:
		switch (data[i]) {
		case '\r':
			terminal->column = 0;
			break;
		case '\n':
			terminal->column = 0;
			if (terminal->row + 1 < terminal->height) {
				terminal->row++;
			} else {
				terminal->start++;
				if (terminal->start == terminal->height)
					terminal->start = 0;
				memset(terminal_get_row(terminal, terminal->row),
							0, terminal->width);
			}

			break;
		case '\t':
			memset(&row[terminal->column], ' ', -terminal->column & 7);
			terminal->column = (terminal->column + 7) & ~7;
			break;
		case '\e':
			terminal->state = STATE_ESCAPE;
			terminal->escape[0] = '\e';
			terminal->escape_length = 1;
			break;
		case '\b':
			if (terminal->column > 0)
				terminal->column--;
			break;
		case '\a':
			/* Bell */
			break;
		default:
			if (terminal->column < terminal->width)
				row[terminal->column++] = data[i] < 32 ? data[i] + 64 : data[i];
			break;
		}
	}

	window_schedule_redraw(terminal->window);
}

static void
key_handler(struct window *window, uint32_t key, uint32_t sym,
	    uint32_t state, uint32_t modifiers, void *data)
{
	struct terminal *terminal = data;
	char ch[2];
	int len = 0;

	switch (sym) {
	case XK_F11:
		if (!state)
			break;
		terminal->fullscreen ^= 1;
		window_set_fullscreen(window, terminal->fullscreen);
		window_schedule_redraw(terminal->window);
		break;

	case XK_Delete:
		sym = 0x04;
	case XK_BackSpace:
	case XK_Tab:
	case XK_Linefeed:
	case XK_Clear:
	case XK_Return:
	case XK_Pause:
	case XK_Scroll_Lock:
	case XK_Sys_Req:
	case XK_Escape:
		ch[len++] = sym & 0x7f;
		break;

	case XK_Shift_L:
	case XK_Shift_R:
	case XK_Control_L:
	case XK_Control_R:
	case XK_Alt_L:
	case XK_Alt_R:
		break;

	default:
		if (modifiers & WINDOW_MODIFIER_CONTROL)
			sym = sym & 0x1f;
		else if (modifiers & WINDOW_MODIFIER_ALT)
			ch[len++] = 0x1b;
		if (sym < 256)
			ch[len++] = sym;
		break;
	}

	if (state && len > 0)
		write(terminal->master, ch, len);
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct terminal *terminal = data;

	terminal->focused = (device != NULL);
	window_schedule_redraw(terminal->window);
}

static struct terminal *
terminal_create(struct display *display, int fullscreen)
{
	struct terminal *terminal;
	cairo_surface_t *surface;
	cairo_t *cr;

	terminal = malloc(sizeof *terminal);
	if (terminal == NULL)
		return terminal;

	memset(terminal, 0, sizeof *terminal);
	terminal->fullscreen = fullscreen;
	terminal->color_scheme = &jbarnes_colors;
	terminal->window = window_create(display, "Wayland Terminal",
					 500, 100, 500, 400);
	terminal->display = display;
	terminal->margin = 5;

	window_set_fullscreen(terminal->window, terminal->fullscreen);
	window_set_user_data(terminal->window, terminal);
	window_set_redraw_handler(terminal->window, redraw_handler);

	window_set_key_handler(terminal->window, key_handler);
	window_set_keyboard_focus_handler(terminal->window,
					  keyboard_focus_handler);

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cr = cairo_create(surface);
	cairo_select_font_face (cr, "mono",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 14);
	cairo_font_extents(cr, &terminal->extents);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	terminal_draw(terminal);

	return terminal;
}

static gboolean
io_handler(GIOChannel   *source,
	   GIOCondition  condition,
	   gpointer      data)
{
	struct terminal *terminal = data;
	gchar buffer[256];
	gsize bytes_read;
	GError *error = NULL;

	g_io_channel_read_chars(source, buffer, sizeof buffer,
				&bytes_read, &error);

	terminal_data(terminal, buffer, bytes_read);

	return TRUE;
}

static int
terminal_run(struct terminal *terminal, const char *path)
{
	int master;
	pid_t pid;

	pid = forkpty(&master, NULL, NULL, NULL);
	if (pid == 0) {
		setenv("TERM", "vt100", 1);
		if (execl(path, path, NULL)) {
			printf("exec failed: %m\n");
			exit(EXIT_FAILURE);
		}
	} else if (pid < 0) {
		fprintf(stderr, "failed to fork and create pty (%m).\n");
		return -1;
	}

	terminal->master = master;
	terminal->channel = g_io_channel_unix_new(master);
	fcntl(master, F_SETFL, O_NONBLOCK);
	g_io_add_watch(terminal->channel, G_IO_IN,
		       io_handler, terminal);

	return 0;
}

static const GOptionEntry option_entries[] = {
	{ "fullscreen", 'f', 0, G_OPTION_ARG_NONE,
	  &option_fullscreen, "Run in fullscreen mode" },
	{ NULL }
};

int main(int argc, char *argv[])
{
	struct display *d;
	struct terminal *terminal;

	d = display_create(&argc, &argv, option_entries);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	terminal = terminal_create(d, option_fullscreen);
	if (terminal_run(terminal, "/bin/bash"))
		exit(EXIT_FAILURE);

	display_run(d);

	return 0;
}
