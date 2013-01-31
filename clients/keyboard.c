/*
 * Copyright © 2012 Openismus GmbH
 * Copyright © 2012 Intel Corporation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/input.h>
#include <cairo.h>

#include "window.h"
#include "input-method-client-protocol.h"
#include "text-client-protocol.h"

struct virtual_keyboard {
	struct input_panel *input_panel;
	struct input_method *input_method;
	struct input_method_context *context;
	struct display *display;
	char *preedit_string;
	uint32_t preedit_style;
	struct {
		xkb_mod_mask_t shift_mask;
	} keysym;
	uint32_t serial;
	uint32_t content_hint;
	uint32_t content_purpose;
	char *surrounding_text;
	struct window *window;
	struct widget *widget;
};

enum key_type {
	keytype_default,
	keytype_backspace,
	keytype_enter,
	keytype_space,
	keytype_switch,
	keytype_symbols,
	keytype_tab,
	keytype_arrow_up,
	keytype_arrow_left,
	keytype_arrow_right,
	keytype_arrow_down,
	keytype_style
};

struct key {
	enum key_type key_type;

	char *label;
	char *alt;

	unsigned int width;
};

struct layout {
	const struct key *keys;
	uint32_t count;

	uint32_t columns;
	uint32_t rows;
};

static const struct key normal_keys[] = {
	{ keytype_default, "q", "Q", 1},
	{ keytype_default, "w", "W", 1},
	{ keytype_default, "e", "E", 1},
	{ keytype_default, "r", "R", 1},
	{ keytype_default, "t", "T", 1},
	{ keytype_default, "y", "Y", 1},
	{ keytype_default, "u", "U", 1},
	{ keytype_default, "i", "I", 1},
	{ keytype_default, "o", "O", 1},
	{ keytype_default, "p", "P", 1},
	{ keytype_backspace, "<--", "<--", 2},

	{ keytype_tab, "->|", "->|", 1},
	{ keytype_default, "a", "A", 1},
	{ keytype_default, "s", "S", 1},
	{ keytype_default, "d", "D", 1},
	{ keytype_default, "f", "F", 1},
	{ keytype_default, "g", "G", 1},
	{ keytype_default, "h", "H", 1},
	{ keytype_default, "j", "J", 1},
	{ keytype_default, "k", "K", 1},
	{ keytype_default, "l", "L", 1},
	{ keytype_enter, "Enter", "Enter", 2},

	{ keytype_switch, "ABC", "abc", 2},
	{ keytype_default, "z", "Z", 1},
	{ keytype_default, "x", "X", 1},
	{ keytype_default, "c", "C", 1},
	{ keytype_default, "v", "V", 1},
	{ keytype_default, "b", "B", 1},
	{ keytype_default, "n", "N", 1},
	{ keytype_default, "m", "M", 1},
	{ keytype_default, ",", ",", 1},
	{ keytype_default, ".", ".", 1},
	{ keytype_switch, "ABC", "abc", 1},

	{ keytype_symbols, "?123", "?123", 1},
	{ keytype_space, "", "", 5},
	{ keytype_arrow_up, "/\\", "/\\", 1},
	{ keytype_arrow_left, "<", "<", 1},
	{ keytype_arrow_right, ">", ">", 1},
	{ keytype_arrow_down, "\\/", "\\/", 1},
	{ keytype_style, "", "", 2}
};

static const struct key numeric_keys[] = {
	{ keytype_default, "1", "1", 1},
	{ keytype_default, "2", "2", 1},
	{ keytype_default, "3", "3", 1},
	{ keytype_default, "4", "4", 1},
	{ keytype_default, "5", "5", 1},
	{ keytype_default, "6", "6", 1},
	{ keytype_default, "7", "7", 1},
	{ keytype_default, "8", "8", 1},
	{ keytype_default, "9", "9", 1},
	{ keytype_default, "0", "0", 1},
	{ keytype_backspace, "<--", "<--", 2},

	{ keytype_space, "", "", 4},
	{ keytype_enter, "Enter", "Enter", 2},
	{ keytype_arrow_up, "/\\", "/\\", 1},
	{ keytype_arrow_left, "<", "<", 1},
	{ keytype_arrow_right, ">", ">", 1},
	{ keytype_arrow_down, "\\/", "\\/", 1},
	{ keytype_style, "", "", 2}
};

static const struct layout normal_layout = {
	normal_keys,
	sizeof(normal_keys) / sizeof(*normal_keys),
	12,
	4
};

static const struct layout numeric_layout = {
	numeric_keys,
	sizeof(numeric_keys) / sizeof(*numeric_keys),
	12,
	2
};

static const char *style_labels[] = {
	"none",
	"default",
	"active",
	"inactive",
	"highlight",
	"underline",
	"selection",
	"incorrect"
};

static const double key_width = 60;
static const double key_height = 50;

enum keyboard_state {
	keyboardstate_default,
	keyboardstate_uppercase
};

struct keyboard {
	struct virtual_keyboard *keyboard;
	struct window *window;
	struct widget *widget;

	enum keyboard_state state;
};

static const char *
label_from_key(struct keyboard *keyboard,
	       const struct key *key)
{
	if (key->key_type == keytype_style)
		return style_labels[keyboard->keyboard->preedit_style];

	if (keyboard->state == keyboardstate_default)
		return key->label;
	else
		return key->alt;
}

static void
draw_key(struct keyboard *keyboard,
	 const struct key *key,
	 cairo_t *cr,
	 unsigned int row,
	 unsigned int col)
{
	const char *label;
	cairo_text_extents_t extents;

	cairo_save(cr);
	cairo_rectangle(cr,
			col * key_width, row * key_height,
			key->width * key_width, key_height);
	cairo_clip(cr);

	/* Paint frame */
	cairo_rectangle(cr,
			col * key_width, row * key_height,
			key->width * key_width, key_height);
	cairo_set_line_width(cr, 3);
	cairo_stroke(cr);

	/* Paint text */
	label = label_from_key(keyboard, key);
	cairo_text_extents(cr, label, &extents);

	cairo_translate(cr,
			col * key_width,
			row * key_height);
	cairo_translate(cr,
			(key->width * key_width - extents.width) / 2,
			(key_height - extents.y_bearing) / 2);
	cairo_show_text(cr, label);

	cairo_restore(cr);
}

