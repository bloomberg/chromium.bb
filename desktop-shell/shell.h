/*
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2011-2012 Collabora, Ltd.
 * Copyright © 2013 Raspberry Pi Foundation
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

#include <stdbool.h>

#include "compositor.h"

enum animation_type {
	ANIMATION_NONE,

	ANIMATION_ZOOM,
	ANIMATION_FADE,
	ANIMATION_DIM_LAYER,
};

enum fade_type {
	FADE_IN,
	FADE_OUT
};

enum exposay_target_state {
	EXPOSAY_TARGET_OVERVIEW, /* show all windows */
	EXPOSAY_TARGET_CANCEL, /* return to normal, same focus */
	EXPOSAY_TARGET_SWITCH, /* return to normal, switch focus */
};

enum exposay_layout_state {
	EXPOSAY_LAYOUT_INACTIVE = 0, /* normal desktop */
	EXPOSAY_LAYOUT_ANIMATE_TO_INACTIVE, /* in transition to normal */
	EXPOSAY_LAYOUT_OVERVIEW, /* show all windows */
	EXPOSAY_LAYOUT_ANIMATE_TO_OVERVIEW, /* in transition to all windows */
};

struct exposay_output {
	int num_surfaces;
	int grid_size;
	int surface_size;

	int hpadding_outer;
	int vpadding_outer;
	int padding_inner;
};

struct exposay {
	/* XXX: Make these exposay_surfaces. */
	struct weston_view *focus_prev;
	struct weston_view *focus_current;
	struct weston_view *clicked;
	struct workspace *workspace;
	struct weston_seat *seat;

	struct wl_list surface_list;

	struct weston_keyboard_grab grab_kbd;
	struct weston_pointer_grab grab_ptr;

	enum exposay_target_state state_target;
	enum exposay_layout_state state_cur;
	int in_flight; /* number of animations still running */

	int row_current;
	int column_current;
	struct exposay_output *cur_output;

	bool mod_pressed;
	bool mod_invalid;
};

struct focus_surface {
	struct weston_surface *surface;
	struct weston_view *view;
	struct weston_transform workspace_transform;
};

struct workspace {
	struct weston_layer layer;

	struct wl_list focus_list;
	struct wl_listener seat_destroyed_listener;

	struct focus_surface *fsurf_front;
	struct focus_surface *fsurf_back;
	struct weston_view_animation *focus_animation;
};

struct shell_output {
	struct desktop_shell  *shell;
	struct weston_output  *output;
	struct exposay_output eoutput;
	struct wl_listener    destroy_listener;
	struct wl_list        link;
};

struct desktop_shell {
	struct weston_compositor *compositor;

	struct wl_listener idle_listener;
	struct wl_listener wake_listener;
	struct wl_listener destroy_listener;
	struct wl_listener show_input_panel_listener;
	struct wl_listener hide_input_panel_listener;
	struct wl_listener update_input_panel_listener;

	struct weston_layer fullscreen_layer;
	struct weston_layer panel_layer;
	struct weston_layer background_layer;
	struct weston_layer lock_layer;
	struct weston_layer input_panel_layer;

	struct wl_listener pointer_focus_listener;
	struct weston_surface *grab_surface;

	struct {
		struct weston_process process;
		struct wl_client *client;
		struct wl_resource *desktop_shell;
		struct wl_listener client_destroy_listener;

		unsigned deathcount;
		uint32_t deathstamp;
	} child;

	bool locked;
	bool showing_input_panels;
	bool prepare_event_sent;

	struct {
		struct weston_surface *surface;
		pixman_box32_t cursor_rectangle;
	} text_input;

	struct weston_surface *lock_surface;
	struct wl_listener lock_surface_listener;

	struct {
		struct wl_array array;
		unsigned int current;
		unsigned int num;

		struct wl_list client_list;

		struct weston_animation animation;
		struct wl_list anim_sticky_list;
		int anim_dir;
		uint32_t anim_timestamp;
		double anim_current;
		struct workspace *anim_from;
		struct workspace *anim_to;
	} workspaces;

	struct {
		char *path;
		int duration;
		struct wl_resource *binding;
		struct weston_process process;
		struct wl_event_source *timer;
	} screensaver;

	struct {
		struct wl_resource *binding;
		struct wl_list surfaces;
	} input_panel;

	struct {
		struct weston_view *view;
		struct weston_view_animation *animation;
		enum fade_type type;
		struct wl_event_source *startup_timer;
	} fade;

	struct exposay exposay;

	uint32_t binding_modifier;
	uint32_t exposay_modifier;
	enum animation_type win_animation_type;
	enum animation_type startup_animation_type;
	enum animation_type focus_animation_type;

	struct weston_layer minimized_layer;

	struct wl_listener output_create_listener;
	struct wl_listener output_move_listener;
	struct wl_list output_list;

	char *client;
};

void
set_alpha_if_fullscreen(struct shell_surface *shsurf);

struct weston_output *
get_default_output(struct weston_compositor *compositor);

struct weston_view *
get_default_view(struct weston_surface *surface);

struct shell_surface *
get_shell_surface(struct weston_surface *surface);

struct workspace *
get_current_workspace(struct desktop_shell *shell);

void
lower_fullscreen_layer(struct desktop_shell *shell);

void
activate(struct desktop_shell *shell, struct weston_surface *es,
	 struct weston_seat *seat);

void
exposay_binding(struct weston_seat *seat,
		enum weston_keyboard_modifier modifier,
		void *data);
int
input_panel_setup(struct desktop_shell *shell);
void
input_panel_destroy(struct desktop_shell *shell);

typedef void (*shell_for_each_layer_func_t)(struct desktop_shell *,
					    struct weston_layer *, void *);

void
shell_for_each_layer(struct desktop_shell *shell,
		     shell_for_each_layer_func_t func,
		     void *data);
