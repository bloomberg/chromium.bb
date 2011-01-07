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

#define ATTRMASK_BOLD		0x01
#define ATTRMASK_UNDERLINE	0x02
#define ATTRMASK_BLINK		0x04
#define ATTRMASK_INVERSE	0x08

/* Buffer sizes */
#define MAX_RESPONSE		11
#define MAX_ESCAPE		64

union utf8_char {
	unsigned char byte[4];
	uint32_t ch;
};

enum utf8_state {
	utf8state_start,
	utf8state_accept,
	utf8state_reject,
	utf8state_expect3,
	utf8state_expect2,
	utf8state_expect1
};

struct utf8_state_machine {
	enum utf8_state state;
	int len;
	union utf8_char s;
};

static void
init_state_machine(struct utf8_state_machine *machine)
{
	machine->state = utf8state_start;
	machine->len = 0;
	machine->s.ch = 0;
}

static enum utf8_state
utf8_next_char(struct utf8_state_machine *machine, char c)
{
	switch(machine->state) {
	case utf8state_start:
	case utf8state_accept:
	case utf8state_reject:
		machine->s.ch = 0;
		machine->len = 0;
		if(c == 0xC0 || c == 0xC1) {
			/* overlong encoding, reject */
			machine->state = utf8state_reject;
		} else if((c & 0x80) == 0) {
			/* single byte, accept */
			machine->s.byte[machine->len++] = c;
			machine->state = utf8state_accept;
		} else if((c & 0xC0) == 0x80) {
			/* parser out of sync, ignore byte */
			machine->state = utf8state_start;
		} else if((c & 0xE0) == 0xC0) {
			/* start of two byte sequence */
			machine->s.byte[machine->len++] = c;
			machine->state = utf8state_expect1;
		} else if((c & 0xF0) == 0xE0) {
			/* start of three byte sequence */
			machine->s.byte[machine->len++] = c;
			machine->state = utf8state_expect2;
		} else if((c & 0xF8) == 0xF0) {
			/* start of four byte sequence */
			machine->s.byte[machine->len++] = c;
			machine->state = utf8state_expect3;
		} else {
			/* overlong encoding, reject */
			machine->state = utf8state_reject;
		}
		break;
	case utf8state_expect3:
		machine->s.byte[machine->len++] = c;
		if((c & 0xC0) == 0x80) {
			/* all good, continue */
			machine->state = utf8state_expect2;
		} else {
			/* missing extra byte, reject */
			machine->state = utf8state_reject;
		}
		break;
	case utf8state_expect2:
		machine->s.byte[machine->len++] = c;
		if((c & 0xC0) == 0x80) {
			/* all good, continue */
			machine->state = utf8state_expect1;
		} else {
			/* missing extra byte, reject */
			machine->state = utf8state_reject;
		}
		break;
	case utf8state_expect1:
		machine->s.byte[machine->len++] = c;
		if((c & 0xC0) == 0x80) {
			/* all good, accept */
			machine->state = utf8state_accept;
		} else {
			/* missing extra byte, reject */
			machine->state = utf8state_reject;
		}
		break;
	default:
		machine->state = utf8state_reject;
		break;
	}
	
	return machine->state;
}

struct terminal_color { double r, g, b, a; };
struct attr {
	unsigned char fg, bg;
	char a;        /* attributes format:
	                * 76543210
			*     ilub */
	char r;        /* reserved */
};
struct color_scheme {
	struct terminal_color palette[16];
	struct terminal_color border;
	struct attr default_attr;
};

static void
attr_init(struct attr *data_attr, struct attr attr, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		data_attr[i] = attr;
	}
}

struct terminal {
	struct window *window;
	struct display *display;
	union utf8_char *data;
	struct attr *data_attr;
	struct attr curr_attr;
	char origin_mode;
	union utf8_char last_char;
	int margin_top, margin_bottom;
	int data_pitch, attr_pitch;  /* The width in bytes of a line */
	int width, height, start, row, column;
	int fd, master;
	GIOChannel *channel;
	uint32_t modifiers;
	char escape[MAX_ESCAPE];
	int escape_length;
	int state;
	int qmark_flag;
	struct utf8_state_machine state_machine;
	int margin;
	int fullscreen;
	int focused;
	struct color_scheme *color_scheme;
	struct terminal_color color_table[256];
	cairo_font_extents_t extents;
	cairo_font_face_t *font_normal, *font_bold;
};