static const struct layout *
get_current_layout(struct virtual_keyboard *keyboard)
{
	switch (keyboard->content_purpose) {
		case TEXT_MODEL_CONTENT_PURPOSE_DIGITS:
		case TEXT_MODEL_CONTENT_PURPOSE_NUMBER:
			return &numeric_layout;
		default:
			return &normal_layout;
	}
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct keyboard *keyboard = data;
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_t *cr;
	unsigned int i;
	unsigned int row = 0, col = 0;
	const struct layout *layout;

	layout = get_current_layout(keyboard->keyboard);

	surface = window_get_surface(keyboard->window);
	widget_get_allocation(keyboard->widget, &allocation);

	cr = cairo_create(surface);
	cairo_rectangle(cr, allocation.x, allocation.y, allocation.width, allocation.height);
	cairo_clip(cr);

	cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 16);

	cairo_translate(cr, allocation.x, allocation.y);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.75);
	cairo_rectangle(cr, 0, 0, layout->columns * key_width, layout->rows * key_height);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	for (i = 0; i < layout->count; ++i) {
		cairo_set_source_rgb(cr, 0, 0, 0);
		draw_key(keyboard, &layout->keys[i], cr, row, col);
		col += layout->keys[i].width;
		if (col >= layout->columns) {
			row += 1;
			col = 0;
		}
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height, void *data)
{
	/* struct keyboard *keyboard = data; */
}

