/*
 * Copyright © 2012 Intel Corporation
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "keyboard-utils.h"

struct keyboard_input {
	struct xkb_context *xkb_context;

	uint32_t modifiers;

	struct xkb_keymap *keymap;
	struct xkb_state *state;
	xkb_mod_mask_t control_mask;
	xkb_mod_mask_t alt_mask;
	xkb_mod_mask_t shift_mask;

	void *user_data;
	keyboard_input_key_handler_t key_handler;
};

struct keyboard_input*
keyboard_input_create(struct xkb_context *xkb_context)
{
	struct keyboard_input *keyboard_input;

	keyboard_input = calloc(1, sizeof *keyboard_input);
	keyboard_input->xkb_context = xkb_context;

	return keyboard_input;
}

void
keyboard_input_destroy(struct keyboard_input *keyboard_input)
{
	free(keyboard_input);
}

void
keyboard_input_handle_keymap(struct keyboard_input *keyboard_input,
			     uint32_t format, int fd, uint32_t size)
{
	char *map_str;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED) {
		close(fd);
		return;
	}

	keyboard_input->keymap = xkb_map_new_from_string(keyboard_input->xkb_context,
							 map_str,
							 XKB_KEYMAP_FORMAT_TEXT_V1,
							 0);

	munmap(map_str, size);
	close(fd);

	if (!keyboard_input->keymap) {
		fprintf(stderr, "failed to compile keymap\n");
		return;
	}

	keyboard_input->state = xkb_state_new(keyboard_input->keymap);
	if (!keyboard_input->state) {
		fprintf(stderr, "failed to create XKB state\n");
		xkb_map_unref(keyboard_input->keymap);
		return;
	}

	keyboard_input->control_mask =
		1 << xkb_map_mod_get_index(keyboard_input->keymap, "Control");
	keyboard_input->alt_mask =
		1 << xkb_map_mod_get_index(keyboard_input->keymap, "Mod1");
	keyboard_input->shift_mask =
		1 << xkb_map_mod_get_index(keyboard_input->keymap, "Shift");
}

void
keyboard_input_handle_key(struct keyboard_input *keyboard_input,
			  uint32_t serial, uint32_t time,
			  uint32_t key, uint32_t state_w)
{
	uint32_t code;
	uint32_t num_syms;
	const xkb_keysym_t *syms;
	xkb_keysym_t sym;
	enum wl_keyboard_key_state state = state_w;

	if (!keyboard_input->state)
		return;

	code = key + 8;
	num_syms = xkb_key_get_syms(keyboard_input->state, code, &syms);

	sym = XKB_KEY_NoSymbol;
	if (num_syms == 1)
		sym = syms[0];

	if (keyboard_input->key_handler)
		(*keyboard_input->key_handler)(keyboard_input, time, key, sym,
					       state, keyboard_input->user_data);
}

void
keyboard_input_handle_modifiers(struct keyboard_input *keyboard_input,
				uint32_t serial, uint32_t mods_depressed,
				uint32_t mods_latched, uint32_t mods_locked,
				uint32_t group)
{
	xkb_mod_mask_t mask;

	xkb_state_update_mask(keyboard_input->state, mods_depressed, mods_latched,
			      mods_locked, 0, 0, group);
	mask = xkb_state_serialize_mods(keyboard_input->state,
					XKB_STATE_DEPRESSED |
					XKB_STATE_LATCHED);

	keyboard_input->modifiers = 0;
	if (mask & keyboard_input->control_mask)
		keyboard_input->modifiers |= MOD_CONTROL_MASK;
	if (mask & keyboard_input->alt_mask)
		keyboard_input->modifiers |= MOD_ALT_MASK;
	if (mask & keyboard_input->shift_mask)
		keyboard_input->modifiers |= MOD_SHIFT_MASK;

}

void
keyboard_input_set_user_data(struct keyboard_input *keyboard_input, void *data)
{
	keyboard_input->user_data = data;
}

void *
keyboard_input_get_user_data(struct keyboard_input *keyboard_input)
{
	return keyboard_input->user_data;
}

void
keyboard_input_set_key_handler(struct keyboard_input *keyboard_input,
			       keyboard_input_key_handler_t handler)
{
	keyboard_input->key_handler = handler;
}