static void
terminal_init(struct terminal *terminal)
{
	terminal->curr_attr = terminal->color_scheme->default_attr;
	terminal->origin_mode = 0;

	terminal->row = 0;
	terminal->column = 0;
}

static void
init_color_table(struct terminal *terminal)
{
	int c, r;
	struct terminal_color *color_table = terminal->color_table;

	for (c = 0; c < 256; c ++) {
		if (c < 16) {
			color_table[c] = terminal->color_scheme->palette[c];
		} else if (c < 232) {
			r = c - 16;
			color_table[c].b = ((double)(r % 6) / 6.0); r /= 6;
			color_table[c].g = ((double)(r % 6) / 6.0); r /= 6;
			color_table[c].r = ((double)(r % 6) / 6.0);
			color_table[c].a = 1.0;
		} else {
			r = (c - 232) * 10 + 8;
			color_table[c].r = ((double) r) / 256.0;
			color_table[c].g = color_table[c].r;
			color_table[c].b = color_table[c].r;
			color_table[c].a = 1.0;
		}
	}
}

static union utf8_char *
terminal_get_row(struct terminal *terminal, int row)
{
	int index;

	index = (row + terminal->start) % terminal->height;

	return &terminal->data[index * terminal->width];
}

static struct attr*
terminal_get_attr_row(struct terminal *terminal, int row) {
	int index;

	index = (row + terminal->start) % terminal->height;

	return &terminal->data_attr[index * terminal->width];
}

static struct attr
terminal_get_attr(struct terminal *terminal, int row, int col) {
	return terminal_get_attr_row(terminal, row)[col];
}

static void
terminal_scroll_buffer(struct terminal *terminal, int d)
{
	int i;

	d = d % (terminal->height + 1);
	terminal->start = (terminal->start + d) % terminal->height;
	if (terminal->start < 0) terminal->start = terminal->height + terminal->start;
	if(d < 0) {
		d = 0 - d;
		for(i = 0; i < d; i++) {
			memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
			attr_init(terminal_get_attr_row(terminal, i),
			    terminal->curr_attr, terminal->width);
		}
	} else {
		for(i = terminal->height - d; i < terminal->height; i++) {
			memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
			attr_init(terminal_get_attr_row(terminal, i),
			    terminal->curr_attr, terminal->width);
		}
	}
}

static void
terminal_scroll_window(struct terminal *terminal, int d)
{
	int i;
	int window_height;
	int from_row, to_row;
	struct attr *dup_attr;
	
	// scrolling range is inclusive
	window_height = terminal->margin_bottom - terminal->margin_top + 1;
	d = d % (window_height + 1);
	if(d < 0) {
		d = 0 - d;
		to_row = terminal->margin_bottom;
		from_row = terminal->margin_bottom - d;
		
		for (i = 0; i < (window_height - d); i++) {
			memcpy(terminal_get_row(terminal, to_row - i),
			       terminal_get_row(terminal, from_row - i),
			       terminal->data_pitch);
			memcpy(terminal_get_attr_row(terminal, to_row - i),
			       terminal_get_attr_row(terminal, from_row - i),
			       terminal->attr_pitch);
		}
		dup_attr = terminal_get_attr_row(terminal, terminal->margin_top);
		for (i = terminal->margin_top; i < (terminal->margin_top + d); i++) {
			memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
			if (i > terminal->margin_top) {
				memcpy(terminal_get_attr_row(terminal, i),
				       dup_attr, terminal->attr_pitch);
			}
		}
	} else {
		to_row = terminal->margin_top;
		from_row = terminal->margin_top + d;
		
		for (i = 0; i < (window_height - d); i++) {
			memcpy(terminal_get_row(terminal, to_row + i),
			       terminal_get_row(terminal, from_row + i),
			       terminal->data_pitch);
			memcpy(terminal_get_attr_row(terminal, to_row + i),
			       terminal_get_attr_row(terminal, from_row + i),
			       terminal->attr_pitch);
		}
		dup_attr = terminal_get_attr_row(terminal, terminal->margin_bottom);
		for (i = terminal->margin_bottom - d + 1; i <= terminal->margin_bottom; i++) {
			memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
			if (i < terminal->margin_bottom) {
				memcpy(terminal_get_attr_row(terminal, i),
				       dup_attr, terminal->attr_pitch);
			}
		}
	}
}