static void
virtual_keyboard_commit_preedit(struct virtual_keyboard *keyboard)
{
	if (!keyboard->preedit_string ||
	    strlen(keyboard->preedit_string) == 0)
		return;

	input_method_context_preedit_cursor(keyboard->context,
					    keyboard->serial,
					    0);
	input_method_context_preedit_string(keyboard->context,
					    keyboard->serial,
					    "",
					    "");
	input_method_context_commit_string(keyboard->context,
					   keyboard->serial,
					   keyboard->preedit_string,
					   strlen(keyboard->preedit_string));
	free(keyboard->preedit_string);
	keyboard->preedit_string = strdup("");
}

static void
virtual_keyboard_send_preedit(struct virtual_keyboard *keyboard,
			      int32_t cursor)
{
	uint32_t index = strlen(keyboard->preedit_string);

	if (keyboard->preedit_style)
		input_method_context_preedit_styling(keyboard->context,
						     keyboard->serial,
						     0,
						     strlen(keyboard->preedit_string),
						     keyboard->preedit_style);
	if (cursor > 0)
		index = cursor;
	input_method_context_preedit_cursor(keyboard->context,
					    keyboard->serial,
					    index);
	input_method_context_preedit_string(keyboard->context,
					    keyboard->serial,
					    keyboard->preedit_string,
					    keyboard->preedit_string);
}

static void
keyboard_handle_key(struct keyboard *keyboard, uint32_t time, const struct key *key, struct input *input, enum wl_pointer_button_state state)
{
	const char *label = keyboard->state == keyboardstate_default ? key->label : key->alt;
	xkb_mod_mask_t mod_mask = keyboard->state == keyboardstate_default ? 0 : keyboard->keyboard->keysym.shift_mask;
	uint32_t key_state = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;

	switch (key->key_type) {
		case keytype_default:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;

			keyboard->keyboard->preedit_string = strcat(keyboard->keyboard->preedit_string,
								    label);
			virtual_keyboard_send_preedit(keyboard->keyboard, -1);
			break;
		case keytype_backspace:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;

			if (strlen(keyboard->keyboard->preedit_string) == 0) {
				input_method_context_delete_surrounding_text(keyboard->keyboard->context,
									     keyboard->keyboard->serial,
									     -1, 1);
			} else {
				keyboard->keyboard->preedit_string[strlen(keyboard->keyboard->preedit_string) - 1] = '\0';
				virtual_keyboard_send_preedit(keyboard->keyboard, -1);
			}
			break;
		case keytype_enter:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			input_method_context_keysym(keyboard->keyboard->context,
						    display_get_serial(keyboard->keyboard->display),
						    time, 
						    XKB_KEY_Return, key_state, mod_mask);
			break;
		case keytype_space:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;
			keyboard->keyboard->preedit_string = strcat(keyboard->keyboard->preedit_string,
								    " ");
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			break;
		case keytype_switch:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;
			if (keyboard->state == keyboardstate_default)
				keyboard->state = keyboardstate_uppercase;
			else
				keyboard->state = keyboardstate_default;
			break;
		case keytype_symbols:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;
			break;
		case keytype_tab:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			input_method_context_keysym(keyboard->keyboard->context,
						    display_get_serial(keyboard->keyboard->display),
						    time, 
						    XKB_KEY_Tab, key_state, mod_mask);
			break;
		case keytype_arrow_up:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			input_method_context_keysym(keyboard->keyboard->context,
						    display_get_serial(keyboard->keyboard->display),
						    time, 
						    XKB_KEY_Up, key_state, mod_mask);
			break;
		case keytype_arrow_left:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			input_method_context_keysym(keyboard->keyboard->context,
						    display_get_serial(keyboard->keyboard->display),
						    time, 
						    XKB_KEY_Left, key_state, mod_mask);
			break;
		case keytype_arrow_right:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			input_method_context_keysym(keyboard->keyboard->context,
						    display_get_serial(keyboard->keyboard->display),
						    time, 
						    XKB_KEY_Right, key_state, mod_mask);
			break;
		case keytype_arrow_down:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			input_method_context_keysym(keyboard->keyboard->context,
						    display_get_serial(keyboard->keyboard->display),
						    time, 
						    XKB_KEY_Down, key_state, mod_mask);
			break;
		case keytype_style:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;
			keyboard->keyboard->preedit_style = (keyboard->keyboard->preedit_style + 1) % 8; /* TODO */
			virtual_keyboard_send_preedit(keyboard->keyboard, -1);
			break;
	}
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button,
	       enum wl_pointer_button_state state, void *data)
{
	struct keyboard *keyboard = data;
	struct rectangle allocation;
	int32_t x, y;
	int row, col;
	unsigned int i;
	const struct layout *layout;

