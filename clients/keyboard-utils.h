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

#ifndef _KEYBOARD_UTILS_H_
#define _KEYBOARD_UTILS_H_

#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>

struct keyboard_input;

typedef void (*keyboard_input_key_handler_t)(struct keyboard_input *keyboard_input,
					     uint32_t time, uint32_t key, uint32_t unicode,
					     enum wl_keyboard_key_state state, void *data);

struct keyboard_input*
keyboard_input_create(struct xkb_context *xkb_context);
void
keyboard_input_destroy(struct keyboard_input *keyboard_input);

void
keyboard_input_handle_keymap(struct keyboard_input *keyboard_input,
			     uint32_t format, int fd, uint32_t size);
void
keyboard_input_handle_key(struct keyboard_input *keyboard_input,
			  uint32_t serial, uint32_t time,
			  uint32_t key, uint32_t state_w);
void
keyboard_input_handle_modifiers(struct keyboard_input *keyboard_input,
				uint32_t serial, uint32_t mods_depressed,
				uint32_t mods_latched, uint32_t mods_locked,
				uint32_t group);

void
keyboard_input_set_user_data(struct keyboard_input *keyboard_input, void *data);

void *
keyboard_input_get_user_data(struct keyboard_input *keyboard_input);

void
keyboard_input_set_key_handler(struct keyboard_input *keyboard_input,
			       keyboard_input_key_handler_t handler);

#define MOD_SHIFT_MASK		0x01
#define MOD_ALT_MASK		0x02
#define MOD_CONTROL_MASK	0x04

#endif