static void
terminal_scroll(struct terminal *terminal, int d)
{
	if(terminal->margin_top == 0 && terminal->margin_bottom == terminal->height - 1)
		terminal_scroll_buffer(terminal, d);
	else
		terminal_scroll_window(terminal, d);
}

static void
terminal_resize(struct terminal *terminal, int width, int height)
{
	size_t size;
	union utf8_char *data;
	struct attr *data_attr;
	int data_pitch, attr_pitch;
	int i, l, total_rows, start;
	struct rectangle rectangle;
	struct winsize ws;

	if (terminal->width == width && terminal->height == height)
		return;

	data_pitch = width * sizeof(union utf8_char);
	size = data_pitch * height;
	data = malloc(size);
	attr_pitch = width * sizeof(struct attr);
	data_attr = malloc(attr_pitch * height);
	memset(data, 0, size);
	attr_init(data_attr, terminal->curr_attr, width * height);
	if (terminal->data && terminal->data_attr) {
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

		for (i = 0; i < total_rows; i++) {
			memcpy(&data[width * i],
			       terminal_get_row(terminal, i),
			       l * sizeof(union utf8_char));
			memcpy(&data_attr[width * i],
			       terminal_get_attr_row(terminal, i),
			       l * sizeof(struct attr));
		}

		free(terminal->data);
		free(terminal->data_attr);
	}

	terminal->data_pitch = data_pitch;
	terminal->attr_pitch = attr_pitch;
	terminal->width = width;
	terminal->height = height;
	if(terminal->margin_bottom >= terminal->height)
		terminal->margin_bottom = terminal->height - 1;
	terminal->data = data;
	terminal->data_attr = data_attr;

	if (terminal->row >= terminal->height)
		terminal->row = terminal->height - 1;
	if (terminal->column >= terminal->width)
		terminal->column = terminal->width - 1;
	terminal->start = 0;
	
	if (!terminal->fullscreen) {
		rectangle.width = terminal->width *
			terminal->extents.max_x_advance + 2 * terminal->margin;
		rectangle.height = terminal->height *
			terminal->extents.height + 2 * terminal->margin;
		window_set_child_size(terminal->window, &rectangle);
	}

	/* Update the window size */
	ws.ws_row = terminal->height;
	ws.ws_col = terminal->width;
	window_get_child_rectangle(terminal->window, &rectangle);
	ws.ws_xpixel = rectangle.width;
	ws.ws_ypixel = rectangle.height;
	ioctl(terminal->master, TIOCSWINSZ, &ws);
}

struct color_scheme DEFAULT_COLORS = {
	{
		{0,    0,    0,    1}, /* black */
		{0.66, 0,    0,    1}, /* red */
		{0  ,  0.66, 0,    1}, /* green */
		{0.66, 0.33, 0,    1}, /* orange (nicer than muddy yellow) */
		{0  ,  0  ,  0.66, 1}, /* blue */
		{0.66, 0  ,  0.66, 1}, /* magenta */
		{0,    0.66, 0.66, 1}, /* cyan */
		{0.66, 0.66, 0.66, 1}, /* light grey */
		{0.22, 0.33, 0.33, 1}, /* dark grey */
		{1,    0.33, 0.33, 1}, /* high red */
		{0.33, 1,    0.33, 1}, /* high green */
		{1,    1,    0.33, 1}, /* high yellow */
		{0.33, 0.33, 1,    1}, /* high blue */
		{1,    0.33, 1,    1}, /* high magenta */
		{0.33, 1,    1,    1}, /* high cyan */
		{1,    1,    1,    1}  /* white */
	},
	{0, 0, 0, 1},                  /* black border */
	{7, 0, 0, }                    /* bg:black (0), fg:light gray (7)  */
};