	layout = get_current_layout(keyboard->keyboard);

	if (button != BTN_LEFT) {
		return;
	}

	input_get_position(input, &x, &y);

	widget_get_allocation(keyboard->widget, &allocation);
	x -= allocation.x;
	y -= allocation.y;

	row = y / key_height;
	col = x / key_width + row * layout->columns;
	for (i = 0; i < layout->count; ++i) {
		col -= layout->keys[i].width;
		if (col < 0) {
			keyboard_handle_key(keyboard, time, &layout->keys[i], input, state);
			break;
		}
	}

	widget_schedule_redraw(widget);
}

static void
input_method_context_surrounding_text(void *data,
				      struct input_method_context *context,
				      const char *text,
				      uint32_t cursor,
				      uint32_t anchor)
{
	struct virtual_keyboard *keyboard = data;

	free(keyboard->surrounding_text);
	keyboard->surrounding_text = strdup(text);
}

static void
input_method_context_reset(void *data,
			   struct input_method_context *context,
			   uint32_t serial)
{
	struct virtual_keyboard *keyboard = data;

	fprintf(stderr, "Reset pre-edit buffer\n");

	if (strlen(keyboard->preedit_string)) {
		input_method_context_preedit_cursor(context,
						    serial,
						    0);
		input_method_context_preedit_string(context,
						    serial,
						    "",
						    "");
		free(keyboard->preedit_string);
		keyboard->preedit_string = strdup("");
	}

	keyboard->serial = serial;
}

static void
input_method_context_content_type(void *data,
				  struct input_method_context *context,
				  uint32_t hint,
				  uint32_t purpose)
{
	struct virtual_keyboard *keyboard = data;

	keyboard->content_hint = hint;
	keyboard->content_purpose = purpose;
}

static void
input_method_context_invoke_action(void *data,
				   struct input_method_context *context,
				   uint32_t button,
				   uint32_t index)
{
	struct virtual_keyboard *keyboard = data;

	if (button != BTN_LEFT)
		return;

	virtual_keyboard_send_preedit(keyboard, index);
}

static void
input_method_context_commit(void *data,
			    struct input_method_context *context)
{
	struct virtual_keyboard *keyboard = data;
	const struct layout *layout;

	layout = get_current_layout(keyboard);

	if (keyboard->surrounding_text)
		fprintf(stderr, "Surrounding text updated: %s\n", keyboard->surrounding_text);

	window_schedule_resize(keyboard->window,
			       layout->columns * key_width,
			       layout->rows * key_height);

	widget_schedule_redraw(keyboard->widget);
}

static const struct input_method_context_listener input_method_context_listener = {
	input_method_context_surrounding_text,
	input_method_context_reset,
	input_method_context_content_type,
	input_method_context_invoke_action,
	input_method_context_commit
};

