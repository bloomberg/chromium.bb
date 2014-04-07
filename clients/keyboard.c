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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/input.h>
#include <cairo.h>

#include "window.h"
#include "input-method-client-protocol.h"
#include "text-client-protocol.h"

struct keyboard;

struct virtual_keyboard {
	struct wl_input_panel *input_panel;
	struct wl_input_method *input_method;
	struct wl_input_method_context *context;
	struct display *display;
	struct output *output;
	char *preedit_string;
	uint32_t preedit_style;
	struct {
		xkb_mod_mask_t shift_mask;
	} keysym;
	uint32_t serial;
	uint32_t content_hint;
	uint32_t content_purpose;
	char *preferred_language;
	char *surrounding_text;
	uint32_t surrounding_cursor;
	struct keyboard *keyboard;
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

	const char *language;
	uint32_t text_direction;
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

static const struct key arabic_keys[] = {
	{ keytype_default, "ض", "ض", 1},
	{ keytype_default, "ص", "ص", 1},
	{ keytype_default, "ث", "ث", 1},
	{ keytype_default, "ق", "ق", 1},
	{ keytype_default, "ف", "ف", 1},
	{ keytype_default, "غ", "إ", 1},
	{ keytype_default, "ع", "ع", 1},
	{ keytype_default, "ه", "ه", 1},
	{ keytype_default, "خ", "خ", 1},
	{ keytype_default, "ح", "ح", 1},
	{ keytype_default, "ج", "ج", 1},
	{ keytype_backspace, "-->", "-->", 2},

	{ keytype_tab, "->|", "->|", 1},
	{ keytype_default, "ش", "ش", 1},
	{ keytype_default, "س", "س", 1},
	{ keytype_default, "ي", "ي", 1},
	{ keytype_default, "ب", "ب", 1},
	{ keytype_default, "ل", "ل", 1},
	{ keytype_default, "ا", "أ", 1},
	{ keytype_default, "ت", "ت", 1},
	{ keytype_default, "ن", "ن", 1},
	{ keytype_default, "م", "م", 1},
	{ keytype_default, "ك", "ك", 1},
	{ keytype_default, "د", "د", 1},
	{ keytype_enter, "Enter", "Enter", 2},

	{ keytype_switch, "ABC", "abc", 2},
	{ keytype_default, "ئ", "ئ", 1},
	{ keytype_default, "ء", "ء", 1},
	{ keytype_default, "ؤ", "ؤ", 1},
	{ keytype_default, "ر", "ر", 1},
	{ keytype_default, "ى", "آ", 1},
	{ keytype_default, "ة", "ة", 1},
	{ keytype_default, "و", "و", 1},
	{ keytype_default, "ز", "ز", 1},
	{ keytype_default, "ظ", "ظ", 1},
	{ keytype_switch, "ABC", "abc", 2},

	{ keytype_symbols, "؟٣٢١", "؟٣٢١", 1},
	{ keytype_default, "ذ", "ذ", 1},
	{ keytype_default, "،", "،", 1},
	{ keytype_space, "", "", 6},
	{ keytype_default, ".", ".", 1},
	{ keytype_default, "ط", "ط", 1},
	{ keytype_style, "", "", 2}
};


static const struct layout normal_layout = {
	normal_keys,
	sizeof(normal_keys) / sizeof(*normal_keys),
	12,
	4,
	"en",
	WL_TEXT_INPUT_TEXT_DIRECTION_LTR
};

static const struct layout numeric_layout = {
	numeric_keys,
	sizeof(numeric_keys) / sizeof(*numeric_keys),
	12,
	2,
	"en",
	WL_TEXT_INPUT_TEXT_DIRECTION_LTR
};

static const struct layout arabic_layout = {
	arabic_keys,
	sizeof(arabic_keys) / sizeof(*arabic_keys),
	13,
	4,
	"ar",
	WL_TEXT_INPUT_TEXT_DIRECTION_RTL
};

static const char *style_labels[] = {
	"default",
	"none",
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

static void __attribute__ ((format (printf, 1, 2)))
dbg(const char *fmt, ...)
{
#ifdef DEBUG
	int l;
	va_list argp;

	va_start(argp, fmt);
	l = vfprintf(stderr, fmt, argp);
	va_end(argp);
#endif
}

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
		case WL_TEXT_INPUT_CONTENT_PURPOSE_DIGITS:
		case WL_TEXT_INPUT_CONTENT_PURPOSE_NUMBER:
			return &numeric_layout;
		default:
			if (keyboard->preferred_language &&
			    strcmp(keyboard->preferred_language, "ar") == 0)
				return &arabic_layout;
			else
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

static char *
insert_text(const char *text, uint32_t offset, const char *insert)
{
	int tlen = strlen(text), ilen = strlen(insert);
	char *new_text = xmalloc(tlen + ilen + 1);

	memcpy(new_text, text, offset);
	memcpy(new_text + offset, insert, ilen);
	memcpy(new_text + offset + ilen, text + offset, tlen - offset);
	new_text[tlen + ilen] = '\0';

	return new_text;
}

static void
virtual_keyboard_commit_preedit(struct virtual_keyboard *keyboard)
{
	char *surrounding_text;

	if (!keyboard->preedit_string ||
	    strlen(keyboard->preedit_string) == 0)
		return;

	wl_input_method_context_cursor_position(keyboard->context,
						0, 0);
	wl_input_method_context_commit_string(keyboard->context,
					      keyboard->serial,
					      keyboard->preedit_string);

	if (keyboard->surrounding_text) {
		surrounding_text = insert_text(keyboard->surrounding_text,
					       keyboard->surrounding_cursor,
					       keyboard->preedit_string);
		free(keyboard->surrounding_text);
		keyboard->surrounding_text = surrounding_text;
		keyboard->surrounding_cursor += strlen(keyboard->preedit_string);
	} else {
		keyboard->surrounding_text = strdup(keyboard->preedit_string);
		keyboard->surrounding_cursor = strlen(keyboard->preedit_string);
	}

	free(keyboard->preedit_string);
	keyboard->preedit_string = strdup("");
}

static void
virtual_keyboard_send_preedit(struct virtual_keyboard *keyboard,
			      int32_t cursor)
{
	uint32_t index = strlen(keyboard->preedit_string);

	if (keyboard->preedit_style)
		wl_input_method_context_preedit_styling(keyboard->context,
							0,
							strlen(keyboard->preedit_string),
							keyboard->preedit_style);
	if (cursor > 0)
		index = cursor;
	wl_input_method_context_preedit_cursor(keyboard->context,
					       index);
	wl_input_method_context_preedit_string(keyboard->context,
					       keyboard->serial,
					       keyboard->preedit_string,
					       keyboard->preedit_string);
}

static const char *
prev_utf8_char(const char *s, const char *p)
{
	for (--p; p >= s; --p) {
		if ((*p & 0xc0) != 0x80)
			return p;
	}
	return NULL;
}

static void
delete_before_cursor(struct virtual_keyboard *keyboard)
{
	const char *start, *end;

	if (!keyboard->surrounding_text) {
		dbg("delete_before_cursor: No surrounding text available\n");
		return;
	}

	start = prev_utf8_char(keyboard->surrounding_text,
			       keyboard->surrounding_text + keyboard->surrounding_cursor);
	if (!start) {
		dbg("delete_before_cursor: No previous character to delete\n");
		return;
	}

	end = keyboard->surrounding_text + keyboard->surrounding_cursor;

	wl_input_method_context_delete_surrounding_text(keyboard->context,
							(start - keyboard->surrounding_text) - keyboard->surrounding_cursor,
							end - start);
	wl_input_method_context_commit_string(keyboard->context,
					      keyboard->serial,
					      "");

	/* Update surrounding text */
	keyboard->surrounding_cursor = start - keyboard->surrounding_text;
	keyboard->surrounding_text[keyboard->surrounding_cursor] = '\0';
	if (*end)
		memmove(keyboard->surrounding_text + keyboard->surrounding_cursor, end, strlen(end));
}

static char *
append(char *s1, const char *s2)
{
	int len1, len2;
	char *s;

	len1 = strlen(s1);
	len2 = strlen(s2);
	s = xrealloc(s1, len1 + len2 + 1);
	memcpy(s + len1, s2, len2);
	s[len1 + len2] = '\0';

	return s;
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

			keyboard->keyboard->preedit_string =
				append(keyboard->keyboard->preedit_string,
				       label);
			virtual_keyboard_send_preedit(keyboard->keyboard, -1);
			break;
		case keytype_backspace:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;

			if (strlen(keyboard->keyboard->preedit_string) == 0) {
				delete_before_cursor(keyboard->keyboard);
			} else {
				keyboard->keyboard->preedit_string[strlen(keyboard->keyboard->preedit_string) - 1] = '\0';
				virtual_keyboard_send_preedit(keyboard->keyboard, -1);
			}
			break;
		case keytype_enter:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			wl_input_method_context_keysym(keyboard->keyboard->context,
						       display_get_serial(keyboard->keyboard->display),
						       time, 
						       XKB_KEY_Return, key_state, mod_mask);
			break;
		case keytype_space:
			if (state != WL_POINTER_BUTTON_STATE_PRESSED)
				break;
			keyboard->keyboard->preedit_string =
				append(keyboard->keyboard->preedit_string, " ");
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
			wl_input_method_context_keysym(keyboard->keyboard->context,
						       display_get_serial(keyboard->keyboard->display),
						       time, 
						       XKB_KEY_Tab, key_state, mod_mask);
			break;
		case keytype_arrow_up:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			wl_input_method_context_keysym(keyboard->keyboard->context,
						       display_get_serial(keyboard->keyboard->display),
						       time, 
						       XKB_KEY_Up, key_state, mod_mask);
			break;
		case keytype_arrow_left:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			wl_input_method_context_keysym(keyboard->keyboard->context,
						       display_get_serial(keyboard->keyboard->display),
						       time, 
						       XKB_KEY_Left, key_state, mod_mask);
			break;
		case keytype_arrow_right:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			wl_input_method_context_keysym(keyboard->keyboard->context,
						       display_get_serial(keyboard->keyboard->display),
						       time, 
						       XKB_KEY_Right, key_state, mod_mask);
			break;
		case keytype_arrow_down:
			virtual_keyboard_commit_preedit(keyboard->keyboard);
			wl_input_method_context_keysym(keyboard->keyboard->context,
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
touch_handler(struct input *input, uint32_t time,
	      float x, float y, uint32_t state, void *data)
{
	struct keyboard *keyboard = data;
	struct rectangle allocation;
	int row, col;
	unsigned int i;
	const struct layout *layout;

	layout = get_current_layout(keyboard->keyboard);

	widget_get_allocation(keyboard->widget, &allocation);

	x -= allocation.x;
	y -= allocation.y;

	row = (int)y / key_height;
	col = (int)x / key_width + row * layout->columns;
	for (i = 0; i < layout->count; ++i) {
		col -= layout->keys[i].width;
		if (col < 0) {
			keyboard_handle_key(keyboard, time,
					    &layout->keys[i], input, state);
			break;
		}
	}

	widget_schedule_redraw(keyboard->widget);
}

static void
touch_down_handler(struct widget *widget, struct input *input,
		   uint32_t serial, uint32_t time, int32_t id,
		   float x, float y, void *data)
{
  touch_handler(input, time, x, y, 
		WL_POINTER_BUTTON_STATE_PRESSED, data);
}

static void
touch_up_handler(struct widget *widget, struct input *input,
		 uint32_t serial, uint32_t time, int32_t id,
		 void *data)
{
  float x, y;

  input_get_touch(input, id, &x, &y);

  touch_handler(input, time, x, y,
		WL_POINTER_BUTTON_STATE_RELEASED, data);
}

static void
handle_surrounding_text(void *data,
			struct wl_input_method_context *context,
			const char *text,
			uint32_t cursor,
			uint32_t anchor)
{
	struct virtual_keyboard *keyboard = data;

	free(keyboard->surrounding_text);
	keyboard->surrounding_text = strdup(text);

	keyboard->surrounding_cursor = cursor;
}

static void
handle_reset(void *data,
	     struct wl_input_method_context *context)
{
	struct virtual_keyboard *keyboard = data;

	dbg("Reset pre-edit buffer\n");

	if (strlen(keyboard->preedit_string)) {
		free(keyboard->preedit_string);
		keyboard->preedit_string = strdup("");
	}
}

static void
handle_content_type(void *data,
		    struct wl_input_method_context *context,
		    uint32_t hint,
		    uint32_t purpose)
{
	struct virtual_keyboard *keyboard = data;

	keyboard->content_hint = hint;
	keyboard->content_purpose = purpose;
}

static void
handle_invoke_action(void *data,
		     struct wl_input_method_context *context,
		     uint32_t button,
		     uint32_t index)
{
	struct virtual_keyboard *keyboard = data;

	if (button != BTN_LEFT)
		return;

	virtual_keyboard_send_preedit(keyboard, index);
}

static void
handle_commit_state(void *data,
		    struct wl_input_method_context *context,
		    uint32_t serial)
{
	struct virtual_keyboard *keyboard = data;
	const struct layout *layout;

	keyboard->serial = serial;

	layout = get_current_layout(keyboard);

	if (keyboard->surrounding_text)
		dbg("Surrounding text updated: %s\n", keyboard->surrounding_text);

	window_schedule_resize(keyboard->keyboard->window,
			       layout->columns * key_width,
			       layout->rows * key_height);

	wl_input_method_context_language(context, keyboard->serial, layout->language);
	wl_input_method_context_text_direction(context, keyboard->serial, layout->text_direction);

	widget_schedule_redraw(keyboard->keyboard->widget);
}

static void
handle_preferred_language(void *data,
			  struct wl_input_method_context *context,
			  const char *language)
{
	struct virtual_keyboard *keyboard = data;

	if (keyboard->preferred_language)
		free(keyboard->preferred_language);

	keyboard->preferred_language = NULL;

	if (language)
		keyboard->preferred_language = strdup(language);
}

static const struct wl_input_method_context_listener input_method_context_listener = {
	handle_surrounding_text,
	handle_reset,
	handle_content_type,
	handle_invoke_action,
	handle_commit_state,
	handle_preferred_language
};

static void
input_method_activate(void *data,
		      struct wl_input_method *input_method,
		      struct wl_input_method_context *context)
{
	struct virtual_keyboard *keyboard = data;
	struct wl_array modifiers_map;
	const struct layout *layout;

	keyboard->keyboard->state = keyboardstate_default;

	if (keyboard->context)
		wl_input_method_context_destroy(keyboard->context);

	if (keyboard->preedit_string)
		free(keyboard->preedit_string);

	keyboard->preedit_string = strdup("");
	keyboard->content_hint = 0;
	keyboard->content_purpose = 0;
	free(keyboard->preferred_language);
	keyboard->preferred_language = NULL;
	free(keyboard->surrounding_text);
	keyboard->surrounding_text = NULL;

	keyboard->serial = 0;

	keyboard->context = context;
	wl_input_method_context_add_listener(context,
					     &input_method_context_listener,
					     keyboard);

	wl_array_init(&modifiers_map);
	keysym_modifiers_add(&modifiers_map, "Shift");
	keysym_modifiers_add(&modifiers_map, "Control");
	keysym_modifiers_add(&modifiers_map, "Mod1");
	wl_input_method_context_modifiers_map(context, &modifiers_map);
	keyboard->keysym.shift_mask = keysym_modifiers_get_mask(&modifiers_map, "Shift");
	wl_array_release(&modifiers_map);

	layout = get_current_layout(keyboard);

	window_schedule_resize(keyboard->keyboard->window,
			       layout->columns * key_width,
			       layout->rows * key_height);

	wl_input_method_context_language(context, keyboard->serial, layout->language);
	wl_input_method_context_text_direction(context, keyboard->serial, layout->text_direction);

	widget_schedule_redraw(keyboard->keyboard->widget);
}

static void
input_method_deactivate(void *data,
			struct wl_input_method *input_method,
			struct wl_input_method_context *context)
{
	struct virtual_keyboard *keyboard = data;

	if (!keyboard->context)
		return;

	wl_input_method_context_destroy(keyboard->context);
	keyboard->context = NULL;
}

static const struct wl_input_method_listener input_method_listener = {
	input_method_activate,
	input_method_deactivate
};

static void
global_handler(struct display *display, uint32_t name,
	       const char *interface, uint32_t version, void *data)
{
	struct virtual_keyboard *keyboard = data;

	if (!strcmp(interface, "wl_input_panel")) {
		keyboard->input_panel =
			display_bind(display, name, &wl_input_panel_interface, 1);
	} else if (!strcmp(interface, "wl_input_method")) {
		keyboard->input_method =
			display_bind(display, name,
				     &wl_input_method_interface, 1);
		wl_input_method_add_listener(keyboard->input_method, &input_method_listener, keyboard);
	}
}

static void
keyboard_create(struct output *output, struct virtual_keyboard *virtual_keyboard)
{
	struct keyboard *keyboard;
	const struct layout *layout;
	struct wl_input_panel_surface *ips;

	layout = get_current_layout(virtual_keyboard);

	keyboard = xzalloc(sizeof *keyboard);
	keyboard->keyboard = virtual_keyboard;
	keyboard->window = window_create_custom(virtual_keyboard->display);
	keyboard->widget = window_add_widget(keyboard->window, keyboard);

	virtual_keyboard->keyboard = keyboard;

	window_set_title(keyboard->window, "Virtual keyboard");
	window_set_user_data(keyboard->window, keyboard);

	widget_set_redraw_handler(keyboard->widget, redraw_handler);
	widget_set_resize_handler(keyboard->widget, resize_handler);
	widget_set_button_handler(keyboard->widget, button_handler);
	widget_set_touch_down_handler(keyboard->widget, touch_down_handler);
	widget_set_touch_up_handler(keyboard->widget, touch_up_handler);

	window_schedule_resize(keyboard->window,
			       layout->columns * key_width,
			       layout->rows * key_height);


	ips = wl_input_panel_get_input_panel_surface(virtual_keyboard->input_panel,
						     window_get_wl_surface(keyboard->window));

	wl_input_panel_surface_set_toplevel(ips,
					    output_get_wl_output(output),
					    WL_INPUT_PANEL_SURFACE_POSITION_CENTER_BOTTOM);
}

int
main(int argc, char *argv[])
{
	struct virtual_keyboard virtual_keyboard;
	struct output *output;

	memset(&virtual_keyboard, 0, sizeof virtual_keyboard);

	virtual_keyboard.display = display_create(&argc, argv);
	if (virtual_keyboard.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	display_set_user_data(virtual_keyboard.display, &virtual_keyboard);
	display_set_global_handler(virtual_keyboard.display, global_handler);

	output = display_get_output(virtual_keyboard.display);
	keyboard_create(output, &virtual_keyboard);

	display_run(virtual_keyboard.display);

	return 0;
}