static void
terminal_draw_contents(struct terminal *terminal)
{
	struct rectangle rectangle;
	cairo_t *cr;
	cairo_font_extents_t extents;
	int top_margin, side_margin;
	int row, col;
	struct attr attr;
	union utf8_char *p_row;
	struct utf8_chars {
		union utf8_char c;
		char null;
	} toShow;
	int foreground, background, bold, underline;
	int text_x, text_y;
	cairo_surface_t *surface;
	double d;

	toShow.null = 0;

	window_get_child_rectangle(terminal->window, &rectangle);

	surface = display_create_surface(terminal->display, &rectangle);
	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr,
			      terminal->color_scheme->border.r,
			      terminal->color_scheme->border.g,
			      terminal->color_scheme->border.b,
			      terminal->color_scheme->border.a);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	cairo_set_font_face(cr, terminal->font_normal);
	cairo_set_font_size(cr, 14);

	cairo_font_extents(cr, &extents);
	side_margin = (rectangle.width - terminal->width * extents.max_x_advance) / 2;
	top_margin = (rectangle.height - terminal->height * extents.height) / 2;

	cairo_set_line_width(cr, 1.0);

	for (row = 0; row < terminal->height; row++) {
		p_row = terminal_get_row(terminal, row);
		for (col = 0; col < terminal->width; col++) {
			/* get the attributes for this character cell */
			attr = terminal_get_attr(terminal, row, col);
			if ((attr.a & ATTRMASK_INVERSE) ||
				(terminal->focused &&
				terminal->row == row && terminal->column == col))
			{
				foreground = attr.bg;
				background = attr.fg;
			} else {
				foreground = attr.fg;
				background = attr.bg;
			}
			bold = attr.a & (ATTRMASK_BOLD | ATTRMASK_BLINK);
			underline = attr.a & ATTRMASK_UNDERLINE;

			/* paint the background */
			cairo_set_source_rgba(cr,
					      terminal->color_table[background].r,
					      terminal->color_table[background].g,
					      terminal->color_table[background].b,
					      terminal->color_table[background].a);
			cairo_move_to(cr, side_margin + (col * extents.max_x_advance),
			      top_margin + (row * extents.height));
			cairo_rel_line_to(cr, extents.max_x_advance, 0);
			cairo_rel_line_to(cr, 0, extents.height);
			cairo_rel_line_to(cr, -extents.max_x_advance, 0);
			cairo_close_path(cr);
			cairo_fill(cr);

			/* paint the foreground */
			if (bold)
				cairo_set_font_face(cr, terminal->font_bold);
			else
				cairo_set_font_face(cr, terminal->font_normal);
			cairo_set_source_rgba(cr,
					      terminal->color_table[foreground].r,
					      terminal->color_table[foreground].g,
					      terminal->color_table[foreground].b,
					      terminal->color_table[foreground].a);

			text_x = side_margin + col * extents.max_x_advance;
			text_y = top_margin + extents.ascent + row * extents.height;
			if (underline) {
				cairo_move_to(cr, text_x, text_y + 2);
				cairo_line_to(cr, text_x + extents.max_x_advance, text_y + 2);
				cairo_stroke(cr);
			}
			cairo_move_to(cr, text_x, text_y);
			
			toShow.c = p_row[col];
			cairo_show_text(cr, (char *) toShow.c.byte);
		}
	}

	if (!terminal->focused) {
		d = 0.5;

		cairo_set_line_width(cr, 1);
		cairo_move_to(cr, side_margin + terminal->column * extents.max_x_advance + d,
			      top_margin + terminal->row * extents.height + d);
		cairo_rel_line_to(cr, extents.max_x_advance - 2 * d, 0);
		cairo_rel_line_to(cr, 0, extents.height - 2 * d);
		cairo_rel_line_to(cr, -extents.max_x_advance + 2 * d, 0);
		cairo_close_path(cr);

		cairo_stroke(cr);
	}

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
#define STATE_ESCAPE_SPECIAL 2
#define STATE_ESCAPE_CSI  3

static void
terminal_data(struct terminal *terminal, const char *data, size_t length);

static void
handle_char(struct terminal *terminal, union utf8_char utf8);

static void
handle_sgr(struct terminal *terminal, int code);

static void
handle_term_parameter(struct terminal *terminal, int code, int sr)
{
	if (terminal->qmark_flag) {
		switch(code) {
		case 6:  /* DECOM */
			terminal->origin_mode = sr;
			if (terminal->origin_mode)
				terminal->row = terminal->margin_top;
			else
				terminal->row = 0;
			terminal->column = 0;
			break;
		default:
			fprintf(stderr, "Unknown parameter: ?%d\n", code);
			break;
		}
	} else {
		switch(code) {
		default:
			fprintf(stderr, "Unknown parameter: %d\n", code);
			break;
		}
	}
}