static void
input_method_activate(void *data,
		      struct input_method *input_method,
		      struct input_method_context *context,
		      uint32_t serial)
{
	struct virtual_keyboard *keyboard = data;
	struct wl_array modifiers_map;

	if (keyboard->context)
		input_method_context_destroy(keyboard->context);

	if (keyboard->preedit_string)
		free(keyboard->preedit_string);

	keyboard->preedit_string = strdup("");
	keyboard->serial = serial;

	keyboard->context = context;
	input_method_context_add_listener(context,
					  &input_method_context_listener,
					  keyboard);

	wl_array_init(&modifiers_map);
	keysym_modifiers_add(&modifiers_map, "Shift");
	keysym_modifiers_add(&modifiers_map, "Control");
	keysym_modifiers_add(&modifiers_map, "Mod1");
	input_method_context_modifiers_map(context, &modifiers_map);
	keyboard->keysym.shift_mask = keysym_modifiers_get_mask(&modifiers_map, "Shift");
	wl_array_release(&modifiers_map);
}

static void
input_method_deactivate(void *data,
			struct input_method *input_method,
			struct input_method_context *context)
{
	struct virtual_keyboard *keyboard = data;

	if (!keyboard->context)
		return;

	input_method_context_destroy(keyboard->context);
	keyboard->context = NULL;
}

static const struct input_method_listener input_method_listener = {
	input_method_activate,
	input_method_deactivate
};

static void
global_handler(struct display *display, uint32_t name,
	       const char *interface, uint32_t version, void *data)
{
	struct virtual_keyboard *keyboard = data;

	if (!strcmp(interface, "input_panel")) {
		keyboard->input_panel =
			display_bind(display, name, &input_panel_interface, 1);
	} else if (!strcmp(interface, "input_method")) {
		keyboard->input_method =
			display_bind(display, name,
				     &input_method_interface, 1);
		input_method_add_listener(keyboard->input_method, &input_method_listener, keyboard);
	}
}

static void
keyboard_create(struct output *output, struct virtual_keyboard *virtual_keyboard)
{
	struct keyboard *keyboard;
	const struct layout *layout;
	struct input_panel_surface *ips;

	layout = get_current_layout(virtual_keyboard);

	keyboard = malloc(sizeof *keyboard);
	memset(keyboard, 0, sizeof *keyboard);

	keyboard->keyboard = virtual_keyboard;
	keyboard->window = window_create_custom(virtual_keyboard->display);
	keyboard->widget = window_add_widget(keyboard->window, keyboard);
	virtual_keyboard->window = keyboard->window;
	virtual_keyboard->widget = keyboard->widget;

	window_set_title(keyboard->window, "Virtual keyboard");
	window_set_user_data(keyboard->window, keyboard);

	widget_set_redraw_handler(keyboard->widget, redraw_handler);
	widget_set_resize_handler(keyboard->widget, resize_handler);
	widget_set_button_handler(keyboard->widget, button_handler);

	window_schedule_resize(keyboard->window,
			       layout->columns * key_width,
			       layout->rows * key_height);


	ips = input_panel_get_input_panel_surface(virtual_keyboard->input_panel,
						  window_get_wl_surface(keyboard->window));

	input_panel_surface_set_toplevel(ips, INPUT_PANEL_SURFACE_POSITION_CENTER_BOTTOM);
}

static void
handle_output_configure(struct output *output, void *data)
{
	struct virtual_keyboard *virtual_keyboard = data;

	/* skip existing outputs */
	if (output_get_user_data(output))
		return;

	output_set_user_data(output, virtual_keyboard);

	keyboard_create(output, virtual_keyboard);
}

int
main(int argc, char *argv[])
{
	struct virtual_keyboard virtual_keyboard;

	memset(&virtual_keyboard, 0, sizeof virtual_keyboard);

	virtual_keyboard.display = display_create(argc, argv);
	if (virtual_keyboard.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	display_set_user_data(virtual_keyboard.display, &virtual_keyboard);
	display_set_global_handler(virtual_keyboard.display, global_handler);
	display_set_output_configure_handler(virtual_keyboard.display, handle_output_configure);

	display_run(virtual_keyboard.display);

	return 0;
}