static void
handle_escape(struct terminal *terminal)
{
	union utf8_char *row;
	struct attr *attr_row;
	char *p;
	int i, count, top, bottom;
	int args[10], set[10] = { 0, };

	terminal->escape[terminal->escape_length++] = '\0';
	i = 0;
	p = &terminal->escape[2];
	while ((isdigit(*p) || *p == ';') && i < 10) {
		if (*p == ';') {
			if (!set[i]) {
				args[i] = 0;
				set[i] = 1;
			}
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
		attr_row = terminal_get_attr_row(terminal, terminal->row);
		memset(&row[terminal->column], 0, (terminal->width - terminal->column) * sizeof(union utf8_char));
		attr_init(&attr_row[terminal->column], terminal->curr_attr, (terminal->width - terminal->column));
		for (i = terminal->row + 1; i < terminal->height; i++) {
			memset(terminal_get_row(terminal, i), 0, terminal->width * sizeof(union utf8_char));
			attr_init(terminal_get_attr_row(terminal, i), terminal->curr_attr, terminal->width);
		}
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
		attr_row = terminal_get_attr_row(terminal, terminal->row);
		memset(&row[terminal->column], 0, (terminal->width - terminal->column) * sizeof(union utf8_char));
		attr_init(&attr_row[terminal->column], terminal->curr_attr, (terminal->width - terminal->column));
		break;
	case 'S':    /* SU */
		terminal_scroll(terminal, set[0] ? args[0] : 1);
		break;
	case 'T':    /* SD */
		terminal_scroll(terminal, 0 - (set[0] ? args[0] : 1));
		break;
	case 'h':    /* SM */
		for(i = 0; i < 10 && set[i]; i++) {
			handle_term_parameter(terminal, args[i], 1);
		}
		break;
	case 'l':    /* RM */
		for(i = 0; i < 10 && set[i]; i++) {
			handle_term_parameter(terminal, args[i], 0);
		}
		break;
	case 'm':    /* SGR */
		if (set[0] && set[1] && set[2] && args[1] == 5) {
			if (args[0] == 38) {
				handle_sgr(terminal, args[2] + 256);
				break;
			} else if (args[0] == 48) {
				handle_sgr(terminal, args[2] + 512);
				break;
			}
		}
		for(i = 0; i < 10; i++) {
			if(set[i]) {
				handle_sgr(terminal, args[i]);
			} else if(i == 0) {
				handle_sgr(terminal, 0);
				break;
			} else {
				break;
			}
		}
		break;
	case 'r':
		if(!set[0]) {
			terminal->margin_top = 0;
			terminal->margin_bottom = terminal->height-1;
			terminal->row = 0;
			terminal->column = 0;
		} else {
			top = (set[0] ? args[0] : 1) - 1;
			top = top < 0 ? 0 :
			      (top >= terminal->height ? terminal->height - 1 : top);
			bottom = (set[1] ? args[1] : 1) - 1;
			bottom = bottom < 0 ? 0 :
			         (bottom >= terminal->height ? terminal->height - 1 : bottom);
			if(bottom > top) {
				terminal->margin_top = top;
				terminal->margin_bottom = bottom;
			} else {
				terminal->margin_top = 0;
				terminal->margin_bottom = terminal->height-1;
			}
			if(terminal->origin_mode)
				terminal->row = terminal->margin_top;
			else
				terminal->row = 0;
			terminal->column = 0;
		}
		break;
	default:
		fprintf(stderr, "Unknown CSI escape: %c\n", *p);
		break;
	}	
}

static void
handle_non_csi_escape(struct terminal *terminal, char code)
{
	switch(code) {
	case 'M':    /* RI */
		terminal->row -= 1;
		if(terminal->row < terminal->margin_top) {
			terminal->row = terminal->margin_top;
			terminal_scroll(terminal, -1);
		}
		break;
	case 'E':    /* NEL */
		terminal->column = 0;
		// fallthrough
	case 'D':    /* IND */
		terminal->row += 1;
		if(terminal->row > terminal->margin_bottom) {
			terminal->row = terminal->margin_bottom;
			terminal_scroll(terminal, +1);
		}
		break;
	default:
		fprintf(stderr, "Unknown escape code: %c\n", code);
		break;
	}
}

static void
handle_special_escape(struct terminal *terminal, char special, char code)
{
}

static void
handle_sgr(struct terminal *terminal, int code)
{
	switch(code) {
	case 0:
		terminal->curr_attr = terminal->color_scheme->default_attr;
		break;
	case 1:
		terminal->curr_attr.a |= ATTRMASK_BOLD;
		if (terminal->curr_attr.fg < 8)
			terminal->curr_attr.fg += 8;
		break;
	case 4:
		terminal->curr_attr.a |= ATTRMASK_UNDERLINE;
		break;
	case 5:
		terminal->curr_attr.a |= ATTRMASK_BLINK;
		break;
	case 2:
	case 21:
	case 22:
		terminal->curr_attr.a &= ~ATTRMASK_BOLD;
		if (terminal->curr_attr.fg < 16 && terminal->curr_attr.fg >= 8)
			terminal->curr_attr.fg -= 8;
		break;
	case 24:
		terminal->curr_attr.a &= ~ATTRMASK_UNDERLINE;
		break;
	case 25:
		terminal->curr_attr.a &= ~ATTRMASK_BLINK;
		break;
	case 7:
	case 26:
		terminal->curr_attr.a |= ATTRMASK_INVERSE;
		break;
	case 27:
		terminal->curr_attr.a &= ~ATTRMASK_INVERSE;
		break;
	case 39:
		terminal->curr_attr.fg = terminal->color_scheme->default_attr.fg;
		break;
	case 49:
		terminal->curr_attr.bg = terminal->color_scheme->default_attr.bg;
		break;
	default:
		if(code >= 30 && code <= 37) {
			terminal->curr_attr.fg = code - 30;
			if (terminal->curr_attr.a & ATTRMASK_BOLD)
				terminal->curr_attr.fg += 8;
		} else if(code >= 40 && code <= 47) {
			terminal->curr_attr.bg = code - 40;
		} else if(code >= 256 && code < 512) {
			terminal->curr_attr.fg = code - 256;
		} else if(code >= 512 && code < 768) {
			terminal->curr_attr.bg = code - 512;
		} else {
			fprintf(stderr, "Unknown SGR code: %d\n", code);
		}
		break;
	}
}

/* Returns 1 if c was special, otherwise 0 */
static int
handle_special_char(struct terminal *terminal, char c)
{
	union utf8_char *row;
	struct attr *attr_row;
	
	row = terminal_get_row(terminal, terminal->row);
	attr_row = terminal_get_attr_row(terminal, terminal->row);
	
	switch(c) {
	case '\r':
		terminal->column = 0;
		break;
	case '\n':
		terminal->column = 0;
		/* fallthrough */
	case '\v':
	case '\f':
		terminal->row++;
		if(terminal->row > terminal->margin_bottom) {
			terminal->row = terminal->margin_bottom;
			terminal_scroll(terminal, +1);
		}

		break;
	case '\t':
		memset(&row[terminal->column], ' ', (-terminal->column & 7) * sizeof(union utf8_char));
		attr_init(&attr_row[terminal->column], terminal->curr_attr, -terminal->column & 7);
		terminal->column = (terminal->column + 7) & ~7;
		break;
	case '\b':
		if (terminal->column > 0)
			terminal->column--;
		break;
	case '\a':
		/* Bell */
		break;
	default:
		return 0;
	}
	
	return 1;
}

static void
handle_char(struct terminal *terminal, union utf8_char utf8)
{
	union utf8_char *row;
	struct attr *attr_row;
	
	if (handle_special_char(terminal, utf8.byte[0])) return;
	
	/* There are a whole lot of non-characters, control codes,
	 * and formatting codes that should probably be ignored,
	 * for example: */
	if (strncmp((char*) utf8.byte, "\xEF\xBB\xBF", 3) == 0) {
		/* BOM, ignore */
		return;
	} 
	
	/* Some of these non-characters should be translated, e.g.: */
	if (utf8.byte[0] < 32) {
		utf8.byte[0] = utf8.byte[0] + 64;
	}
	
	/* handle right margin effects */
	if (terminal->column >= terminal->width) {
		terminal->column--;
	}
	
	row = terminal_get_row(terminal, terminal->row);
	attr_row = terminal_get_attr_row(terminal, terminal->row);
	
	row[terminal->column] = utf8;
	attr_row[terminal->column++] = terminal->curr_attr;

	if (utf8.ch != terminal->last_char.ch)
		terminal->last_char = utf8;
}

static void
terminal_data(struct terminal *terminal, const char *data, size_t length)
{
	int i;
	union utf8_char utf8;
	enum utf8_state parser_state;

	for (i = 0; i < length; i++) {
		parser_state =
			utf8_next_char(&terminal->state_machine, data[i]);
		switch(parser_state) {
		case utf8state_accept:
			utf8.ch = terminal->state_machine.s.ch;
			break;
		case utf8state_reject:
			/* the unicode replacement character */
			utf8.byte[0] = 0xEF;
			utf8.byte[1] = 0xBF;
			utf8.byte[2] = 0xBD;
			utf8.byte[3] = 0x00;
			break;
		default:
			continue;
		}

		/* assume escape codes never use non-ASCII characters */
		if (terminal->state == STATE_ESCAPE) {
			terminal->escape[terminal->escape_length++] = utf8.byte[0];
			if (utf8.byte[0] == '[') {
				terminal->state = STATE_ESCAPE_CSI;
				continue;
			} else if (utf8.byte[0] == '#' || utf8.byte[0] == '(' ||
				utf8.byte[0] == ')')
			{
				terminal->state = STATE_ESCAPE_SPECIAL;
				continue;
			} else {
				terminal->state = STATE_NORMAL;
				handle_non_csi_escape(terminal, utf8.byte[0]);
				continue;
			}
		} else if (terminal->state == STATE_ESCAPE_SPECIAL) {
			terminal->escape[terminal->escape_length++] = utf8.byte[0];
			terminal->state = STATE_NORMAL;
			if (isdigit(utf8.byte[0]) || isalpha(utf8.byte[0])) {
				handle_special_escape(terminal, terminal->escape[1],
				                      utf8.byte[0]);
				continue;
			}
		} else if (terminal->state == STATE_ESCAPE_CSI) {
			if (handle_special_char(terminal, utf8.byte[0]) != 0) {
				/* do nothing */
			} else if (utf8.byte[0] == '?') {
				terminal->qmark_flag = 1;
			} else {
				/* Don't overflow the buffer */
				if (terminal->escape_length < MAX_ESCAPE)
					terminal->escape[terminal->escape_length++] = utf8.byte[0];
				if (terminal->escape_length >= MAX_ESCAPE)
					terminal->state = STATE_NORMAL;
			}
			
			if (isalpha(utf8.byte[0]) || utf8.byte[0] == '@' ||
				utf8.byte[0] == '`')
			{
				terminal->state = STATE_NORMAL;
				handle_escape(terminal);
				continue;
			} else {
				continue;
			}
		}

		/* this is valid, because ASCII characters are never used to
		 * introduce a multibyte sequence in UTF-8 */
		if (utf8.byte[0] == '\e') {
			terminal->state = STATE_ESCAPE;
			terminal->escape[0] = '\e';
			terminal->escape_length = 1;
			terminal->qmark_flag = 0;
		} else {
			handle_char(terminal, utf8);
		} /* if */
	} /* for */

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
	terminal->color_scheme = &DEFAULT_COLORS;
	terminal_init(terminal);
	terminal->margin_top = 0;
	terminal->margin_bottom = 10000;  /* much too large, will be trimmed down
	                                   * by terminal_resize */
	terminal->window = window_create(display, "Wayland Terminal",
					 500, 400);

	init_state_machine(&terminal->state_machine);
	init_color_table(terminal);

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
	terminal->font_bold = cairo_toy_font_face_create ("mono",
	                      CAIRO_FONT_SLANT_NORMAL,
	                      CAIRO_FONT_WEIGHT_BOLD);
	cairo_font_face_reference(terminal->font_bold);
	terminal->font_normal = cairo_toy_font_face_create ("mono",
	                        CAIRO_FONT_SLANT_NORMAL,
	                        CAIRO_FONT_WEIGHT_NORMAL);
	cairo_font_face_reference(terminal->font_normal);
	cairo_set_font_face(cr, terminal->font_normal);
	cairo_set_font_size(cr, 14);
	cairo_font_extents(cr, &terminal->extents);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	terminal_resize(terminal, 80, 24);
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
