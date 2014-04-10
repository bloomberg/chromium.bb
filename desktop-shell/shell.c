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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <assert.h>
#include <signal.h>
#include <math.h>
#include <sys/types.h>

#include "shell.h"
#include "desktop-shell-server-protocol.h"
#include "workspaces-server-protocol.h"
#include "../shared/config-parser.h"
#include "xdg-shell-server-protocol.h"

#define DEFAULT_NUM_WORKSPACES 1
#define DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH 200

#ifndef static_assert
#define static_assert(cond, msg)
#endif

struct focus_state {
	struct weston_seat *seat;
	struct workspace *ws;
	struct weston_surface *keyboard_focus;
	struct wl_list link;
	struct wl_listener seat_destroy_listener;
	struct wl_listener surface_destroy_listener;
};

enum shell_surface_type {
	SHELL_SURFACE_NONE,
	SHELL_SURFACE_TOPLEVEL,
	SHELL_SURFACE_POPUP,
	SHELL_SURFACE_XWAYLAND
};

/*
 * Surface stacking and ordering.
 *
 * This is handled using several linked lists of surfaces, organised into
 * ‘layers’. The layers are ordered, and each of the surfaces in one layer are
 * above all of the surfaces in the layer below. The set of layers is static and
 * in the following order (top-most first):
 *  • Lock layer (only ever displayed on its own)
 *  • Cursor layer
 *  • Input panel layer
 *  • Fullscreen layer
 *  • Panel layer
 *  • Workspace layers
 *  • Background layer
 *
 * The list of layers may be manipulated to remove whole layers of surfaces from
 * display. For example, when locking the screen, all layers except the lock
 * layer are removed.
 *
 * A surface’s layer is modified on configuring the surface, in
 * set_surface_type() (which is only called when the surface’s type change is
 * _committed_). If a surface’s type changes (e.g. when making a window
 * fullscreen) its layer changes too.
 *
 * In order to allow popup and transient surfaces to be correctly stacked above
 * their parent surfaces, each surface tracks both its parent surface, and a
 * linked list of its children. When a surface’s layer is updated, so are the
 * layers of its children. Note that child surfaces are *not* the same as
 * subsurfaces — child/parent surfaces are purely for maintaining stacking
 * order.
 *
 * The children_link list of siblings of a surface (i.e. those surfaces which
 * have the same parent) only contains weston_surfaces which have a
 * shell_surface. Stacking is not implemented for non-shell_surface
 * weston_surfaces. This means that the following implication does *not* hold:
 *     (shsurf->parent != NULL) ⇒ !wl_list_is_empty(shsurf->children_link)
 */

struct shell_surface {
	struct wl_resource *resource;
	struct wl_signal destroy_signal;

	struct weston_surface *surface;
	struct weston_view *view;
	int32_t last_width, last_height;
	struct wl_listener surface_destroy_listener;
	struct wl_listener resource_destroy_listener;

	struct weston_surface *parent;
	struct wl_list children_list;  /* child surfaces of this one */
	struct wl_list children_link;  /* sibling surfaces of this one */
	struct desktop_shell *shell;

	enum shell_surface_type type;
	char *title, *class;
	int32_t saved_x, saved_y;
	int32_t saved_width, saved_height;
	bool saved_position_valid;
	bool saved_size_valid;
	bool saved_rotation_valid;
	int unresponsive, grabbed;
	uint32_t resize_edges;

	struct {
		struct weston_transform transform;
		struct weston_matrix rotation;
	} rotation;

	struct {
		struct wl_list grab_link;
		int32_t x, y;
		struct shell_seat *shseat;
		uint32_t serial;
	} popup;

	struct {
		int32_t x, y;
		uint32_t flags;
	} transient;

	struct {
		enum wl_shell_surface_fullscreen_method type;
		struct weston_transform transform; /* matrix from x, y */
		uint32_t framerate;
		struct weston_view *black_view;
	} fullscreen;

	struct weston_transform workspace_transform;

	struct weston_output *fullscreen_output;
	struct weston_output *output;
	struct weston_output *recommended_output;
	struct wl_list link;

	const struct weston_shell_client *client;

	struct {
		bool maximized;
		bool fullscreen;
		bool relative;
	} state, next_state, requested_state; /* surface states */
	bool state_changed;
	bool state_requested;

	int focus_count;
};

struct shell_grab {
	struct weston_pointer_grab grab;
	struct shell_surface *shsurf;
	struct wl_listener shsurf_destroy_listener;
};

struct shell_touch_grab {
	struct weston_touch_grab grab;
	struct shell_surface *shsurf;
	struct wl_listener shsurf_destroy_listener;
	struct weston_touch *touch;
};

struct weston_move_grab {
	struct shell_grab base;
	wl_fixed_t dx, dy;
};

struct weston_touch_move_grab {
	struct shell_touch_grab base;
	int active;
	wl_fixed_t dx, dy;
};

struct rotate_grab {
	struct shell_grab base;
	struct weston_matrix rotation;
	struct {
		float x;
		float y;
	} center;
};

struct shell_seat {
	struct weston_seat *seat;
	struct wl_listener seat_destroy_listener;
	struct weston_surface *focused_surface;

	struct {
		struct weston_pointer_grab grab;
		struct wl_list surfaces_list;
		struct wl_client *client;
		int32_t initial_up;
	} popup_grab;
};

struct shell_client {
	struct wl_resource *resource;
	struct wl_client *client;
	struct desktop_shell *shell;
	struct wl_listener destroy_listener;
	struct wl_event_source *ping_timer;
	uint32_t ping_serial;
	int unresponsive;
};

void
set_alpha_if_fullscreen(struct shell_surface *shsurf)
{
	if (shsurf && shsurf->state.fullscreen)
		shsurf->fullscreen.black_view->alpha = 0.25;
}

static struct shell_client *
get_shell_client(struct wl_client *client);

static struct desktop_shell *
shell_surface_get_shell(struct shell_surface *shsurf);

static void
surface_rotate(struct shell_surface *surface, struct weston_seat *seat);

static void
shell_fade_startup(struct desktop_shell *shell);

static struct shell_seat *
get_shell_seat(struct weston_seat *seat);

static void
shell_surface_update_child_surface_layers(struct shell_surface *shsurf);

static bool
shell_surface_is_wl_shell_surface(struct shell_surface *shsurf);

static bool
shell_surface_is_xdg_surface(struct shell_surface *shsurf);

static bool
shell_surface_is_xdg_popup(struct shell_surface *shsurf);

static void
shell_surface_set_parent(struct shell_surface *shsurf,
                         struct weston_surface *parent);

static bool
shell_surface_is_top_fullscreen(struct shell_surface *shsurf)
{
	struct desktop_shell *shell;
	struct weston_view *top_fs_ev;

	shell = shell_surface_get_shell(shsurf);

	if (wl_list_empty(&shell->fullscreen_layer.view_list))
		return false;

	top_fs_ev = container_of(shell->fullscreen_layer.view_list.next,
			         struct weston_view,
				 layer_link);
	return (shsurf == get_shell_surface(top_fs_ev->surface));
}

static void
destroy_shell_grab_shsurf(struct wl_listener *listener, void *data)
{
	struct shell_grab *grab;

	grab = container_of(listener, struct shell_grab,
			    shsurf_destroy_listener);

	grab->shsurf = NULL;
}

struct weston_view *
get_default_view(struct weston_surface *surface)
{
	struct shell_surface *shsurf;
	struct weston_view *view;

	if (!surface || wl_list_empty(&surface->views))
		return NULL;

	shsurf = get_shell_surface(surface);
	if (shsurf)
		return shsurf->view;

	wl_list_for_each(view, &surface->views, surface_link)
		if (weston_view_is_mapped(view))
			return view;

	return container_of(surface->views.next, struct weston_view, surface_link);
}

static void
popup_grab_end(struct weston_pointer *pointer);

static void
shell_grab_start(struct shell_grab *grab,
		 const struct weston_pointer_grab_interface *interface,
		 struct shell_surface *shsurf,
		 struct weston_pointer *pointer,
		 enum desktop_shell_cursor cursor)
{
	struct desktop_shell *shell = shsurf->shell;

	popup_grab_end(pointer);

	grab->grab.interface = interface;
	grab->shsurf = shsurf;
	grab->shsurf_destroy_listener.notify = destroy_shell_grab_shsurf;
	wl_signal_add(&shsurf->destroy_signal,
		      &grab->shsurf_destroy_listener);

	shsurf->grabbed = 1;
	weston_pointer_start_grab(pointer, &grab->grab);
	if (shell->child.desktop_shell) {
		desktop_shell_send_grab_cursor(shell->child.desktop_shell,
					       cursor);
		weston_pointer_set_focus(pointer,
					 get_default_view(shell->grab_surface),
					 wl_fixed_from_int(0),
					 wl_fixed_from_int(0));
	}
}

static void
shell_grab_end(struct shell_grab *grab)
{
	if (grab->shsurf) {
		wl_list_remove(&grab->shsurf_destroy_listener.link);
		grab->shsurf->grabbed = 0;
		grab->shsurf->resize_edges = 0;
	}

	weston_pointer_end_grab(grab->grab.pointer);
}

static void
shell_touch_grab_start(struct shell_touch_grab *grab,
		       const struct weston_touch_grab_interface *interface,
		       struct shell_surface *shsurf,
		       struct weston_touch *touch)
{
	struct desktop_shell *shell = shsurf->shell;

	grab->grab.interface = interface;
	grab->shsurf = shsurf;
	grab->shsurf_destroy_listener.notify = destroy_shell_grab_shsurf;
	wl_signal_add(&shsurf->destroy_signal,
		      &grab->shsurf_destroy_listener);

	grab->touch = touch;
	shsurf->grabbed = 1;

	weston_touch_start_grab(touch, &grab->grab);
	if (shell->child.desktop_shell)
		weston_touch_set_focus(touch->seat,
				       get_default_view(shell->grab_surface));
}

static void
shell_touch_grab_end(struct shell_touch_grab *grab)
{
	if (grab->shsurf) {
		wl_list_remove(&grab->shsurf_destroy_listener.link);
		grab->shsurf->grabbed = 0;
	}

	weston_touch_end_grab(grab->touch);
}

static void
center_on_output(struct weston_view *view,
		 struct weston_output *output);

static enum weston_keyboard_modifier
get_modifier(char *modifier)
{
	if (!modifier)
		return MODIFIER_SUPER;

	if (!strcmp("ctrl", modifier))
		return MODIFIER_CTRL;
	else if (!strcmp("alt", modifier))
		return MODIFIER_ALT;
	else if (!strcmp("super", modifier))
		return MODIFIER_SUPER;
	else
		return MODIFIER_SUPER;
}

static enum animation_type
get_animation_type(char *animation)
{
	if (!animation)
		return ANIMATION_NONE;

	if (!strcmp("zoom", animation))
		return ANIMATION_ZOOM;
	else if (!strcmp("fade", animation))
		return ANIMATION_FADE;
	else if (!strcmp("dim-layer", animation))
		return ANIMATION_DIM_LAYER;
	else
		return ANIMATION_NONE;
}

static void
shell_configuration(struct desktop_shell *shell)
{
	struct weston_config_section *section;
	int duration;
	char *s;

	section = weston_config_get_section(shell->compositor->config,
					    "screensaver", NULL, NULL);
	weston_config_section_get_string(section,
					 "path", &shell->screensaver.path, NULL);
	weston_config_section_get_int(section, "duration", &duration, 60);
	shell->screensaver.duration = duration * 1000;

	section = weston_config_get_section(shell->compositor->config,
					    "shell", NULL, NULL);
	weston_config_section_get_string(section,
					 "client", &s, LIBEXECDIR "/" WESTON_SHELL_CLIENT);
	shell->client = s;
	weston_config_section_get_string(section,
					 "binding-modifier", &s, "super");
	shell->binding_modifier = get_modifier(s);
	free(s);

	weston_config_section_get_string(section,
					 "exposay-modifier", &s, "none");
	if (strcmp(s, "none") == 0)
		shell->exposay_modifier = 0;
	else
		shell->exposay_modifier = get_modifier(s);
	free(s);

	weston_config_section_get_string(section, "animation", &s, "none");
	shell->win_animation_type = get_animation_type(s);
	free(s);
	weston_config_section_get_string(section,
					 "startup-animation", &s, "fade");
	shell->startup_animation_type = get_animation_type(s);
	free(s);
	if (shell->startup_animation_type == ANIMATION_ZOOM)
		shell->startup_animation_type = ANIMATION_NONE;
	weston_config_section_get_string(section, "focus-animation", &s, "none");
	shell->focus_animation_type = get_animation_type(s);
	free(s);
	weston_config_section_get_uint(section, "num-workspaces",
				       &shell->workspaces.num,
				       DEFAULT_NUM_WORKSPACES);
}

struct weston_output *
get_default_output(struct weston_compositor *compositor)
{
	return container_of(compositor->output_list.next,
			    struct weston_output, link);
}


/* no-op func for checking focus surface */
static void
focus_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
}

static struct focus_surface *
get_focus_surface(struct weston_surface *surface)
{
	if (surface->configure == focus_surface_configure)
		return surface->configure_private;
	else
		return NULL;
}

static bool
is_focus_surface (struct weston_surface *es)
{
	return (es->configure == focus_surface_configure);
}

static bool
is_focus_view (struct weston_view *view)
{
	return is_focus_surface (view->surface);
}

static struct focus_surface *
create_focus_surface(struct weston_compositor *ec,
		     struct weston_output *output)
{
	struct focus_surface *fsurf = NULL;
	struct weston_surface *surface = NULL;

	fsurf = malloc(sizeof *fsurf);
	if (!fsurf)
		return NULL;

	fsurf->surface = weston_surface_create(ec);
	surface = fsurf->surface;
	if (surface == NULL) {
		free(fsurf);
		return NULL;
	}

	surface->configure = focus_surface_configure;
	surface->output = output;
	surface->configure_private = fsurf;

	fsurf->view = weston_view_create(surface);
	if (fsurf->view == NULL) {
		weston_surface_destroy(surface);
		free(fsurf);
		return NULL;
	}
	fsurf->view->output = output;

	weston_surface_set_size(surface, output->width, output->height);
	weston_view_set_position(fsurf->view, output->x, output->y);
	weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1.0);
	pixman_region32_fini(&surface->opaque);
	pixman_region32_init_rect(&surface->opaque, output->x, output->y,
				  output->width, output->height);
	pixman_region32_fini(&surface->input);
	pixman_region32_init(&surface->input);

	wl_list_init(&fsurf->workspace_transform.link);

	return fsurf;
}

static void
focus_surface_destroy(struct focus_surface *fsurf)
{
	weston_surface_destroy(fsurf->surface);
	free(fsurf);
}

static void
focus_animation_done(struct weston_view_animation *animation, void *data)
{
	struct workspace *ws = data;

	ws->focus_animation = NULL;
}

static void
focus_state_destroy(struct focus_state *state)
{
	wl_list_remove(&state->seat_destroy_listener.link);
	wl_list_remove(&state->surface_destroy_listener.link);
	free(state);
}

static void
focus_state_seat_destroy(struct wl_listener *listener, void *data)
{
	struct focus_state *state = container_of(listener,
						 struct focus_state,
						 seat_destroy_listener);

	wl_list_remove(&state->link);
	focus_state_destroy(state);
}

static void
focus_state_surface_destroy(struct wl_listener *listener, void *data)
{
	struct focus_state *state = container_of(listener,
						 struct focus_state,
						 surface_destroy_listener);
	struct desktop_shell *shell;
	struct weston_surface *main_surface, *next;
	struct weston_view *view;

	main_surface = weston_surface_get_main_surface(state->keyboard_focus);

	next = NULL;
	wl_list_for_each(view, &state->ws->layer.view_list, layer_link) {
		if (view->surface == main_surface)
			continue;
		if (is_focus_view(view))
			continue;

		next = view->surface;
		break;
	}

	/* if the focus was a sub-surface, activate its main surface */
	if (main_surface != state->keyboard_focus)
		next = main_surface;

	shell = state->seat->compositor->shell_interface.shell;
	if (next) {
		state->keyboard_focus = NULL;
		activate(shell, next, state->seat);
	} else {
		if (shell->focus_animation_type == ANIMATION_DIM_LAYER) {
			if (state->ws->focus_animation)
				weston_view_animation_destroy(state->ws->focus_animation);

			state->ws->focus_animation = weston_fade_run(
				state->ws->fsurf_front->view,
				state->ws->fsurf_front->view->alpha, 0.0, 300,
				focus_animation_done, state->ws);
		}

		wl_list_remove(&state->link);
		focus_state_destroy(state);
	}
}

static struct focus_state *
focus_state_create(struct weston_seat *seat, struct workspace *ws)
{
	struct focus_state *state;

	state = malloc(sizeof *state);
	if (state == NULL)
		return NULL;

	state->keyboard_focus = NULL;
	state->ws = ws;
	state->seat = seat;
	wl_list_insert(&ws->focus_list, &state->link);

	state->seat_destroy_listener.notify = focus_state_seat_destroy;
	state->surface_destroy_listener.notify = focus_state_surface_destroy;
	wl_signal_add(&seat->destroy_signal,
		      &state->seat_destroy_listener);
	wl_list_init(&state->surface_destroy_listener.link);

	return state;
}

static struct focus_state *
ensure_focus_state(struct desktop_shell *shell, struct weston_seat *seat)
{
	struct workspace *ws = get_current_workspace(shell);
	struct focus_state *state;

	wl_list_for_each(state, &ws->focus_list, link)
		if (state->seat == seat)
			break;

	if (&state->link == &ws->focus_list)
		state = focus_state_create(seat, ws);

	return state;
}

static void
focus_state_set_focus(struct focus_state *state,
		      struct weston_surface *surface)
{
	if (state->keyboard_focus) {
		wl_list_remove(&state->surface_destroy_listener.link);
		wl_list_init(&state->surface_destroy_listener.link);
	}

	state->keyboard_focus = surface;
	if (surface)
		wl_signal_add(&surface->destroy_signal,
			      &state->surface_destroy_listener);
}

static void
restore_focus_state(struct desktop_shell *shell, struct workspace *ws)
{
	struct focus_state *state, *next;
	struct weston_surface *surface;
	struct wl_list pending_seat_list;
	struct weston_seat *seat, *next_seat;

	/* Temporarily steal the list of seats so that we can keep
	 * track of the seats we've already processed */
	wl_list_init(&pending_seat_list);
	wl_list_insert_list(&pending_seat_list, &shell->compositor->seat_list);
	wl_list_init(&shell->compositor->seat_list);

	wl_list_for_each_safe(state, next, &ws->focus_list, link) {
		wl_list_remove(&state->seat->link);
		wl_list_insert(&shell->compositor->seat_list,
			       &state->seat->link);

		if (state->seat->keyboard == NULL)
			continue;

		surface = state->keyboard_focus;

		weston_keyboard_set_focus(state->seat->keyboard, surface);
	}

	/* For any remaining seats that we don't have a focus state
	 * for we'll reset the keyboard focus to NULL */
	wl_list_for_each_safe(seat, next_seat, &pending_seat_list, link) {
		wl_list_insert(&shell->compositor->seat_list, &seat->link);

		if (state->seat->keyboard == NULL)
			continue;

		weston_keyboard_set_focus(seat->keyboard, NULL);
	}
}

static void
replace_focus_state(struct desktop_shell *shell, struct workspace *ws,
		    struct weston_seat *seat)
{
	struct focus_state *state;

	wl_list_for_each(state, &ws->focus_list, link) {
		if (state->seat == seat) {
			focus_state_set_focus(state, seat->keyboard->focus);
			return;
		}
	}
}

static void
drop_focus_state(struct desktop_shell *shell, struct workspace *ws,
		 struct weston_surface *surface)
{
	struct focus_state *state;

	wl_list_for_each(state, &ws->focus_list, link)
		if (state->keyboard_focus == surface)
			focus_state_set_focus(state, NULL);
}

static void
animate_focus_change(struct desktop_shell *shell, struct workspace *ws,
		     struct weston_view *from, struct weston_view *to)
{
	struct weston_output *output;
	bool focus_surface_created = false;

	/* FIXME: Only support dim animation using two layers */
	if (from == to || shell->focus_animation_type != ANIMATION_DIM_LAYER)
		return;

	output = get_default_output(shell->compositor);
	if (ws->fsurf_front == NULL && (from || to)) {
		ws->fsurf_front = create_focus_surface(shell->compositor, output);
		if (ws->fsurf_front == NULL)
			return;
		ws->fsurf_front->view->alpha = 0.0;

		ws->fsurf_back = create_focus_surface(shell->compositor, output);
		if (ws->fsurf_back == NULL) {
			focus_surface_destroy(ws->fsurf_front);
			return;
		}
		ws->fsurf_back->view->alpha = 0.0;

		focus_surface_created = true;
	} else {
		wl_list_remove(&ws->fsurf_front->view->layer_link);
		wl_list_remove(&ws->fsurf_back->view->layer_link);
	}

	if (ws->focus_animation) {
		weston_view_animation_destroy(ws->focus_animation);
		ws->focus_animation = NULL;
	}

	if (to)
		wl_list_insert(&to->layer_link,
			       &ws->fsurf_front->view->layer_link);
	else if (from)
		wl_list_insert(&ws->layer.view_list,
			       &ws->fsurf_front->view->layer_link);

	if (focus_surface_created) {
		ws->focus_animation = weston_fade_run(
			ws->fsurf_front->view,
			ws->fsurf_front->view->alpha, 0.6, 300,
			focus_animation_done, ws);
	} else if (from) {
		wl_list_insert(&from->layer_link,
			       &ws->fsurf_back->view->layer_link);
		ws->focus_animation = weston_stable_fade_run(
			ws->fsurf_front->view, 0.0,
			ws->fsurf_back->view, 0.6,
			focus_animation_done, ws);
	} else if (to) {
		wl_list_insert(&ws->layer.view_list,
			       &ws->fsurf_back->view->layer_link);
		ws->focus_animation = weston_stable_fade_run(
			ws->fsurf_front->view, 0.0,
			ws->fsurf_back->view, 0.6,
			focus_animation_done, ws);
	}
}

static void
workspace_destroy(struct workspace *ws)
{
	struct focus_state *state, *next;

	wl_list_for_each_safe(state, next, &ws->focus_list, link)
		focus_state_destroy(state);

	if (ws->fsurf_front)
		focus_surface_destroy(ws->fsurf_front);
	if (ws->fsurf_back)
		focus_surface_destroy(ws->fsurf_back);

	free(ws);
}

static void
seat_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_seat *seat = data;
	struct focus_state *state, *next;
	struct workspace *ws = container_of(listener,
					    struct workspace,
					    seat_destroyed_listener);

	wl_list_for_each_safe(state, next, &ws->focus_list, link)
		if (state->seat == seat)
			wl_list_remove(&state->link);
}

static struct workspace *
workspace_create(void)
{
	struct workspace *ws = malloc(sizeof *ws);
	if (ws == NULL)
		return NULL;

	weston_layer_init(&ws->layer, NULL);

	wl_list_init(&ws->focus_list);
	wl_list_init(&ws->seat_destroyed_listener.link);
	ws->seat_destroyed_listener.notify = seat_destroyed;
	ws->fsurf_front = NULL;
	ws->fsurf_back = NULL;
	ws->focus_animation = NULL;

	return ws;
}

static int
workspace_is_empty(struct workspace *ws)
{
	return wl_list_empty(&ws->layer.view_list);
}

static struct workspace *
get_workspace(struct desktop_shell *shell, unsigned int index)
{
	struct workspace **pws = shell->workspaces.array.data;
	assert(index < shell->workspaces.num);
	pws += index;
	return *pws;
}

struct workspace *
get_current_workspace(struct desktop_shell *shell)
{
	return get_workspace(shell, shell->workspaces.current);
}

static void
activate_workspace(struct desktop_shell *shell, unsigned int index)
{
	struct workspace *ws;

	ws = get_workspace(shell, index);
	wl_list_insert(&shell->panel_layer.link, &ws->layer.link);

	shell->workspaces.current = index;
}

static unsigned int
get_output_height(struct weston_output *output)
{
	return abs(output->region.extents.y1 - output->region.extents.y2);
}

static void
view_translate(struct workspace *ws, struct weston_view *view, double d)
{
	struct weston_transform *transform;

	if (is_focus_view(view)) {
		struct focus_surface *fsurf = get_focus_surface(view->surface);
		transform = &fsurf->workspace_transform;
	} else {
		struct shell_surface *shsurf = get_shell_surface(view->surface);
		transform = &shsurf->workspace_transform;
	}

	if (wl_list_empty(&transform->link))
		wl_list_insert(view->geometry.transformation_list.prev,
			       &transform->link);

	weston_matrix_init(&transform->matrix);
	weston_matrix_translate(&transform->matrix,
				0.0, d, 0.0);
	weston_view_geometry_dirty(view);
}

static void
workspace_translate_out(struct workspace *ws, double fraction)
{
	struct weston_view *view;
	unsigned int height;
	double d;

	wl_list_for_each(view, &ws->layer.view_list, layer_link) {
		height = get_output_height(view->surface->output);
		d = height * fraction;

		view_translate(ws, view, d);
	}
}

static void
workspace_translate_in(struct workspace *ws, double fraction)
{
	struct weston_view *view;
	unsigned int height;
	double d;

	wl_list_for_each(view, &ws->layer.view_list, layer_link) {
		height = get_output_height(view->surface->output);

		if (fraction > 0)
			d = -(height - height * fraction);
		else
			d = height + height * fraction;

		view_translate(ws, view, d);
	}
}

static void
broadcast_current_workspace_state(struct desktop_shell *shell)
{
	struct wl_resource *resource;

	wl_resource_for_each(resource, &shell->workspaces.client_list)
		workspace_manager_send_state(resource,
					     shell->workspaces.current,
					     shell->workspaces.num);
}

static void
reverse_workspace_change_animation(struct desktop_shell *shell,
				   unsigned int index,
				   struct workspace *from,
				   struct workspace *to)
{
	shell->workspaces.current = index;

	shell->workspaces.anim_to = to;
	shell->workspaces.anim_from = from;
	shell->workspaces.anim_dir = -1 * shell->workspaces.anim_dir;
	shell->workspaces.anim_timestamp = 0;

	weston_compositor_schedule_repaint(shell->compositor);
}

static void
workspace_deactivate_transforms(struct workspace *ws)
{
	struct weston_view *view;
	struct weston_transform *transform;

	wl_list_for_each(view, &ws->layer.view_list, layer_link) {
		if (is_focus_view(view)) {
			struct focus_surface *fsurf = get_focus_surface(view->surface);
			transform = &fsurf->workspace_transform;
		} else {
			struct shell_surface *shsurf = get_shell_surface(view->surface);
			transform = &shsurf->workspace_transform;
		}

		if (!wl_list_empty(&transform->link)) {
			wl_list_remove(&transform->link);
			wl_list_init(&transform->link);
		}
		weston_view_geometry_dirty(view);
	}
}

static void
finish_workspace_change_animation(struct desktop_shell *shell,
				  struct workspace *from,
				  struct workspace *to)
{
	weston_compositor_schedule_repaint(shell->compositor);

	wl_list_remove(&shell->workspaces.animation.link);
	workspace_deactivate_transforms(from);
	workspace_deactivate_transforms(to);
	shell->workspaces.anim_to = NULL;

	wl_list_remove(&shell->workspaces.anim_from->layer.link);
}

static void
animate_workspace_change_frame(struct weston_animation *animation,
			       struct weston_output *output, uint32_t msecs)
{
	struct desktop_shell *shell =
		container_of(animation, struct desktop_shell,
			     workspaces.animation);
	struct workspace *from = shell->workspaces.anim_from;
	struct workspace *to = shell->workspaces.anim_to;
	uint32_t t;
	double x, y;

	if (workspace_is_empty(from) && workspace_is_empty(to)) {
		finish_workspace_change_animation(shell, from, to);
		return;
	}

	if (shell->workspaces.anim_timestamp == 0) {
		if (shell->workspaces.anim_current == 0.0)
			shell->workspaces.anim_timestamp = msecs;
		else
			shell->workspaces.anim_timestamp =
				msecs -
				/* Invers of movement function 'y' below. */
				(asin(1.0 - shell->workspaces.anim_current) *
				 DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH *
				 M_2_PI);
	}

	t = msecs - shell->workspaces.anim_timestamp;

	/*
	 * x = [0, π/2]
	 * y(x) = sin(x)
	 */
	x = t * (1.0/DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH) * M_PI_2;
	y = sin(x);

	if (t < DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH) {
		weston_compositor_schedule_repaint(shell->compositor);

		workspace_translate_out(from, shell->workspaces.anim_dir * y);
		workspace_translate_in(to, shell->workspaces.anim_dir * y);
		shell->workspaces.anim_current = y;

		weston_compositor_schedule_repaint(shell->compositor);
	}
	else
		finish_workspace_change_animation(shell, from, to);
}

static void
animate_workspace_change(struct desktop_shell *shell,
			 unsigned int index,
			 struct workspace *from,
			 struct workspace *to)
{
	struct weston_output *output;

	int dir;

	if (index > shell->workspaces.current)
		dir = -1;
	else
		dir = 1;

	shell->workspaces.current = index;

	shell->workspaces.anim_dir = dir;
	shell->workspaces.anim_from = from;
	shell->workspaces.anim_to = to;
	shell->workspaces.anim_current = 0.0;
	shell->workspaces.anim_timestamp = 0;

	output = container_of(shell->compositor->output_list.next,
			      struct weston_output, link);
	wl_list_insert(&output->animation_list,
		       &shell->workspaces.animation.link);

	wl_list_insert(from->layer.link.prev, &to->layer.link);

	workspace_translate_in(to, 0);

	restore_focus_state(shell, to);

	weston_compositor_schedule_repaint(shell->compositor);
}

static void
update_workspace(struct desktop_shell *shell, unsigned int index,
		 struct workspace *from, struct workspace *to)
{
	shell->workspaces.current = index;
	wl_list_insert(&from->layer.link, &to->layer.link);
	wl_list_remove(&from->layer.link);
}

static void
change_workspace(struct desktop_shell *shell, unsigned int index)
{
	struct workspace *from;
	struct workspace *to;
	struct focus_state *state;

	if (index == shell->workspaces.current)
		return;

	/* Don't change workspace when there is any fullscreen surfaces. */
	if (!wl_list_empty(&shell->fullscreen_layer.view_list))
		return;

	from = get_current_workspace(shell);
	to = get_workspace(shell, index);

	if (shell->workspaces.anim_from == to &&
	    shell->workspaces.anim_to == from) {
		restore_focus_state(shell, to);
		reverse_workspace_change_animation(shell, index, from, to);
		broadcast_current_workspace_state(shell);
		return;
	}

	if (shell->workspaces.anim_to != NULL)
		finish_workspace_change_animation(shell,
						  shell->workspaces.anim_from,
						  shell->workspaces.anim_to);

	restore_focus_state(shell, to);

	if (shell->focus_animation_type != ANIMATION_NONE) {
		wl_list_for_each(state, &from->focus_list, link)
			if (state->keyboard_focus)
				animate_focus_change(shell, from,
						     get_default_view(state->keyboard_focus), NULL);

		wl_list_for_each(state, &to->focus_list, link)
			if (state->keyboard_focus)
				animate_focus_change(shell, to,
						     NULL, get_default_view(state->keyboard_focus));
	}

	if (workspace_is_empty(to) && workspace_is_empty(from))
		update_workspace(shell, index, from, to);
	else
		animate_workspace_change(shell, index, from, to);

	broadcast_current_workspace_state(shell);
}

static bool
workspace_has_only(struct workspace *ws, struct weston_surface *surface)
{
	struct wl_list *list = &ws->layer.view_list;
	struct wl_list *e;

	if (wl_list_empty(list))
		return false;

	e = list->next;

	if (e->next != list)
		return false;

	return container_of(e, struct weston_view, layer_link)->surface == surface;
}

static void
move_surface_to_workspace(struct desktop_shell *shell,
                          struct shell_surface *shsurf,
                          uint32_t workspace)
{
	struct workspace *from;
	struct workspace *to;
	struct weston_seat *seat;
	struct weston_surface *focus;
	struct weston_view *view;

	if (workspace == shell->workspaces.current)
		return;

	view = get_default_view(shsurf->surface);
	if (!view)
		return;

	assert(weston_surface_get_main_surface(view->surface) == view->surface);

	if (workspace >= shell->workspaces.num)
		workspace = shell->workspaces.num - 1;

	from = get_current_workspace(shell);
	to = get_workspace(shell, workspace);

	wl_list_remove(&view->layer_link);
	wl_list_insert(&to->layer.view_list, &view->layer_link);

	shell_surface_update_child_surface_layers(shsurf);

	drop_focus_state(shell, from, view->surface);
	wl_list_for_each(seat, &shell->compositor->seat_list, link) {
		if (!seat->keyboard)
			continue;

		focus = weston_surface_get_main_surface(seat->keyboard->focus);
		if (focus == view->surface)
			weston_keyboard_set_focus(seat->keyboard, NULL);
	}

	weston_view_damage_below(view);
}

static void
take_surface_to_workspace_by_seat(struct desktop_shell *shell,
				  struct weston_seat *seat,
				  unsigned int index)
{
	struct weston_surface *surface;
	struct weston_view *view;
	struct shell_surface *shsurf;
	struct workspace *from;
	struct workspace *to;
	struct focus_state *state;

	surface = weston_surface_get_main_surface(seat->keyboard->focus);
	view = get_default_view(surface);
	if (view == NULL ||
	    index == shell->workspaces.current ||
	    is_focus_view(view))
		return;

	from = get_current_workspace(shell);
	to = get_workspace(shell, index);

	wl_list_remove(&view->layer_link);
	wl_list_insert(&to->layer.view_list, &view->layer_link);

	shsurf = get_shell_surface(surface);
	if (shsurf != NULL)
		shell_surface_update_child_surface_layers(shsurf);

	replace_focus_state(shell, to, seat);
	drop_focus_state(shell, from, surface);

	if (shell->workspaces.anim_from == to &&
	    shell->workspaces.anim_to == from) {
		wl_list_remove(&to->layer.link);
		wl_list_insert(from->layer.link.prev, &to->layer.link);

		reverse_workspace_change_animation(shell, index, from, to);
		broadcast_current_workspace_state(shell);

		return;
	}

	if (shell->workspaces.anim_to != NULL)
		finish_workspace_change_animation(shell,
						  shell->workspaces.anim_from,
						  shell->workspaces.anim_to);

	if (workspace_is_empty(from) &&
	    workspace_has_only(to, surface))
		update_workspace(shell, index, from, to);
	else {
		if (shsurf != NULL &&
		    wl_list_empty(&shsurf->workspace_transform.link))
			wl_list_insert(&shell->workspaces.anim_sticky_list,
				       &shsurf->workspace_transform.link);

		animate_workspace_change(shell, index, from, to);
	}

	broadcast_current_workspace_state(shell);

	state = ensure_focus_state(shell, seat);
	if (state != NULL)
		focus_state_set_focus(state, surface);
}

static void
workspace_manager_move_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource,
			       uint32_t workspace)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct weston_surface *main_surface;
	struct shell_surface *shell_surface;

	main_surface = weston_surface_get_main_surface(surface);
	shell_surface = get_shell_surface(main_surface);
	if (shell_surface == NULL)
		return;

	move_surface_to_workspace(shell, shell_surface, workspace);
}

static const struct workspace_manager_interface workspace_manager_implementation = {
	workspace_manager_move_surface,
};

static void
unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static void
bind_workspace_manager(struct wl_client *client,
		       void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client,
				      &workspace_manager_interface, 1, id);

	if (resource == NULL) {
		weston_log("couldn't add workspace manager object");
		return;
	}

	wl_resource_set_implementation(resource,
				       &workspace_manager_implementation,
				       shell, unbind_resource);
	wl_list_insert(&shell->workspaces.client_list,
		       wl_resource_get_link(resource));

	workspace_manager_send_state(resource,
				     shell->workspaces.current,
				     shell->workspaces.num);
}

static void
touch_move_grab_down(struct weston_touch_grab *grab, uint32_t time,
		     int touch_id, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void
touch_move_grab_up(struct weston_touch_grab *grab, uint32_t time, int touch_id)
{
	struct weston_touch_move_grab *move =
		(struct weston_touch_move_grab *) container_of(
			grab, struct shell_touch_grab, grab);

	if (touch_id == 0)
		move->active = 0;

	if (grab->touch->num_tp == 0) {
		shell_touch_grab_end(&move->base);
		free(move);
	}
}

static void
touch_move_grab_motion(struct weston_touch_grab *grab, uint32_t time,
		       int touch_id, wl_fixed_t sx, wl_fixed_t sy)
{
	struct weston_touch_move_grab *move = (struct weston_touch_move_grab *) grab;
	struct shell_surface *shsurf = move->base.shsurf;
	struct weston_surface *es;
	int dx = wl_fixed_to_int(grab->touch->grab_x + move->dx);
	int dy = wl_fixed_to_int(grab->touch->grab_y + move->dy);

	if (!shsurf || !move->active)
		return;

	es = shsurf->surface;

	weston_view_set_position(shsurf->view, dx, dy);

	weston_compositor_schedule_repaint(es->compositor);
}

static void
touch_move_grab_cancel(struct weston_touch_grab *grab)
{
	struct weston_touch_move_grab *move =
		(struct weston_touch_move_grab *) container_of(
			grab, struct shell_touch_grab, grab);

	shell_touch_grab_end(&move->base);
	free(move);
}

static const struct weston_touch_grab_interface touch_move_grab_interface = {
	touch_move_grab_down,
	touch_move_grab_up,
	touch_move_grab_motion,
	touch_move_grab_cancel,
};

static int
surface_touch_move(struct shell_surface *shsurf, struct weston_seat *seat)
{
	struct weston_touch_move_grab *move;

	if (!shsurf)
		return -1;

	if (shsurf->state.fullscreen)
		return 0;
	if (shsurf->grabbed)
		return 0;

	move = malloc(sizeof *move);
	if (!move)
		return -1;

	move->active = 1;
	move->dx = wl_fixed_from_double(shsurf->view->geometry.x) -
			seat->touch->grab_x;
	move->dy = wl_fixed_from_double(shsurf->view->geometry.y) -
			seat->touch->grab_y;

	shell_touch_grab_start(&move->base, &touch_move_grab_interface, shsurf,
			       seat->touch);

	return 0;
}

static void
noop_grab_focus(struct weston_pointer_grab *grab)
{
}

static void
move_grab_motion(struct weston_pointer_grab *grab, uint32_t time,
		 wl_fixed_t x, wl_fixed_t y)
{
	struct weston_move_grab *move = (struct weston_move_grab *) grab;
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = move->base.shsurf;
	int dx, dy;

	weston_pointer_move(pointer, x, y);
	dx = wl_fixed_to_int(pointer->x + move->dx);
	dy = wl_fixed_to_int(pointer->y + move->dy);

	if (!shsurf)
		return;

	weston_view_set_position(shsurf->view, dx, dy);

	weston_compositor_schedule_repaint(shsurf->surface->compositor);
}

static void
move_grab_button(struct weston_pointer_grab *grab,
		 uint32_t time, uint32_t button, uint32_t state_w)
{
	struct shell_grab *shell_grab = container_of(grab, struct shell_grab,
						    grab);
	struct weston_pointer *pointer = grab->pointer;
	enum wl_pointer_button_state state = state_w;

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		shell_grab_end(shell_grab);
		free(grab);
	}
}

static void
move_grab_cancel(struct weston_pointer_grab *grab)
{
	struct shell_grab *shell_grab =
		container_of(grab, struct shell_grab, grab);

	shell_grab_end(shell_grab);
	free(grab);
}

static const struct weston_pointer_grab_interface move_grab_interface = {
	noop_grab_focus,
	move_grab_motion,
	move_grab_button,
	move_grab_cancel,
};

static int
surface_move(struct shell_surface *shsurf, struct weston_seat *seat)
{
	struct weston_move_grab *move;

	if (!shsurf)
		return -1;

	if (shsurf->grabbed)
		return 0;
	if (shsurf->state.fullscreen || shsurf->state.maximized)
		return 0;

	move = malloc(sizeof *move);
	if (!move)
		return -1;

	move->dx = wl_fixed_from_double(shsurf->view->geometry.x) -
			seat->pointer->grab_x;
	move->dy = wl_fixed_from_double(shsurf->view->geometry.y) -
			seat->pointer->grab_y;

	shell_grab_start(&move->base, &move_grab_interface, shsurf,
			 seat->pointer, DESKTOP_SHELL_CURSOR_MOVE);

	return 0;
}

static void
common_surface_move(struct wl_resource *resource,
		    struct wl_resource *seat_resource, uint32_t serial)
{
	struct weston_seat *seat = wl_resource_get_user_data(seat_resource);
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_surface *surface;

	if (seat->pointer &&
	    seat->pointer->focus &&
	    seat->pointer->button_count > 0 &&
	    seat->pointer->grab_serial == serial) {
		surface = weston_surface_get_main_surface(seat->pointer->focus->surface);
		if ((surface == shsurf->surface) &&
		    (surface_move(shsurf, seat) < 0))
			wl_resource_post_no_memory(resource);
	} else if (seat->touch &&
		   seat->touch->focus &&
		   seat->touch->grab_serial == serial) {
		surface = weston_surface_get_main_surface(seat->touch->focus->surface);
		if ((surface == shsurf->surface) &&
		    (surface_touch_move(shsurf, seat) < 0))
			wl_resource_post_no_memory(resource);
	}
}

static void
shell_surface_move(struct wl_client *client, struct wl_resource *resource,
		   struct wl_resource *seat_resource, uint32_t serial)
{
	common_surface_move(resource, seat_resource, serial);
}

struct weston_resize_grab {
	struct shell_grab base;
	uint32_t edges;
	int32_t width, height;
};

static void
resize_grab_motion(struct weston_pointer_grab *grab, uint32_t time,
		   wl_fixed_t x, wl_fixed_t y)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = resize->base.shsurf;
	int32_t width, height;
	wl_fixed_t from_x, from_y;
	wl_fixed_t to_x, to_y;

	weston_pointer_move(pointer, x, y);

	if (!shsurf)
		return;

	weston_view_from_global_fixed(shsurf->view,
				      pointer->grab_x, pointer->grab_y,
				      &from_x, &from_y);
	weston_view_from_global_fixed(shsurf->view,
				      pointer->x, pointer->y, &to_x, &to_y);

	width = resize->width;
	if (resize->edges & WL_SHELL_SURFACE_RESIZE_LEFT) {
		width += wl_fixed_to_int(from_x - to_x);
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_RIGHT) {
		width += wl_fixed_to_int(to_x - from_x);
	}

	height = resize->height;
	if (resize->edges & WL_SHELL_SURFACE_RESIZE_TOP) {
		height += wl_fixed_to_int(from_y - to_y);
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_BOTTOM) {
		height += wl_fixed_to_int(to_y - from_y);
	}

	shsurf->client->send_configure(shsurf->surface,
				       resize->edges, width, height);
}

static void
send_configure(struct weston_surface *surface,
	       uint32_t edges, int32_t width, int32_t height)
{
	struct shell_surface *shsurf = get_shell_surface(surface);

	assert(shsurf);

	wl_shell_surface_send_configure(shsurf->resource,
					edges, width, height);
}

static const struct weston_shell_client shell_client = {
	send_configure
};

static void
resize_grab_button(struct weston_pointer_grab *grab,
		   uint32_t time, uint32_t button, uint32_t state_w)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;
	struct weston_pointer *pointer = grab->pointer;
	enum wl_pointer_button_state state = state_w;

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		shell_grab_end(&resize->base);
		free(grab);
	}
}

static void
resize_grab_cancel(struct weston_pointer_grab *grab)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;

	shell_grab_end(&resize->base);
	free(grab);
}

static const struct weston_pointer_grab_interface resize_grab_interface = {
	noop_grab_focus,
	resize_grab_motion,
	resize_grab_button,
	resize_grab_cancel,
};

/*
 * Returns the bounding box of a surface and all its sub-surfaces,
 * in the surface coordinates system. */
static void
surface_subsurfaces_boundingbox(struct weston_surface *surface, int32_t *x,
				int32_t *y, int32_t *w, int32_t *h) {
	pixman_region32_t region;
	pixman_box32_t *box;
	struct weston_subsurface *subsurface;

	pixman_region32_init_rect(&region, 0, 0,
	                          surface->width,
	                          surface->height);

	wl_list_for_each(subsurface, &surface->subsurface_list, parent_link) {
		pixman_region32_union_rect(&region, &region,
		                           subsurface->position.x,
		                           subsurface->position.y,
		                           subsurface->surface->width,
		                           subsurface->surface->height);
	}

	box = pixman_region32_extents(&region);
	if (x)
		*x = box->x1;
	if (y)
		*y = box->y1;
	if (w)
		*w = box->x2 - box->x1;
	if (h)
		*h = box->y2 - box->y1;

	pixman_region32_fini(&region);
}

static int
surface_resize(struct shell_surface *shsurf,
	       struct weston_seat *seat, uint32_t edges)
{
	struct weston_resize_grab *resize;

	if (shsurf->state.fullscreen || shsurf->state.maximized)
		return 0;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return 0;

	resize = malloc(sizeof *resize);
	if (!resize)
		return -1;

	resize->edges = edges;
	surface_subsurfaces_boundingbox(shsurf->surface, NULL, NULL,
	                                &resize->width, &resize->height);

	shsurf->resize_edges = edges;
	shell_grab_start(&resize->base, &resize_grab_interface, shsurf,
			 seat->pointer, edges);

	return 0;
}

static void
common_surface_resize(struct wl_resource *resource,
		      struct wl_resource *seat_resource, uint32_t serial,
		      uint32_t edges)
{
	struct weston_seat *seat = wl_resource_get_user_data(seat_resource);
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_surface *surface;

	if (shsurf->state.fullscreen)
		return;

	if (seat->pointer->button_count == 0 ||
	    seat->pointer->grab_serial != serial ||
	    seat->pointer->focus == NULL)
		return;

	surface = weston_surface_get_main_surface(seat->pointer->focus->surface);
	if (surface != shsurf->surface)
		return;

	if (surface_resize(shsurf, seat, edges) < 0)
		wl_resource_post_no_memory(resource);
}

static void
shell_surface_resize(struct wl_client *client, struct wl_resource *resource,
		     struct wl_resource *seat_resource, uint32_t serial,
		     uint32_t edges)
{
	common_surface_resize(resource, seat_resource, serial, edges);
}

static void
busy_cursor_grab_focus(struct weston_pointer_grab *base)
{
	struct shell_grab *grab = (struct shell_grab *) base;
	struct weston_pointer *pointer = base->pointer;
	struct weston_view *view;
	wl_fixed_t sx, sy;

	view = weston_compositor_pick_view(pointer->seat->compositor,
					   pointer->x, pointer->y,
					   &sx, &sy);

	if (!grab->shsurf || grab->shsurf->surface != view->surface) {
		shell_grab_end(grab);
		free(grab);
	}
}

static void
busy_cursor_grab_motion(struct weston_pointer_grab *grab, uint32_t time,
			wl_fixed_t x, wl_fixed_t y)
{
	weston_pointer_move(grab->pointer, x, y);
}

static void
busy_cursor_grab_button(struct weston_pointer_grab *base,
			uint32_t time, uint32_t button, uint32_t state)
{
	struct shell_grab *grab = (struct shell_grab *) base;
	struct shell_surface *shsurf = grab->shsurf;
	struct weston_seat *seat = grab->grab.pointer->seat;

	if (shsurf && button == BTN_LEFT && state) {
		activate(shsurf->shell, shsurf->surface, seat);
		surface_move(shsurf, seat);
	} else if (shsurf && button == BTN_RIGHT && state) {
		activate(shsurf->shell, shsurf->surface, seat);
		surface_rotate(shsurf, seat);
	}
}

static void
busy_cursor_grab_cancel(struct weston_pointer_grab *base)
{
	struct shell_grab *grab = (struct shell_grab *) base;

	shell_grab_end(grab);
	free(grab);
}

static const struct weston_pointer_grab_interface busy_cursor_grab_interface = {
	busy_cursor_grab_focus,
	busy_cursor_grab_motion,
	busy_cursor_grab_button,
	busy_cursor_grab_cancel,
};

static void
set_busy_cursor(struct shell_surface *shsurf, struct weston_pointer *pointer)
{
	struct shell_grab *grab;

	if (pointer->grab->interface == &busy_cursor_grab_interface)
		return;

	grab = malloc(sizeof *grab);
	if (!grab)
		return;

	shell_grab_start(grab, &busy_cursor_grab_interface, shsurf, pointer,
			 DESKTOP_SHELL_CURSOR_BUSY);
	/* Mark the shsurf as ungrabbed so that button binding is able
	 * to move it. */
	shsurf->grabbed = 0;
}

static void
end_busy_cursor(struct weston_compositor *compositor, struct wl_client *client)
{
	struct shell_grab *grab;
	struct weston_seat *seat;

	wl_list_for_each(seat, &compositor->seat_list, link) {
		if (seat->pointer == NULL)
			continue;

		grab = (struct shell_grab *) seat->pointer->grab;
		if (grab->grab.interface == &busy_cursor_grab_interface &&
		    wl_resource_get_client(grab->shsurf->resource) == client) {
			shell_grab_end(grab);
			free(grab);
		}
	}
}

static void
handle_shell_client_destroy(struct wl_listener *listener, void *data);

static int
xdg_ping_timeout_handler(void *data)
{
	struct shell_client *sc = data;
	struct weston_seat *seat;
	struct shell_surface *shsurf;

	/* Client is not responding */
	sc->unresponsive = 1;
	wl_list_for_each(seat, &sc->shell->compositor->seat_list, link) {
		if (seat->pointer == NULL || seat->pointer->focus == NULL)
			continue;
		if (seat->pointer->focus->surface->resource == NULL)
			continue;
		
		shsurf = get_shell_surface(seat->pointer->focus->surface);
		if (shsurf &&
		    wl_resource_get_client(shsurf->resource) == sc->client)
			set_busy_cursor(shsurf, seat->pointer);
	}

	return 1;
}

static void
handle_xdg_ping(struct shell_surface *shsurf, uint32_t serial)
{
	struct weston_compositor *compositor = shsurf->shell->compositor;
	struct wl_client *client = wl_resource_get_client(shsurf->resource);
	struct shell_client *sc;
	struct wl_event_loop *loop;
	static const int ping_timeout = 200;

	sc = get_shell_client(client);
	if (sc->unresponsive) {
		xdg_ping_timeout_handler(sc);
		return;
	}

	sc->ping_serial = serial;
	loop = wl_display_get_event_loop(compositor->wl_display);
	if (sc->ping_timer == NULL)
		sc->ping_timer =
			wl_event_loop_add_timer(loop,
						xdg_ping_timeout_handler, sc);
	if (sc->ping_timer == NULL)
		return;

	wl_event_source_timer_update(sc->ping_timer, ping_timeout);

	if (shell_surface_is_xdg_surface(shsurf) ||
	    shell_surface_is_xdg_popup(shsurf))
		xdg_shell_send_ping(sc->resource, serial);
	else if (shell_surface_is_wl_shell_surface(shsurf))
		wl_shell_surface_send_ping(shsurf->resource, serial);
}

static void
ping_handler(struct weston_surface *surface, uint32_t serial)
{
	struct shell_surface *shsurf = get_shell_surface(surface);

	if (!shsurf)
		return;
	if (!shsurf->resource)
		return;
	if (shsurf->surface == shsurf->shell->grab_surface)
		return;

	handle_xdg_ping(shsurf, serial);
}

static void
handle_pointer_focus(struct wl_listener *listener, void *data)
{
	struct weston_pointer *pointer = data;
	struct weston_view *view = pointer->focus;
	struct weston_compositor *compositor;
	uint32_t serial;

	if (!view)
		return;

	compositor = view->surface->compositor;
	serial = wl_display_next_serial(compositor->wl_display);
	ping_handler(view->surface, serial);
}

static void
create_pointer_focus_listener(struct weston_seat *seat)
{
	struct wl_listener *listener;

	if (!seat->pointer)
		return;

	listener = malloc(sizeof *listener);
	listener->notify = handle_pointer_focus;
	wl_signal_add(&seat->pointer->focus_signal, listener);
}

static void
shell_surface_lose_keyboard_focus(struct shell_surface *shsurf)
{
	if (--shsurf->focus_count == 0)
		if (shell_surface_is_xdg_surface(shsurf))
			xdg_surface_send_deactivated(shsurf->resource);
}

static void
shell_surface_gain_keyboard_focus(struct shell_surface *shsurf)
{
	if (shsurf->focus_count++ == 0)
		if (shell_surface_is_xdg_surface(shsurf))
			xdg_surface_send_activated(shsurf->resource);
}

static void
handle_keyboard_focus(struct wl_listener *listener, void *data)
{
	struct weston_keyboard *keyboard = data;
	struct shell_seat *seat = get_shell_seat(keyboard->seat);

	if (seat->focused_surface) {
		struct shell_surface *shsurf = get_shell_surface(seat->focused_surface);
		if (shsurf)
			shell_surface_lose_keyboard_focus(shsurf);
	}

	seat->focused_surface = keyboard->focus;

	if (seat->focused_surface) {
		struct shell_surface *shsurf = get_shell_surface(seat->focused_surface);
		if (shsurf)
			shell_surface_gain_keyboard_focus(shsurf);
	}
}

static void
create_keyboard_focus_listener(struct weston_seat *seat)
{
	struct wl_listener *listener;

	if (!seat->keyboard)
		return;

	listener = malloc(sizeof *listener);
	listener->notify = handle_keyboard_focus;
	wl_signal_add(&seat->keyboard->focus_signal, listener);
}

static struct shell_client *
get_shell_client(struct wl_client *client)
{
	struct wl_listener *listener;

	listener = wl_client_get_destroy_listener(client,
						  handle_shell_client_destroy);
	if (listener == NULL)
		return NULL;

	return container_of(listener, struct shell_client, destroy_listener);
}

static void
shell_client_pong(struct shell_client *sc, uint32_t serial)
{
	if (sc->ping_serial != serial)
		return;

	sc->unresponsive = 0;
	end_busy_cursor(sc->shell->compositor, sc->client);

	if (sc->ping_timer) {
		wl_event_source_remove(sc->ping_timer);
		sc->ping_timer = NULL;
	}

}

static void
shell_surface_pong(struct wl_client *client,
		   struct wl_resource *resource, uint32_t serial)
{
	struct shell_client *sc;

	sc = get_shell_client(client);
	shell_client_pong(sc, serial);
}

static void
set_title(struct shell_surface *shsurf, const char *title)
{
	free(shsurf->title);
	shsurf->title = strdup(title);
}

static void
shell_surface_set_title(struct wl_client *client,
			struct wl_resource *resource, const char *title)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	set_title(shsurf, title);
}

static void
shell_surface_set_class(struct wl_client *client,
			struct wl_resource *resource, const char *class)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	free(shsurf->class);
	shsurf->class = strdup(class);
}

static void
restore_output_mode(struct weston_output *output)
{
	if (output->current_mode != output->original_mode ||
	    (int32_t)output->current_scale != output->original_scale)
		weston_output_switch_mode(output,
					  output->original_mode,
					  output->original_scale,
					  WESTON_MODE_SWITCH_RESTORE_NATIVE);
}

static void
restore_all_output_modes(struct weston_compositor *compositor)
{
	struct weston_output *output;

	wl_list_for_each(output, &compositor->output_list, link)
		restore_output_mode(output);
}

static int
get_output_panel_height(struct desktop_shell *shell,
			struct weston_output *output)
{
	struct weston_view *view;
	int panel_height = 0;

	if (!output)
		return 0;

	wl_list_for_each(view, &shell->panel_layer.view_list, layer_link) {
		if (view->surface->output == output) {
			panel_height = view->surface->height;
			break;
		}
	}

	return panel_height;
}

/* The surface will be inserted into the list immediately after the link
 * returned by this function (i.e. will be stacked immediately above the
 * returned link). */
static struct wl_list *
shell_surface_calculate_layer_link (struct shell_surface *shsurf)
{
	struct workspace *ws;
	struct weston_view *parent;

	switch (shsurf->type) {
	case SHELL_SURFACE_POPUP:
	case SHELL_SURFACE_TOPLEVEL:
		if (shsurf->state.fullscreen) {
			return &shsurf->shell->fullscreen_layer.view_list;
		} else if (shsurf->parent) {
			/* Move the surface to its parent layer so
			 * that surfaces which are transient for
			 * fullscreen surfaces don't get hidden by the
			 * fullscreen surfaces. */

			/* TODO: Handle a parent with multiple views */
			parent = get_default_view(shsurf->parent);
			if (parent)
				return parent->layer_link.prev;
		}
		break;

	case SHELL_SURFACE_XWAYLAND:
		return &shsurf->shell->fullscreen_layer.view_list;

	case SHELL_SURFACE_NONE:
	default:
		/* Go to the fallback, below. */
		break;
	}

	/* Move the surface to a normal workspace layer so that surfaces
	 * which were previously fullscreen or transient are no longer
	 * rendered on top. */
	ws = get_current_workspace(shsurf->shell);
	return &ws->layer.view_list;
}

static void
shell_surface_update_child_surface_layers (struct shell_surface *shsurf)
{
	struct shell_surface *child;

	/* Move the child layers to the same workspace as shsurf. They will be
	 * stacked above shsurf. */
	wl_list_for_each_reverse(child, &shsurf->children_list, children_link) {
		if (shsurf->view->layer_link.prev != &child->view->layer_link) {
			weston_view_geometry_dirty(child->view);
			wl_list_remove(&child->view->layer_link);
			wl_list_insert(shsurf->view->layer_link.prev,
			               &child->view->layer_link);
			weston_view_geometry_dirty(child->view);
			weston_surface_damage(child->surface);

			/* Recurse. We don’t expect this to recurse very far (if
			 * at all) because that would imply we have transient
			 * (or popup) children of transient surfaces, which
			 * would be unusual. */
			shell_surface_update_child_surface_layers(child);
		}
	}
}

/* Update the surface’s layer. Mark both the old and new views as having dirty
 * geometry to ensure the changes are redrawn.
 *
 * If any child surfaces exist and are mapped, ensure they’re in the same layer
 * as this surface. */
static void
shell_surface_update_layer(struct shell_surface *shsurf)
{
	struct wl_list *new_layer_link;

	new_layer_link = shell_surface_calculate_layer_link(shsurf);

	if (new_layer_link == &shsurf->view->layer_link)
		return;

	weston_view_geometry_dirty(shsurf->view);
	wl_list_remove(&shsurf->view->layer_link);
	wl_list_insert(new_layer_link, &shsurf->view->layer_link);
	weston_view_geometry_dirty(shsurf->view);
	weston_surface_damage(shsurf->surface);

	shell_surface_update_child_surface_layers(shsurf);
}

static void
shell_surface_set_parent(struct shell_surface *shsurf,
                         struct weston_surface *parent)
{
	shsurf->parent = parent;

	wl_list_remove(&shsurf->children_link);
	wl_list_init(&shsurf->children_link);

	/* Insert into the parent surface’s child list. */
	if (parent != NULL) {
		struct shell_surface *parent_shsurf = get_shell_surface(parent);
		if (parent_shsurf != NULL)
			wl_list_insert(&parent_shsurf->children_list,
			               &shsurf->children_link);
	}
}

static void
shell_surface_set_output(struct shell_surface *shsurf,
                         struct weston_output *output)
{
	struct weston_surface *es = shsurf->surface;

	/* get the default output, if the client set it as NULL
	   check whether the ouput is available */
	if (output)
		shsurf->output = output;
	else if (es->output)
		shsurf->output = es->output;
	else
		shsurf->output = get_default_output(es->compositor);
}

static void
surface_clear_next_states(struct shell_surface *shsurf)
{
	shsurf->next_state.maximized = false;
	shsurf->next_state.fullscreen = false;

	if ((shsurf->next_state.maximized != shsurf->state.maximized) ||
	    (shsurf->next_state.fullscreen != shsurf->state.fullscreen))
		shsurf->state_changed = true;
}

static void
set_toplevel(struct shell_surface *shsurf)
{
	shell_surface_set_parent(shsurf, NULL);
	surface_clear_next_states(shsurf);
	shsurf->type = SHELL_SURFACE_TOPLEVEL;

	/* The layer_link is updated in set_surface_type(),
	 * called from configure. */
}

static void
shell_surface_set_toplevel(struct wl_client *client,
			   struct wl_resource *resource)
{
	struct shell_surface *surface = wl_resource_get_user_data(resource);

	set_toplevel(surface);
}

static void
set_transient(struct shell_surface *shsurf,
	      struct weston_surface *parent, int x, int y, uint32_t flags)
{
	assert(parent != NULL);

	shell_surface_set_parent(shsurf, parent);

	surface_clear_next_states(shsurf);

	shsurf->transient.x = x;
	shsurf->transient.y = y;
	shsurf->transient.flags = flags;

	shsurf->next_state.relative = true;
	shsurf->state_changed = true;
	shsurf->type = SHELL_SURFACE_TOPLEVEL;

	/* The layer_link is updated in set_surface_type(),
	 * called from configure. */
}

static void
shell_surface_set_transient(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *parent_resource,
			    int x, int y, uint32_t flags)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_surface *parent =
		wl_resource_get_user_data(parent_resource);

	set_transient(shsurf, parent, x, y, flags);
}

static void
set_fullscreen(struct shell_surface *shsurf,
	       uint32_t method,
	       uint32_t framerate,
	       struct weston_output *output)
{
	shell_surface_set_output(shsurf, output);

	shsurf->fullscreen_output = shsurf->output;
	shsurf->fullscreen.type = method;
	shsurf->fullscreen.framerate = framerate;

	shsurf->type = SHELL_SURFACE_TOPLEVEL;

	shsurf->client->send_configure(shsurf->surface, 0,
				       shsurf->output->width,
				       shsurf->output->height);

	/* The layer_link is updated in set_surface_type(),
	 * called from configure. */
}

static void
weston_view_set_initial_position(struct weston_view *view,
				 struct desktop_shell *shell);

static void
unset_fullscreen(struct shell_surface *shsurf)
{
	/* Unset the fullscreen output, driver configuration and transforms. */
	if (shsurf->fullscreen.type == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER &&
	    shell_surface_is_top_fullscreen(shsurf)) {
		restore_output_mode(shsurf->fullscreen_output);
	}
	shsurf->fullscreen_output = NULL;

	shsurf->fullscreen.type = WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;
	shsurf->fullscreen.framerate = 0;

	wl_list_remove(&shsurf->fullscreen.transform.link);
	wl_list_init(&shsurf->fullscreen.transform.link);

	if (shsurf->fullscreen.black_view)
		weston_surface_destroy(shsurf->fullscreen.black_view->surface);
	shsurf->fullscreen.black_view = NULL;

	if (shsurf->saved_position_valid)
		weston_view_set_position(shsurf->view,
					 shsurf->saved_x, shsurf->saved_y);
	else
		weston_view_set_initial_position(shsurf->view, shsurf->shell);

	if (shsurf->saved_rotation_valid) {
		wl_list_insert(&shsurf->view->geometry.transformation_list,
		               &shsurf->rotation.transform.link);
		shsurf->saved_rotation_valid = false;
	}

	/* Layer is updated in set_surface_type(). */
}

static void
shell_surface_set_fullscreen(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t method,
			     uint32_t framerate,
			     struct wl_resource *output_resource)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_output *output;

	if (output_resource)
		output = wl_resource_get_user_data(output_resource);
	else
		output = NULL;

	shell_surface_set_parent(shsurf, NULL);

	surface_clear_next_states(shsurf);
	set_fullscreen(shsurf, method, framerate, output);

	shsurf->next_state.fullscreen = true;
	shsurf->state_changed = true;
}

static void
set_popup(struct shell_surface *shsurf,
          struct weston_surface *parent,
          struct weston_seat *seat,
          uint32_t serial,
          int32_t x,
          int32_t y)
{
	assert(parent != NULL);

	shsurf->popup.shseat = get_shell_seat(seat);
	shsurf->popup.serial = serial;
	shsurf->popup.x = x;
	shsurf->popup.y = y;

	shsurf->type = SHELL_SURFACE_POPUP;
}

static void
shell_surface_set_popup(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *seat_resource,
			uint32_t serial,
			struct wl_resource *parent_resource,
			int32_t x, int32_t y, uint32_t flags)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_surface *parent =
		wl_resource_get_user_data(parent_resource);

	shell_surface_set_parent(shsurf, parent);

	surface_clear_next_states(shsurf);
	set_popup(shsurf,
	          parent,
	          wl_resource_get_user_data(seat_resource),
	          serial, x, y);
}

static void
set_maximized(struct shell_surface *shsurf,
              struct weston_output *output)
{
	struct desktop_shell *shell;
	uint32_t edges = 0, panel_height = 0;

	shell_surface_set_output(shsurf, output);

	shell = shell_surface_get_shell(shsurf);
	panel_height = get_output_panel_height(shell, shsurf->output);
	edges = WL_SHELL_SURFACE_RESIZE_TOP | WL_SHELL_SURFACE_RESIZE_LEFT;

	shsurf->client->send_configure(shsurf->surface, edges,
	                               shsurf->output->width,
	                               shsurf->output->height - panel_height);

	shsurf->type = SHELL_SURFACE_TOPLEVEL;
}

static void
unset_maximized(struct shell_surface *shsurf)
{
	/* undo all maximized things here */
	shsurf->output = get_default_output(shsurf->surface->compositor);

	if (shsurf->saved_position_valid)
		weston_view_set_position(shsurf->view,
					 shsurf->saved_x, shsurf->saved_y);
	else
		weston_view_set_initial_position(shsurf->view, shsurf->shell);

	if (shsurf->saved_rotation_valid) {
		wl_list_insert(&shsurf->view->geometry.transformation_list,
			       &shsurf->rotation.transform.link);
		shsurf->saved_rotation_valid = false;
	}

	/* Layer is updated in set_surface_type(). */
}

static void
set_minimized(struct weston_surface *surface, uint32_t is_true)
{
	struct shell_surface *shsurf;
	struct workspace *current_ws;
	struct weston_seat *seat;
	struct weston_surface *focus;
	struct weston_view *view;

	view = get_default_view(surface);
	if (!view)
		return;

	assert(weston_surface_get_main_surface(view->surface) == view->surface);

	shsurf = get_shell_surface(surface);
	current_ws = get_current_workspace(shsurf->shell);

	wl_list_remove(&view->layer_link);
	 /* hide or show, depending on the state */
	if (is_true) {
		wl_list_insert(&shsurf->shell->minimized_layer.view_list, &view->layer_link);

		drop_focus_state(shsurf->shell, current_ws, view->surface);
		wl_list_for_each(seat, &shsurf->shell->compositor->seat_list, link) {
			if (!seat->keyboard)
				continue;
			focus = weston_surface_get_main_surface(seat->keyboard->focus);
			if (focus == view->surface)
				weston_keyboard_set_focus(seat->keyboard, NULL);
		}
	}
	else {
		wl_list_insert(&current_ws->layer.view_list, &view->layer_link);

		wl_list_for_each(seat, &shsurf->shell->compositor->seat_list, link) {
			if (!seat->keyboard)
				continue;
			activate(shsurf->shell, view->surface, seat);
		}
	}

	shell_surface_update_child_surface_layers(shsurf);

	weston_view_damage_below(view);
}

static void
shell_surface_set_maximized(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *output_resource)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_output *output;

	if (output_resource)
		output = wl_resource_get_user_data(output_resource);
	else
		output = NULL;

	shell_surface_set_parent(shsurf, NULL);

	surface_clear_next_states(shsurf);
	set_maximized(shsurf, output);

	shsurf->next_state.maximized = true;
	shsurf->state_changed = true;
}

/* This is only ever called from set_surface_type(), so there’s no need to
 * update layer_links here, since they’ll be updated when we return. */
static int
reset_surface_type(struct shell_surface *surface)
{
	if (surface->state.fullscreen)
		unset_fullscreen(surface);
	if (surface->state.maximized)
		unset_maximized(surface);

	return 0;
}

static void
set_full_output(struct shell_surface *shsurf)
{
	shsurf->saved_x = shsurf->view->geometry.x;
	shsurf->saved_y = shsurf->view->geometry.y;
	shsurf->saved_width = shsurf->surface->width;
	shsurf->saved_height = shsurf->surface->height;
	shsurf->saved_size_valid = true;
	shsurf->saved_position_valid = true;

	if (!wl_list_empty(&shsurf->rotation.transform.link)) {
		wl_list_remove(&shsurf->rotation.transform.link);
		wl_list_init(&shsurf->rotation.transform.link);
		weston_view_geometry_dirty(shsurf->view);
		shsurf->saved_rotation_valid = true;
	}
}

static void
set_surface_type(struct shell_surface *shsurf)
{
	struct weston_surface *pes = shsurf->parent;
	struct weston_view *pev = get_default_view(pes);

	reset_surface_type(shsurf);

	shsurf->state = shsurf->next_state;
	shsurf->state_changed = false;

	switch (shsurf->type) {
	case SHELL_SURFACE_TOPLEVEL:
		if (shsurf->state.maximized || shsurf->state.fullscreen) {
			set_full_output(shsurf);
		} else if (shsurf->state.relative && pev) {
			weston_view_set_position(shsurf->view,
						 pev->geometry.x + shsurf->transient.x,
						 pev->geometry.y + shsurf->transient.y);
		}
		break;

	case SHELL_SURFACE_XWAYLAND:
		weston_view_set_position(shsurf->view, shsurf->transient.x,
					 shsurf->transient.y);
		break;

	case SHELL_SURFACE_POPUP:
	case SHELL_SURFACE_NONE:
	default:
		break;
	}

	/* Update the surface’s layer. */
	shell_surface_update_layer(shsurf);
}

static struct desktop_shell *
shell_surface_get_shell(struct shell_surface *shsurf)
{
	return shsurf->shell;
}

static void
black_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy);

static struct weston_view *
create_black_surface(struct weston_compositor *ec,
		     struct weston_surface *fs_surface,
		     float x, float y, int w, int h)
{
	struct weston_surface *surface = NULL;
	struct weston_view *view;

	surface = weston_surface_create(ec);
	if (surface == NULL) {
		weston_log("no memory\n");
		return NULL;
	}
	view = weston_view_create(surface);
	if (surface == NULL) {
		weston_log("no memory\n");
		weston_surface_destroy(surface);
		return NULL;
	}

	surface->configure = black_surface_configure;
	surface->configure_private = fs_surface;
	weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1);
	pixman_region32_fini(&surface->opaque);
	pixman_region32_init_rect(&surface->opaque, 0, 0, w, h);
	pixman_region32_fini(&surface->input);
	pixman_region32_init_rect(&surface->input, 0, 0, w, h);

	weston_surface_set_size(surface, w, h);
	weston_view_set_position(view, x, y);

	return view;
}

static void
shell_ensure_fullscreen_black_view(struct shell_surface *shsurf)
{
	struct weston_output *output = shsurf->fullscreen_output;

	assert(shsurf->state.fullscreen);

	if (!shsurf->fullscreen.black_view)
		shsurf->fullscreen.black_view =
			create_black_surface(shsurf->surface->compositor,
			                     shsurf->surface,
			                     output->x, output->y,
			                     output->width,
			                     output->height);

	weston_view_geometry_dirty(shsurf->fullscreen.black_view);
	wl_list_remove(&shsurf->fullscreen.black_view->layer_link);
	wl_list_insert(&shsurf->view->layer_link,
	               &shsurf->fullscreen.black_view->layer_link);
	weston_view_geometry_dirty(shsurf->fullscreen.black_view);
	weston_surface_damage(shsurf->surface);
}

/* Create black surface and append it to the associated fullscreen surface.
 * Handle size dismatch and positioning according to the method. */
static void
shell_configure_fullscreen(struct shell_surface *shsurf)
{
	struct weston_output *output = shsurf->fullscreen_output;
	struct weston_surface *surface = shsurf->surface;
	struct weston_matrix *matrix;
	float scale, output_aspect, surface_aspect, x, y;
	int32_t surf_x, surf_y, surf_width, surf_height;

	if (shsurf->fullscreen.type != WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER)
		restore_output_mode(output);

	shell_ensure_fullscreen_black_view(shsurf);

	surface_subsurfaces_boundingbox(shsurf->surface, &surf_x, &surf_y,
	                                &surf_width, &surf_height);

	switch (shsurf->fullscreen.type) {
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT:
		if (surface->buffer_ref.buffer)
			center_on_output(shsurf->view, shsurf->fullscreen_output);
		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE:
		/* 1:1 mapping between surface and output dimensions */
		if (output->width == surf_width &&
			output->height == surf_height) {
			weston_view_set_position(shsurf->view,
						 output->x - surf_x,
						 output->y - surf_y);
			break;
		}

		matrix = &shsurf->fullscreen.transform.matrix;
		weston_matrix_init(matrix);

		output_aspect = (float) output->width /
			(float) output->height;
		/* XXX: Use surf_width and surf_height here? */
		surface_aspect = (float) surface->width /
			(float) surface->height;
		if (output_aspect < surface_aspect)
			scale = (float) output->width /
				(float) surf_width;
		else
			scale = (float) output->height /
				(float) surf_height;

		weston_matrix_scale(matrix, scale, scale, 1);
		wl_list_remove(&shsurf->fullscreen.transform.link);
		wl_list_insert(&shsurf->view->geometry.transformation_list,
			       &shsurf->fullscreen.transform.link);
		x = output->x + (output->width - surf_width * scale) / 2 - surf_x;
		y = output->y + (output->height - surf_height * scale) / 2 - surf_y;
		weston_view_set_position(shsurf->view, x, y);

		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER:
		if (shell_surface_is_top_fullscreen(shsurf)) {
			struct weston_mode mode = {0,
				surf_width * surface->buffer_viewport.buffer.scale,
				surf_height * surface->buffer_viewport.buffer.scale,
				shsurf->fullscreen.framerate};

			if (weston_output_switch_mode(output, &mode, surface->buffer_viewport.buffer.scale,
					WESTON_MODE_SWITCH_SET_TEMPORARY) == 0) {
				weston_view_set_position(shsurf->view,
							 output->x - surf_x,
							 output->y - surf_y);
				shsurf->fullscreen.black_view->surface->width = output->width;
				shsurf->fullscreen.black_view->surface->height = output->height;
				weston_view_set_position(shsurf->fullscreen.black_view,
							 output->x - surf_x,
							 output->y - surf_y);
				break;
			} else {
				restore_output_mode(output);
				center_on_output(shsurf->view, output);
			}
		}
		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_FILL:
		center_on_output(shsurf->view, output);
		break;
	default:
		break;
	}
}

static void
shell_map_fullscreen(struct shell_surface *shsurf)
{
	shell_configure_fullscreen(shsurf);
}

static void
set_xwayland(struct shell_surface *shsurf, int x, int y, uint32_t flags)
{
	/* XXX: using the same fields for transient type */
	surface_clear_next_states(shsurf);
	shsurf->transient.x = x;
	shsurf->transient.y = y;
	shsurf->transient.flags = flags;

	shell_surface_set_parent(shsurf, NULL);

	shsurf->type = SHELL_SURFACE_XWAYLAND;
	shsurf->state_changed = true;
}

static const struct weston_pointer_grab_interface popup_grab_interface;

static void
destroy_shell_seat(struct wl_listener *listener, void *data)
{
	struct shell_seat *shseat =
		container_of(listener,
			     struct shell_seat, seat_destroy_listener);
	struct shell_surface *shsurf, *prev = NULL;

	if (shseat->popup_grab.grab.interface == &popup_grab_interface) {
		weston_pointer_end_grab(shseat->popup_grab.grab.pointer);
		shseat->popup_grab.client = NULL;

		wl_list_for_each(shsurf, &shseat->popup_grab.surfaces_list, popup.grab_link) {
			shsurf->popup.shseat = NULL;
			if (prev) {
				wl_list_init(&prev->popup.grab_link);
			}
			prev = shsurf;
		}
		wl_list_init(&prev->popup.grab_link);
	}

	wl_list_remove(&shseat->seat_destroy_listener.link);
	free(shseat);
}

static struct shell_seat *
create_shell_seat(struct weston_seat *seat)
{
	struct shell_seat *shseat;

	shseat = calloc(1, sizeof *shseat);
	if (!shseat) {
		weston_log("no memory to allocate shell seat\n");
		return NULL;
	}

	shseat->seat = seat;
	wl_list_init(&shseat->popup_grab.surfaces_list);

	shseat->seat_destroy_listener.notify = destroy_shell_seat;
	wl_signal_add(&seat->destroy_signal,
	              &shseat->seat_destroy_listener);

	return shseat;
}

static struct shell_seat *
get_shell_seat(struct weston_seat *seat)
{
	struct wl_listener *listener;

	listener = wl_signal_get(&seat->destroy_signal, destroy_shell_seat);
	if (listener == NULL)
		return create_shell_seat(seat);

	return container_of(listener,
			    struct shell_seat, seat_destroy_listener);
}

static void
popup_grab_focus(struct weston_pointer_grab *grab)
{
	struct weston_pointer *pointer = grab->pointer;
	struct weston_view *view;
	struct shell_seat *shseat =
	    container_of(grab, struct shell_seat, popup_grab.grab);
	struct wl_client *client = shseat->popup_grab.client;
	wl_fixed_t sx, sy;

	view = weston_compositor_pick_view(pointer->seat->compositor,
					   pointer->x, pointer->y,
					   &sx, &sy);

	if (view && view->surface->resource &&
	    wl_resource_get_client(view->surface->resource) == client) {
		weston_pointer_set_focus(pointer, view, sx, sy);
	} else {
		weston_pointer_set_focus(pointer, NULL,
					 wl_fixed_from_int(0),
					 wl_fixed_from_int(0));
	}
}

static void
popup_grab_motion(struct weston_pointer_grab *grab, uint32_t time,
		  wl_fixed_t x, wl_fixed_t y)
{
	struct weston_pointer *pointer = grab->pointer;
	struct wl_resource *resource;
	wl_fixed_t sx, sy;

	weston_pointer_move(pointer, x, y);

	wl_resource_for_each(resource, &pointer->focus_resource_list) {
		weston_view_from_global_fixed(pointer->focus,
					      pointer->x, pointer->y,
					      &sx, &sy);
		wl_pointer_send_motion(resource, time, sx, sy);
	}
}

static void
popup_grab_button(struct weston_pointer_grab *grab,
		  uint32_t time, uint32_t button, uint32_t state_w)
{
	struct wl_resource *resource;
	struct shell_seat *shseat =
	    container_of(grab, struct shell_seat, popup_grab.grab);
	struct wl_display *display = shseat->seat->compositor->wl_display;
	enum wl_pointer_button_state state = state_w;
	uint32_t serial;
	struct wl_list *resource_list;

	resource_list = &grab->pointer->focus_resource_list;
	if (!wl_list_empty(resource_list)) {
		serial = wl_display_get_serial(display);
		wl_resource_for_each(resource, resource_list) {
			wl_pointer_send_button(resource, serial,
					       time, button, state);
		}
	} else if (state == WL_POINTER_BUTTON_STATE_RELEASED &&
		   (shseat->popup_grab.initial_up ||
		    time - shseat->seat->pointer->grab_time > 500)) {
		popup_grab_end(grab->pointer);
	}

	if (state == WL_POINTER_BUTTON_STATE_RELEASED)
		shseat->popup_grab.initial_up = 1;
}

static void
popup_grab_cancel(struct weston_pointer_grab *grab)
{
	popup_grab_end(grab->pointer);
}

static const struct weston_pointer_grab_interface popup_grab_interface = {
	popup_grab_focus,
	popup_grab_motion,
	popup_grab_button,
	popup_grab_cancel,
};

static void
shell_surface_send_popup_done(struct shell_surface *shsurf)
{
	if (shell_surface_is_wl_shell_surface(shsurf))
		wl_shell_surface_send_popup_done(shsurf->resource);
	else if (shell_surface_is_xdg_popup(shsurf))
		xdg_popup_send_popup_done(shsurf->resource,
					  shsurf->popup.serial);
}

static void
popup_grab_end(struct weston_pointer *pointer)
{
	struct weston_pointer_grab *grab = pointer->grab;
	struct shell_seat *shseat =
	    container_of(grab, struct shell_seat, popup_grab.grab);
	struct shell_surface *shsurf;
	struct shell_surface *prev = NULL;

	if (pointer->grab->interface == &popup_grab_interface) {
		weston_pointer_end_grab(grab->pointer);
		shseat->popup_grab.client = NULL;
		shseat->popup_grab.grab.interface = NULL;
		assert(!wl_list_empty(&shseat->popup_grab.surfaces_list));
		/* Send the popup_done event to all the popups open */
		wl_list_for_each(shsurf, &shseat->popup_grab.surfaces_list, popup.grab_link) {
			shell_surface_send_popup_done(shsurf);
			shsurf->popup.shseat = NULL;
			if (prev) {
				wl_list_init(&prev->popup.grab_link);
			}
			prev = shsurf;
		}
		wl_list_init(&prev->popup.grab_link);
		wl_list_init(&shseat->popup_grab.surfaces_list);
	}
}

static void
add_popup_grab(struct shell_surface *shsurf, struct shell_seat *shseat)
{
	struct weston_seat *seat = shseat->seat;

	if (wl_list_empty(&shseat->popup_grab.surfaces_list)) {
		shseat->popup_grab.client = wl_resource_get_client(shsurf->resource);
		shseat->popup_grab.grab.interface = &popup_grab_interface;
		/* We must make sure here that this popup was opened after
		 * a mouse press, and not just by moving around with other
		 * popups already open. */
		if (shseat->seat->pointer->button_count > 0)
			shseat->popup_grab.initial_up = 0;

		wl_list_insert(&shseat->popup_grab.surfaces_list, &shsurf->popup.grab_link);
		weston_pointer_start_grab(seat->pointer, &shseat->popup_grab.grab);
	} else {
		wl_list_insert(&shseat->popup_grab.surfaces_list, &shsurf->popup.grab_link);
	}
}

static void
remove_popup_grab(struct shell_surface *shsurf)
{
	struct shell_seat *shseat = shsurf->popup.shseat;

	wl_list_remove(&shsurf->popup.grab_link);
	wl_list_init(&shsurf->popup.grab_link);
	if (wl_list_empty(&shseat->popup_grab.surfaces_list)) {
		weston_pointer_end_grab(shseat->popup_grab.grab.pointer);
		shseat->popup_grab.grab.interface = NULL;
	}
}

static void
shell_map_popup(struct shell_surface *shsurf)
{
	struct shell_seat *shseat = shsurf->popup.shseat;
	struct weston_view *parent_view = get_default_view(shsurf->parent);

	shsurf->surface->output = parent_view->output;
	shsurf->view->output = parent_view->output;

	weston_view_set_transform_parent(shsurf->view, parent_view);
	weston_view_set_position(shsurf->view, shsurf->popup.x, shsurf->popup.y);
	weston_view_update_transform(shsurf->view);

	if (shseat->seat->pointer->grab_serial == shsurf->popup.serial) {
		add_popup_grab(shsurf, shseat);
	} else {
		shell_surface_send_popup_done(shsurf);
		shseat->popup_grab.client = NULL;
	}
}

static const struct wl_shell_surface_interface shell_surface_implementation = {
	shell_surface_pong,
	shell_surface_move,
	shell_surface_resize,
	shell_surface_set_toplevel,
	shell_surface_set_transient,
	shell_surface_set_fullscreen,
	shell_surface_set_popup,
	shell_surface_set_maximized,
	shell_surface_set_title,
	shell_surface_set_class
};

static void
destroy_shell_surface(struct shell_surface *shsurf)
{
	struct shell_surface *child, *next;

	wl_signal_emit(&shsurf->destroy_signal, shsurf);

	if (!wl_list_empty(&shsurf->popup.grab_link)) {
		remove_popup_grab(shsurf);
	}

	if (shsurf->fullscreen.type == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER &&
	    shell_surface_is_top_fullscreen(shsurf))
		restore_output_mode (shsurf->fullscreen_output);

	if (shsurf->fullscreen.black_view)
		weston_surface_destroy(shsurf->fullscreen.black_view->surface);

	/* As destroy_resource() use wl_list_for_each_safe(),
	 * we can always remove the listener.
	 */
	wl_list_remove(&shsurf->surface_destroy_listener.link);
	shsurf->surface->configure = NULL;
	free(shsurf->title);

	weston_view_destroy(shsurf->view);

	wl_list_remove(&shsurf->children_link);
	wl_list_for_each_safe(child, next, &shsurf->children_list, children_link)
		shell_surface_set_parent(child, NULL);

	wl_list_remove(&shsurf->link);
	free(shsurf);
}

static void
shell_destroy_shell_surface(struct wl_resource *resource)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	if (!wl_list_empty(&shsurf->popup.grab_link))
		remove_popup_grab(shsurf);
	shsurf->resource = NULL;
}

static void
shell_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct shell_surface *shsurf = container_of(listener,
						    struct shell_surface,
						    surface_destroy_listener);

	if (shsurf->resource)
		wl_resource_destroy(shsurf->resource);
	else
		destroy_shell_surface(shsurf);
}

static void
fade_out_done(struct weston_view_animation *animation, void *data)
{
	struct shell_surface *shsurf = data;

	weston_surface_destroy(shsurf->surface);
}

static void
handle_resource_destroy(struct wl_listener *listener, void *data)
{
	struct shell_surface *shsurf =
		container_of(listener, struct shell_surface,
			     resource_destroy_listener);

	shsurf->surface->ref_count++;

	pixman_region32_fini(&shsurf->surface->pending.input);
	pixman_region32_init(&shsurf->surface->pending.input);
	pixman_region32_fini(&shsurf->surface->input);
	pixman_region32_init(&shsurf->surface->input);
	if (weston_surface_is_mapped(shsurf->surface))
		weston_fade_run(shsurf->view, 1.0, 0.0, 300.0,
				fade_out_done, shsurf);
}

static void
shell_surface_configure(struct weston_surface *, int32_t, int32_t);

struct shell_surface *
get_shell_surface(struct weston_surface *surface)
{
	if (surface->configure == shell_surface_configure)
		return surface->configure_private;
	else
		return NULL;
}

static struct shell_surface *
create_common_surface(void *shell, struct weston_surface *surface,
		      const struct weston_shell_client *client)
{
	struct shell_surface *shsurf;

	if (surface->configure) {
		weston_log("surface->configure already set\n");
		return NULL;
	}

	shsurf = calloc(1, sizeof *shsurf);
	if (!shsurf) {
		weston_log("no memory to allocate shell surface\n");
		return NULL;
	}

	shsurf->view = weston_view_create(surface);
	if (!shsurf->view) {
		weston_log("no memory to allocate shell surface\n");
		free(shsurf);
		return NULL;
	}

	surface->configure = shell_surface_configure;
	surface->configure_private = shsurf;

	shsurf->resource_destroy_listener.notify = handle_resource_destroy;
	wl_resource_add_destroy_listener(surface->resource,
					 &shsurf->resource_destroy_listener);

	shsurf->shell = (struct desktop_shell *) shell;
	shsurf->unresponsive = 0;
	shsurf->saved_position_valid = false;
	shsurf->saved_size_valid = false;
	shsurf->saved_rotation_valid = false;
	shsurf->surface = surface;
	shsurf->fullscreen.type = WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;
	shsurf->fullscreen.framerate = 0;
	shsurf->fullscreen.black_view = NULL;
	wl_list_init(&shsurf->fullscreen.transform.link);

	wl_signal_init(&shsurf->destroy_signal);
	shsurf->surface_destroy_listener.notify = shell_handle_surface_destroy;
	wl_signal_add(&surface->destroy_signal,
		      &shsurf->surface_destroy_listener);

	/* init link so its safe to always remove it in destroy_shell_surface */
	wl_list_init(&shsurf->link);
	wl_list_init(&shsurf->popup.grab_link);

	/* empty when not in use */
	wl_list_init(&shsurf->rotation.transform.link);
	weston_matrix_init(&shsurf->rotation.rotation);

	wl_list_init(&shsurf->workspace_transform.link);

	wl_list_init(&shsurf->children_link);
	wl_list_init(&shsurf->children_list);
	shsurf->parent = NULL;

	shsurf->type = SHELL_SURFACE_NONE;

	shsurf->client = client;

	return shsurf;
}

static struct shell_surface *
create_shell_surface(void *shell, struct weston_surface *surface,
		     const struct weston_shell_client *client)
{
	return create_common_surface(shell, surface, client);
}

static struct weston_view *
get_primary_view(void *shell, struct shell_surface *shsurf)
{
	return shsurf->view;
}

static void
shell_get_shell_surface(struct wl_client *client,
			struct wl_resource *resource,
			uint32_t id,
			struct wl_resource *surface_resource)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct shell_surface *shsurf;

	if (get_shell_surface(surface)) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "desktop_shell::get_shell_surface already requested");
		return;
	}

	shsurf = create_shell_surface(shell, surface, &shell_client);
	if (!shsurf) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->configure already set");
		return;
	}

	shsurf->resource =
		wl_resource_create(client,
				   &wl_shell_surface_interface, 1, id);
	wl_resource_set_implementation(shsurf->resource,
				       &shell_surface_implementation,
				       shsurf, shell_destroy_shell_surface);
}

static bool
shell_surface_is_wl_shell_surface(struct shell_surface *shsurf)
{
	/* A shell surface without a resource is created from xwayland
	 * and is considered a wl_shell surface for now. */

	return shsurf->resource == NULL ||
		wl_resource_instance_of(shsurf->resource,
					&wl_shell_surface_interface,
					&shell_surface_implementation);
}

static const struct wl_shell_interface shell_implementation = {
	shell_get_shell_surface
};

/****************************
 * xdg-shell implementation */

static void
xdg_surface_destroy(struct wl_client *client,
		    struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
xdg_surface_set_transient_for(struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *parent_resource)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_surface *parent;

	if (parent_resource)
		parent = wl_resource_get_user_data(parent_resource);
	else
		parent = NULL;

	shell_surface_set_parent(shsurf, parent);
}

static void
xdg_surface_set_margin(struct wl_client *client,
			     struct wl_resource *resource,
			     int32_t left,
			     int32_t right,
			     int32_t top,
			     int32_t bottom)
{
	/* Do nothing, Weston doesn't try to constrain or place
	 * surfaces in any special manner... */
}

static void
xdg_surface_set_app_id(struct wl_client *client,
		       struct wl_resource *resource,
		       const char *app_id)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	free(shsurf->class);
	shsurf->class = strdup(app_id);
}

static void
xdg_surface_set_title(struct wl_client *client,
			struct wl_resource *resource, const char *title)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	set_title(shsurf, title);
}

static void
xdg_surface_move(struct wl_client *client, struct wl_resource *resource,
		 struct wl_resource *seat_resource, uint32_t serial)
{
	common_surface_move(resource, seat_resource, serial);
}

static void
xdg_surface_resize(struct wl_client *client, struct wl_resource *resource,
		   struct wl_resource *seat_resource, uint32_t serial,
		   uint32_t edges)
{
	common_surface_resize(resource, seat_resource, serial, edges);
}

static void
xdg_surface_set_output(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *output_resource)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);
	struct weston_output *output;

	if (output_resource)
		output = wl_resource_get_user_data(output_resource);
	else
		output = NULL;

	shsurf->recommended_output = output;
}

static void
xdg_surface_change_state(struct shell_surface *shsurf,
			 uint32_t state, uint32_t value, uint32_t serial)
{
	xdg_surface_send_change_state(shsurf->resource, state, value, serial);
	shsurf->state_requested = true;

	switch (state) {
	case XDG_SURFACE_STATE_MAXIMIZED:
		shsurf->requested_state.maximized = value;
		if (value)
			set_maximized(shsurf, NULL);
		break;
	case XDG_SURFACE_STATE_FULLSCREEN:
		shsurf->requested_state.fullscreen = value;

		if (value)
			set_fullscreen(shsurf,
				       WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
				       0, shsurf->recommended_output);
		break;
	}
}

static void
xdg_surface_request_change_state(struct wl_client *client,
				 struct wl_resource *resource,
				 uint32_t state,
				 uint32_t value,
				 uint32_t serial)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	/* The client can't know what the current state is, so we need
	   to always send a state change in response. */

	if (shsurf->type != SHELL_SURFACE_TOPLEVEL)
		return;

	switch (state) {
	case XDG_SURFACE_STATE_MAXIMIZED:
	case XDG_SURFACE_STATE_FULLSCREEN:
		break;
	default:
		/* send error? ignore? send change state with value 0? */
		return;
	}

	xdg_surface_change_state(shsurf, state, value, serial);
}

static void
xdg_surface_ack_change_state(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t state,
			     uint32_t value,
			     uint32_t serial)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	if (shsurf->state_requested) {
		shsurf->next_state = shsurf->requested_state;
		shsurf->state_changed = true;
		shsurf->state_requested = false;
	}
}

static void
xdg_surface_set_minimized(struct wl_client *client,
			    struct wl_resource *resource)
{
	struct shell_surface *shsurf = wl_resource_get_user_data(resource);

	if (shsurf->type != SHELL_SURFACE_TOPLEVEL)
		return;

	 /* apply compositor's own minimization logic (hide) */
	set_minimized(shsurf->surface, 1);
}

static const struct xdg_surface_interface xdg_surface_implementation = {
	xdg_surface_destroy,
	xdg_surface_set_transient_for,
	xdg_surface_set_margin,
	xdg_surface_set_title,
	xdg_surface_set_app_id,
	xdg_surface_move,
	xdg_surface_resize,
	xdg_surface_set_output,
	xdg_surface_request_change_state,
	xdg_surface_ack_change_state,
	xdg_surface_set_minimized
};

static void
xdg_send_configure(struct weston_surface *surface,
	       uint32_t edges, int32_t width, int32_t height)
{
	struct shell_surface *shsurf = get_shell_surface(surface);

	assert(shsurf);

	xdg_surface_send_configure(shsurf->resource, width, height);
}

static const struct weston_shell_client xdg_client = {
	xdg_send_configure
};

static void
xdg_use_unstable_version(struct wl_client *client,
			 struct wl_resource *resource,
			 int32_t version)
{
	if (version > 1) {
		wl_resource_post_error(resource,
				       1,
				       "xdg-shell:: version not implemented yet.");
		return;
	}
}

static struct shell_surface *
create_xdg_surface(void *shell, struct weston_surface *surface,
		   const struct weston_shell_client *client)
{
	struct shell_surface *shsurf;

	shsurf = create_common_surface(shell, surface, client);
	shsurf->type = SHELL_SURFACE_TOPLEVEL;

	return shsurf;
}

static void
xdg_get_xdg_surface(struct wl_client *client,
		    struct wl_resource *resource,
		    uint32_t id,
		    struct wl_resource *surface_resource)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct shell_client *sc = wl_resource_get_user_data(resource);
	struct desktop_shell *shell = sc->shell;
	struct shell_surface *shsurf;

	if (get_shell_surface(surface)) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "xdg_shell::get_xdg_surface already requested");
		return;
	}

	shsurf = create_xdg_surface(shell, surface, &xdg_client);
	if (!shsurf) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->configure already set");
		return;
	}

	shsurf->resource =
		wl_resource_create(client,
				   &xdg_surface_interface, 1, id);
	wl_resource_set_implementation(shsurf->resource,
				       &xdg_surface_implementation,
				       shsurf, shell_destroy_shell_surface);
}

static bool
shell_surface_is_xdg_surface(struct shell_surface *shsurf)
{
	return shsurf->resource &&
		wl_resource_instance_of(shsurf->resource,
					&xdg_surface_interface,
					&xdg_surface_implementation);
}

/* xdg-popup implementation */

static void
xdg_popup_destroy(struct wl_client *client,
		  struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct xdg_popup_interface xdg_popup_implementation = {
	xdg_popup_destroy,
};

static void
xdg_popup_send_configure(struct weston_surface *surface,
			 uint32_t edges, int32_t width, int32_t height)
{
}

static const struct weston_shell_client xdg_popup_client = {
	xdg_popup_send_configure
};

static struct shell_surface *
create_xdg_popup(void *shell, struct weston_surface *surface,
		 const struct weston_shell_client *client,
		 struct weston_surface *parent,
		 struct shell_seat *seat,
		 uint32_t serial,
		 int32_t x, int32_t y)
{
	struct shell_surface *shsurf;

	shsurf = create_common_surface(shell, surface, client);
	shsurf->type = SHELL_SURFACE_POPUP;
	shsurf->popup.shseat = seat;
	shsurf->popup.serial = serial;
	shsurf->popup.x = x;
	shsurf->popup.y = y;
	shell_surface_set_parent(shsurf, parent);

	return shsurf;
}

static void
xdg_get_xdg_popup(struct wl_client *client,
		  struct wl_resource *resource,
		  uint32_t id,
		  struct wl_resource *surface_resource,
		  struct wl_resource *parent_resource,
		  struct wl_resource *seat_resource,
		  uint32_t serial,
		  int32_t x, int32_t y, uint32_t flags)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct shell_client *sc = wl_resource_get_user_data(resource);
	struct desktop_shell *shell = sc->shell;
	struct shell_surface *shsurf;
	struct weston_surface *parent;
	struct shell_seat *seat;

	if (get_shell_surface(surface)) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "xdg_shell::get_xdg_popup already requested");
		return;
	}

	if (!parent_resource) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "xdg_shell::get_xdg_popup requires a parent shell surface");
	}

	parent = wl_resource_get_user_data(parent_resource);
	seat = get_shell_seat(wl_resource_get_user_data(seat_resource));;

	shsurf = create_xdg_popup(shell, surface, &xdg_popup_client,
				  parent, seat, serial, x, y);
	if (!shsurf) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->configure already set");
		return;
	}

	shsurf->resource =
		wl_resource_create(client,
				   &xdg_popup_interface, 1, id);
	wl_resource_set_implementation(shsurf->resource,
				       &xdg_popup_implementation,
				       shsurf, shell_destroy_shell_surface);
}

static void
xdg_pong(struct wl_client *client,
	 struct wl_resource *resource, uint32_t serial)
{
	struct shell_client *sc = wl_resource_get_user_data(resource);

	shell_client_pong(sc, serial);
}

static bool
shell_surface_is_xdg_popup(struct shell_surface *shsurf)
{
	return wl_resource_instance_of(shsurf->resource,
				       &xdg_popup_interface,
				       &xdg_popup_implementation);
}

static const struct xdg_shell_interface xdg_implementation = {
	xdg_use_unstable_version,
	xdg_get_xdg_surface,
	xdg_get_xdg_popup,
	xdg_pong
};

static int
xdg_shell_unversioned_dispatch(const void *implementation,
			       void *_target, uint32_t opcode,
			       const struct wl_message *message,
			       union wl_argument *args)
{
	struct wl_resource *resource = _target;
	struct shell_client *sc = wl_resource_get_user_data(resource);

	if (opcode != 0) {
		wl_resource_post_error(resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "must call use_unstable_version first");
		return 0;
	}

#define XDG_SERVER_VERSION 3

	static_assert(XDG_SERVER_VERSION == XDG_SHELL_VERSION_CURRENT,
		      "shell implementation doesn't match protocol version");

	if (args[0].i != XDG_SERVER_VERSION) {
		wl_resource_post_error(resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "incompatible version, server is %d "
				       "client wants %d",
				       XDG_SERVER_VERSION, args[0].i);
		return 0;
	}

	wl_resource_set_implementation(resource, &xdg_implementation,
				       sc, NULL);

	return 1;
}

/* end of xdg-shell implementation */
/***********************************/

static void
shell_fade(struct desktop_shell *shell, enum fade_type type);

static int
screensaver_timeout(void *data)
{
	struct desktop_shell *shell = data;

	shell_fade(shell, FADE_OUT);

	return 1;
}

static void
handle_screensaver_sigchld(struct weston_process *proc, int status)
{
	struct desktop_shell *shell =
		container_of(proc, struct desktop_shell, screensaver.process);

	proc->pid = 0;

	if (shell->locked)
		weston_compositor_sleep(shell->compositor);
}

static void
launch_screensaver(struct desktop_shell *shell)
{
	if (shell->screensaver.binding)
		return;

	if (!shell->screensaver.path) {
		weston_compositor_sleep(shell->compositor);
		return;
	}

	if (shell->screensaver.process.pid != 0) {
		weston_log("old screensaver still running\n");
		return;
	}

	weston_client_launch(shell->compositor,
			   &shell->screensaver.process,
			   shell->screensaver.path,
			   handle_screensaver_sigchld);
}

static void
terminate_screensaver(struct desktop_shell *shell)
{
	if (shell->screensaver.process.pid == 0)
		return;

	/* Disarm the screensaver timer, otherwise it may fire when the
	 * compositor is not in the idle state. In that case, the screen will
	 * be locked, but the wake_signal won't fire on user input, making the
	 * system unresponsive. */
	wl_event_source_timer_update(shell->screensaver.timer, 0);

	kill(shell->screensaver.process.pid, SIGTERM);
}

static void
configure_static_view(struct weston_view *ev, struct weston_layer *layer)
{
	struct weston_view *v, *next;

	wl_list_for_each_safe(v, next, &layer->view_list, layer_link) {
		if (v->output == ev->output && v != ev) {
			weston_view_unmap(v);
			v->surface->configure = NULL;
		}
	}

	weston_view_set_position(ev, ev->output->x, ev->output->y);

	if (wl_list_empty(&ev->layer_link)) {
		wl_list_insert(&layer->view_list, &ev->layer_link);
		weston_compositor_schedule_repaint(ev->surface->compositor);
	}
}

static void
background_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
	struct desktop_shell *shell = es->configure_private;
	struct weston_view *view;

	view = container_of(es->views.next, struct weston_view, surface_link);

	configure_static_view(view, &shell->background_layer);
}

static void
desktop_shell_set_background(struct wl_client *client,
			     struct wl_resource *resource,
			     struct wl_resource *output_resource,
			     struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct weston_view *view, *next;

	if (surface->configure) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface role already assigned");
		return;
	}

	wl_list_for_each_safe(view, next, &surface->views, surface_link)
		weston_view_destroy(view);
	view = weston_view_create(surface);

	surface->configure = background_configure;
	surface->configure_private = shell;
	surface->output = wl_resource_get_user_data(output_resource);
	view->output = surface->output;
	desktop_shell_send_configure(resource, 0,
				     surface_resource,
				     surface->output->width,
				     surface->output->height);
}

static void
panel_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
	struct desktop_shell *shell = es->configure_private;
	struct weston_view *view;

	view = container_of(es->views.next, struct weston_view, surface_link);

	configure_static_view(view, &shell->panel_layer);
}

static void
desktop_shell_set_panel(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *output_resource,
			struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct weston_view *view, *next;

	if (surface->configure) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface role already assigned");
		return;
	}

	wl_list_for_each_safe(view, next, &surface->views, surface_link)
		weston_view_destroy(view);
	view = weston_view_create(surface);

	surface->configure = panel_configure;
	surface->configure_private = shell;
	surface->output = wl_resource_get_user_data(output_resource);
	view->output = surface->output;
	desktop_shell_send_configure(resource, 0,
				     surface_resource,
				     surface->output->width,
				     surface->output->height);
}

static void
lock_surface_configure(struct weston_surface *surface, int32_t sx, int32_t sy)
{
	struct desktop_shell *shell = surface->configure_private;
	struct weston_view *view;

	view = container_of(surface->views.next, struct weston_view, surface_link);

	if (surface->width == 0)
		return;

	center_on_output(view, get_default_output(shell->compositor));

	if (!weston_surface_is_mapped(surface)) {
		wl_list_insert(&shell->lock_layer.view_list,
			       &view->layer_link);
		weston_view_update_transform(view);
		shell_fade(shell, FADE_IN);
	}
}

static void
handle_lock_surface_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
	    container_of(listener, struct desktop_shell, lock_surface_listener);

	weston_log("lock surface gone\n");
	shell->lock_surface = NULL;
}

static void
desktop_shell_set_lock_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);

	shell->prepare_event_sent = false;

	if (!shell->locked)
		return;

	shell->lock_surface = surface;

	shell->lock_surface_listener.notify = handle_lock_surface_destroy;
	wl_signal_add(&surface->destroy_signal,
		      &shell->lock_surface_listener);

	weston_view_create(surface);
	surface->configure = lock_surface_configure;
	surface->configure_private = shell;
}

static void
resume_desktop(struct desktop_shell *shell)
{
	struct workspace *ws = get_current_workspace(shell);

	terminate_screensaver(shell);

	wl_list_remove(&shell->lock_layer.link);
	if (shell->showing_input_panels) {
		wl_list_insert(&shell->compositor->cursor_layer.link,
			       &shell->input_panel_layer.link);
		wl_list_insert(&shell->input_panel_layer.link,
			       &shell->fullscreen_layer.link);
	} else {
		wl_list_insert(&shell->compositor->cursor_layer.link,
			       &shell->fullscreen_layer.link);
	}
	wl_list_insert(&shell->fullscreen_layer.link,
		       &shell->panel_layer.link);
	wl_list_insert(&shell->panel_layer.link,
		       &ws->layer.link),

	restore_focus_state(shell, get_current_workspace(shell));

	shell->locked = false;
	shell_fade(shell, FADE_IN);
	weston_compositor_damage_all(shell->compositor);
}

static void
desktop_shell_unlock(struct wl_client *client,
		     struct wl_resource *resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	shell->prepare_event_sent = false;

	if (shell->locked)
		resume_desktop(shell);
}

static void
desktop_shell_set_grab_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	shell->grab_surface = wl_resource_get_user_data(surface_resource);
	weston_view_create(shell->grab_surface);
}

static void
desktop_shell_desktop_ready(struct wl_client *client,
			    struct wl_resource *resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	shell_fade_startup(shell);
}

static const struct desktop_shell_interface desktop_shell_implementation = {
	desktop_shell_set_background,
	desktop_shell_set_panel,
	desktop_shell_set_lock_surface,
	desktop_shell_unlock,
	desktop_shell_set_grab_surface,
	desktop_shell_desktop_ready
};

static enum shell_surface_type
get_shell_surface_type(struct weston_surface *surface)
{
	struct shell_surface *shsurf;

	shsurf = get_shell_surface(surface);
	if (!shsurf)
		return SHELL_SURFACE_NONE;
	return shsurf->type;
}

static void
move_binding(struct weston_seat *seat, uint32_t time, uint32_t button, void *data)
{
	struct weston_surface *focus;
	struct weston_surface *surface;
	struct shell_surface *shsurf;

	if (seat->pointer->focus == NULL)
		return;

	focus = seat->pointer->focus->surface;

	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (shsurf == NULL || shsurf->state.fullscreen ||
	    shsurf->state.maximized)
		return;

	surface_move(shsurf, (struct weston_seat *) seat);
}

static void
maximize_binding(struct weston_seat *seat, uint32_t time, uint32_t button, void *data)
{
	struct weston_surface *focus = seat->keyboard->focus;
	struct weston_surface *surface;
	struct shell_surface *shsurf;
	uint32_t serial;

	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (shsurf == NULL)
		return;

	if (!shell_surface_is_xdg_surface(shsurf))
		return;

	serial = wl_display_next_serial(seat->compositor->wl_display);
	xdg_surface_change_state(shsurf, XDG_SURFACE_STATE_MAXIMIZED,
				 !shsurf->state.maximized, serial);
}

static void
fullscreen_binding(struct weston_seat *seat, uint32_t time, uint32_t button, void *data)
{
	struct weston_surface *focus = seat->keyboard->focus;
	struct weston_surface *surface;
	struct shell_surface *shsurf;
	uint32_t serial;

	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (shsurf == NULL)
		return;

	if (!shell_surface_is_xdg_surface(shsurf))
		return;

	serial = wl_display_next_serial(seat->compositor->wl_display);
	xdg_surface_change_state(shsurf, XDG_SURFACE_STATE_FULLSCREEN,
				 !shsurf->state.fullscreen, serial);
}

static void
touch_move_binding(struct weston_seat *seat, uint32_t time, void *data)
{
	struct weston_surface *focus = seat->touch->focus->surface;
	struct weston_surface *surface;
	struct shell_surface *shsurf;

	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (shsurf == NULL || shsurf->state.fullscreen ||
	    shsurf->state.maximized)
		return;

	surface_touch_move(shsurf, (struct weston_seat *) seat);
}

static void
resize_binding(struct weston_seat *seat, uint32_t time, uint32_t button, void *data)
{
	struct weston_surface *focus;
	struct weston_surface *surface;
	uint32_t edges = 0;
	int32_t x, y;
	struct shell_surface *shsurf;

	if (seat->pointer->focus == NULL)
		return;

	focus = seat->pointer->focus->surface;

	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (shsurf == NULL || shsurf->state.fullscreen ||
	    shsurf->state.maximized)
		return;

	weston_view_from_global(shsurf->view,
				wl_fixed_to_int(seat->pointer->grab_x),
				wl_fixed_to_int(seat->pointer->grab_y),
				&x, &y);

	if (x < shsurf->surface->width / 3)
		edges |= WL_SHELL_SURFACE_RESIZE_LEFT;
	else if (x < 2 * shsurf->surface->width / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_SURFACE_RESIZE_RIGHT;

	if (y < shsurf->surface->height / 3)
		edges |= WL_SHELL_SURFACE_RESIZE_TOP;
	else if (y < 2 * shsurf->surface->height / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_SURFACE_RESIZE_BOTTOM;

	surface_resize(shsurf, (struct weston_seat *) seat, edges);
}

static void
surface_opacity_binding(struct weston_seat *seat, uint32_t time, uint32_t axis,
			wl_fixed_t value, void *data)
{
	float step = 0.005;
	struct shell_surface *shsurf;
	struct weston_surface *focus = seat->pointer->focus->surface;
	struct weston_surface *surface;

	/* XXX: broken for windows containing sub-surfaces */
	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (!shsurf)
		return;

	shsurf->view->alpha -= wl_fixed_to_double(value) * step;

	if (shsurf->view->alpha > 1.0)
		shsurf->view->alpha = 1.0;
	if (shsurf->view->alpha < step)
		shsurf->view->alpha = step;

	weston_view_geometry_dirty(shsurf->view);
	weston_surface_damage(surface);
}

static void
do_zoom(struct weston_seat *seat, uint32_t time, uint32_t key, uint32_t axis,
	wl_fixed_t value)
{
	struct weston_seat *ws = (struct weston_seat *) seat;
	struct weston_compositor *compositor = ws->compositor;
	struct weston_output *output;
	float increment;

	wl_list_for_each(output, &compositor->output_list, link) {
		if (pixman_region32_contains_point(&output->region,
						   wl_fixed_to_double(seat->pointer->x),
						   wl_fixed_to_double(seat->pointer->y),
						   NULL)) {
			if (key == KEY_PAGEUP)
				increment = output->zoom.increment;
			else if (key == KEY_PAGEDOWN)
				increment = -output->zoom.increment;
			else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
				/* For every pixel zoom 20th of a step */
				increment = output->zoom.increment *
					    -wl_fixed_to_double(value) / 20.0;
			else
				increment = 0;

			output->zoom.level += increment;

			if (output->zoom.level < 0.0)
				output->zoom.level = 0.0;
			else if (output->zoom.level > output->zoom.max_level)
				output->zoom.level = output->zoom.max_level;
			else if (!output->zoom.active) {
				weston_output_activate_zoom(output);
			}

			output->zoom.spring_z.target = output->zoom.level;

			weston_output_update_zoom(output);
		}
	}
}

static void
zoom_axis_binding(struct weston_seat *seat, uint32_t time, uint32_t axis,
		  wl_fixed_t value, void *data)
{
	do_zoom(seat, time, 0, axis, value);
}

static void
zoom_key_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		 void *data)
{
	do_zoom(seat, time, key, 0, 0);
}

static void
terminate_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		  void *data)
{
	struct weston_compositor *compositor = data;

	wl_display_terminate(compositor->wl_display);
}

static void
rotate_grab_motion(struct weston_pointer_grab *grab, uint32_t time,
		   wl_fixed_t x, wl_fixed_t y)
{
	struct rotate_grab *rotate =
		container_of(grab, struct rotate_grab, base.grab);
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = rotate->base.shsurf;
	float cx, cy, dx, dy, cposx, cposy, dposx, dposy, r;

	weston_pointer_move(pointer, x, y);

	if (!shsurf)
		return;

	cx = 0.5f * shsurf->surface->width;
	cy = 0.5f * shsurf->surface->height;

	dx = wl_fixed_to_double(pointer->x) - rotate->center.x;
	dy = wl_fixed_to_double(pointer->y) - rotate->center.y;
	r = sqrtf(dx * dx + dy * dy);

	wl_list_remove(&shsurf->rotation.transform.link);
	weston_view_geometry_dirty(shsurf->view);

	if (r > 20.0f) {
		struct weston_matrix *matrix =
			&shsurf->rotation.transform.matrix;

		weston_matrix_init(&rotate->rotation);
		weston_matrix_rotate_xy(&rotate->rotation, dx / r, dy / r);

		weston_matrix_init(matrix);
		weston_matrix_translate(matrix, -cx, -cy, 0.0f);
		weston_matrix_multiply(matrix, &shsurf->rotation.rotation);
		weston_matrix_multiply(matrix, &rotate->rotation);
		weston_matrix_translate(matrix, cx, cy, 0.0f);

		wl_list_insert(
			&shsurf->view->geometry.transformation_list,
			&shsurf->rotation.transform.link);
	} else {
		wl_list_init(&shsurf->rotation.transform.link);
		weston_matrix_init(&shsurf->rotation.rotation);
		weston_matrix_init(&rotate->rotation);
	}

	/* We need to adjust the position of the surface
	 * in case it was resized in a rotated state before */
	cposx = shsurf->view->geometry.x + cx;
	cposy = shsurf->view->geometry.y + cy;
	dposx = rotate->center.x - cposx;
	dposy = rotate->center.y - cposy;
	if (dposx != 0.0f || dposy != 0.0f) {
		weston_view_set_position(shsurf->view,
					 shsurf->view->geometry.x + dposx,
					 shsurf->view->geometry.y + dposy);
	}

	/* Repaint implies weston_surface_update_transform(), which
	 * lazily applies the damage due to rotation update.
	 */
	weston_compositor_schedule_repaint(shsurf->surface->compositor);
}

static void
rotate_grab_button(struct weston_pointer_grab *grab,
		   uint32_t time, uint32_t button, uint32_t state_w)
{
	struct rotate_grab *rotate =
		container_of(grab, struct rotate_grab, base.grab);
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = rotate->base.shsurf;
	enum wl_pointer_button_state state = state_w;

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (shsurf)
			weston_matrix_multiply(&shsurf->rotation.rotation,
					       &rotate->rotation);
		shell_grab_end(&rotate->base);
		free(rotate);
	}
}

static void
rotate_grab_cancel(struct weston_pointer_grab *grab)
{
	struct rotate_grab *rotate =
		container_of(grab, struct rotate_grab, base.grab);

	shell_grab_end(&rotate->base);
	free(rotate);
}

static const struct weston_pointer_grab_interface rotate_grab_interface = {
	noop_grab_focus,
	rotate_grab_motion,
	rotate_grab_button,
	rotate_grab_cancel,
};

static void
surface_rotate(struct shell_surface *surface, struct weston_seat *seat)
{
	struct rotate_grab *rotate;
	float dx, dy;
	float r;

	rotate = malloc(sizeof *rotate);
	if (!rotate)
		return;

	weston_view_to_global_float(surface->view,
				    surface->surface->width * 0.5f,
				    surface->surface->height * 0.5f,
				    &rotate->center.x, &rotate->center.y);

	dx = wl_fixed_to_double(seat->pointer->x) - rotate->center.x;
	dy = wl_fixed_to_double(seat->pointer->y) - rotate->center.y;
	r = sqrtf(dx * dx + dy * dy);
	if (r > 20.0f) {
		struct weston_matrix inverse;

		weston_matrix_init(&inverse);
		weston_matrix_rotate_xy(&inverse, dx / r, -dy / r);
		weston_matrix_multiply(&surface->rotation.rotation, &inverse);

		weston_matrix_init(&rotate->rotation);
		weston_matrix_rotate_xy(&rotate->rotation, dx / r, dy / r);
	} else {
		weston_matrix_init(&surface->rotation.rotation);
		weston_matrix_init(&rotate->rotation);
	}

	shell_grab_start(&rotate->base, &rotate_grab_interface, surface,
			 seat->pointer, DESKTOP_SHELL_CURSOR_ARROW);
}

static void
rotate_binding(struct weston_seat *seat, uint32_t time, uint32_t button,
	       void *data)
{
	struct weston_surface *focus;
	struct weston_surface *base_surface;
	struct shell_surface *surface;

	if (seat->pointer->focus == NULL)
		return;

	focus = seat->pointer->focus->surface;

	base_surface = weston_surface_get_main_surface(focus);
	if (base_surface == NULL)
		return;

	surface = get_shell_surface(base_surface);
	if (surface == NULL || surface->state.fullscreen ||
	    surface->state.maximized)
		return;

	surface_rotate(surface, seat);
}

/* Move all fullscreen layers down to the current workspace in a non-reversible
 * manner. This should be used when implementing shell-wide overlays, such as
 * the alt-tab switcher, which need to de-promote fullscreen layers. */
void
lower_fullscreen_layer(struct desktop_shell *shell)
{
	struct workspace *ws;
	struct weston_view *view, *prev;

	ws = get_current_workspace(shell);
	wl_list_for_each_reverse_safe(view, prev,
				      &shell->fullscreen_layer.view_list,
				      layer_link) {
		wl_list_remove(&view->layer_link);
		wl_list_insert(&ws->layer.view_list, &view->layer_link);
		weston_view_damage_below(view);
		weston_surface_damage(view->surface);
	}
}

void
activate(struct desktop_shell *shell, struct weston_surface *es,
	 struct weston_seat *seat)
{
	struct weston_surface *main_surface;
	struct focus_state *state;
	struct workspace *ws;
	struct weston_surface *old_es;
	struct shell_surface *shsurf;

	main_surface = weston_surface_get_main_surface(es);

	weston_surface_activate(es, seat);

	state = ensure_focus_state(shell, seat);
	if (state == NULL)
		return;

	old_es = state->keyboard_focus;
	focus_state_set_focus(state, es);

	shsurf = get_shell_surface(main_surface);
	assert(shsurf);

	if (shsurf->state.fullscreen)
		shell_configure_fullscreen(shsurf);
	else
		restore_all_output_modes(shell->compositor);

	/* Update the surface’s layer. This brings it to the top of the stacking
	 * order as appropriate. */
	shell_surface_update_layer(shsurf);

	if (shell->focus_animation_type != ANIMATION_NONE) {
		ws = get_current_workspace(shell);
		animate_focus_change(shell, ws, get_default_view(old_es), get_default_view(es));
	}
}

/* no-op func for checking black surface */
static void
black_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
}

static bool
is_black_surface (struct weston_surface *es, struct weston_surface **fs_surface)
{
	if (es->configure == black_surface_configure) {
		if (fs_surface)
			*fs_surface = (struct weston_surface *)es->configure_private;
		return true;
	}
	return false;
}

static void
activate_binding(struct weston_seat *seat,
		 struct desktop_shell *shell,
		 struct weston_surface *focus)
{
	struct weston_surface *main_surface;

	if (!focus)
		return;

	if (is_black_surface(focus, &main_surface))
		focus = main_surface;

	main_surface = weston_surface_get_main_surface(focus);
	if (get_shell_surface_type(main_surface) == SHELL_SURFACE_NONE)
		return;

	activate(shell, focus, seat);
}

static void
click_to_activate_binding(struct weston_seat *seat, uint32_t time, uint32_t button,
			  void *data)
{
	if (seat->pointer->grab != &seat->pointer->default_grab)
		return;
	if (seat->pointer->focus == NULL)
		return;

	activate_binding(seat, data, seat->pointer->focus->surface);
}

static void
touch_to_activate_binding(struct weston_seat *seat, uint32_t time, void *data)
{
	if (seat->touch->grab != &seat->touch->default_grab)
		return;
	if (seat->touch->focus == NULL)
		return;

	activate_binding(seat, data, seat->touch->focus->surface);
}

static void
unfocus_all_seats(struct desktop_shell *shell)
{
	struct weston_seat *seat, *next;

	wl_list_for_each_safe(seat, next, &shell->compositor->seat_list, link) {
		if (seat->keyboard == NULL)
			continue;

		weston_keyboard_set_focus(seat->keyboard, NULL);
	}
}

static void
lock(struct desktop_shell *shell)
{
	struct workspace *ws = get_current_workspace(shell);

	if (shell->locked) {
		weston_compositor_sleep(shell->compositor);
		return;
	}

	shell->locked = true;

	/* Hide all surfaces by removing the fullscreen, panel and
	 * toplevel layers.  This way nothing else can show or receive
	 * input events while we are locked. */

	wl_list_remove(&shell->panel_layer.link);
	wl_list_remove(&shell->fullscreen_layer.link);
	if (shell->showing_input_panels)
		wl_list_remove(&shell->input_panel_layer.link);
	wl_list_remove(&ws->layer.link);
	wl_list_insert(&shell->compositor->cursor_layer.link,
		       &shell->lock_layer.link);

	launch_screensaver(shell);

	/* Remove the keyboard focus on all seats. This will be
	 * restored to the workspace's saved state via
	 * restore_focus_state when the compositor is unlocked */
	unfocus_all_seats(shell);

	/* TODO: disable bindings that should not work while locked. */

	/* All this must be undone in resume_desktop(). */
}

static void
unlock(struct desktop_shell *shell)
{
	if (!shell->locked || shell->lock_surface) {
		shell_fade(shell, FADE_IN);
		return;
	}

	/* If desktop-shell client has gone away, unlock immediately. */
	if (!shell->child.desktop_shell) {
		resume_desktop(shell);
		return;
	}

	if (shell->prepare_event_sent)
		return;

	desktop_shell_send_prepare_lock_surface(shell->child.desktop_shell);
	shell->prepare_event_sent = true;
}

static void
shell_fade_done(struct weston_view_animation *animation, void *data)
{
	struct desktop_shell *shell = data;

	shell->fade.animation = NULL;

	switch (shell->fade.type) {
	case FADE_IN:
		weston_surface_destroy(shell->fade.view->surface);
		shell->fade.view = NULL;
		break;
	case FADE_OUT:
		lock(shell);
		break;
	default:
		break;
	}
}

static struct weston_view *
shell_fade_create_surface(struct desktop_shell *shell)
{
	struct weston_compositor *compositor = shell->compositor;
	struct weston_surface *surface;
	struct weston_view *view;

	surface = weston_surface_create(compositor);
	if (!surface)
		return NULL;

	view = weston_view_create(surface);
	if (!view) {
		weston_surface_destroy(surface);
		return NULL;
	}

	weston_surface_set_size(surface, 8192, 8192);
	weston_view_set_position(view, 0, 0);
	weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1.0);
	wl_list_insert(&compositor->fade_layer.view_list,
		       &view->layer_link);
	pixman_region32_init(&surface->input);

	return view;
}

static void
shell_fade(struct desktop_shell *shell, enum fade_type type)
{
	float tint;

	switch (type) {
	case FADE_IN:
		tint = 0.0;
		break;
	case FADE_OUT:
		tint = 1.0;
		break;
	default:
		weston_log("shell: invalid fade type\n");
		return;
	}

	shell->fade.type = type;

	if (shell->fade.view == NULL) {
		shell->fade.view = shell_fade_create_surface(shell);
		if (!shell->fade.view)
			return;

		shell->fade.view->alpha = 1.0 - tint;
		weston_view_update_transform(shell->fade.view);
	}

	if (shell->fade.view->output == NULL) {
		/* If the black view gets a NULL output, we lost the
		 * last output and we'll just cancel the fade.  This
		 * happens when you close the last window under the
		 * X11 or Wayland backends. */
		shell->locked = false;
		weston_surface_destroy(shell->fade.view->surface);
		shell->fade.view = NULL;
	} else if (shell->fade.animation) {
		weston_fade_update(shell->fade.animation, tint);
	} else {
		shell->fade.animation =
			weston_fade_run(shell->fade.view,
					1.0 - tint, tint, 300.0,
					shell_fade_done, shell);
	}
}

static void
do_shell_fade_startup(void *data)
{
	struct desktop_shell *shell = data;

	if (shell->startup_animation_type == ANIMATION_FADE)
		shell_fade(shell, FADE_IN);
	else if (shell->startup_animation_type == ANIMATION_NONE) {
		weston_surface_destroy(shell->fade.view->surface);
		shell->fade.view = NULL;
	}
}

static void
shell_fade_startup(struct desktop_shell *shell)
{
	struct wl_event_loop *loop;

	if (!shell->fade.startup_timer)
		return;

	wl_event_source_remove(shell->fade.startup_timer);
	shell->fade.startup_timer = NULL;

	loop = wl_display_get_event_loop(shell->compositor->wl_display);
	wl_event_loop_add_idle(loop, do_shell_fade_startup, shell);
}

static int
fade_startup_timeout(void *data)
{
	struct desktop_shell *shell = data;

	shell_fade_startup(shell);
	return 0;
}

static void
shell_fade_init(struct desktop_shell *shell)
{
	/* Make compositor output all black, and wait for the desktop-shell
	 * client to signal it is ready, then fade in. The timer triggers a
	 * fade-in, in case the desktop-shell client takes too long.
	 */

	struct wl_event_loop *loop;

	if (shell->fade.view != NULL) {
		weston_log("%s: warning: fade surface already exists\n",
			   __func__);
		return;
	}

	shell->fade.view = shell_fade_create_surface(shell);
	if (!shell->fade.view)
		return;

	weston_view_update_transform(shell->fade.view);
	weston_surface_damage(shell->fade.view->surface);

	loop = wl_display_get_event_loop(shell->compositor->wl_display);
	shell->fade.startup_timer =
		wl_event_loop_add_timer(loop, fade_startup_timeout, shell);
	wl_event_source_timer_update(shell->fade.startup_timer, 15000);
}

static void
idle_handler(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, idle_listener);
	struct weston_seat *seat;

	wl_list_for_each(seat, &shell->compositor->seat_list, link)
		if (seat->pointer)
			popup_grab_end(seat->pointer);

	shell_fade(shell, FADE_OUT);
	/* lock() is called from shell_fade_done() */
}

static void
wake_handler(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, wake_listener);

	unlock(shell);
}

static void
center_on_output(struct weston_view *view, struct weston_output *output)
{
	int32_t surf_x, surf_y, width, height;
	float x, y;

	surface_subsurfaces_boundingbox(view->surface, &surf_x, &surf_y, &width, &height);

	x = output->x + (output->width - width) / 2 - surf_x / 2;
	y = output->y + (output->height - height) / 2 - surf_y / 2;

	weston_view_set_position(view, x, y);
}

static void
weston_view_set_initial_position(struct weston_view *view,
				 struct desktop_shell *shell)
{
	struct weston_compositor *compositor = shell->compositor;
	int ix = 0, iy = 0;
	int range_x, range_y;
	int dx, dy, x, y, panel_height;
	struct weston_output *output, *target_output = NULL;
	struct weston_seat *seat;

	/* As a heuristic place the new window on the same output as the
	 * pointer. Falling back to the output containing 0, 0.
	 *
	 * TODO: Do something clever for touch too?
	 */
	wl_list_for_each(seat, &compositor->seat_list, link) {
		if (seat->pointer) {
			ix = wl_fixed_to_int(seat->pointer->x);
			iy = wl_fixed_to_int(seat->pointer->y);
			break;
		}
	}

	wl_list_for_each(output, &compositor->output_list, link) {
		if (pixman_region32_contains_point(&output->region, ix, iy, NULL)) {
			target_output = output;
			break;
		}
	}

	if (!target_output) {
		weston_view_set_position(view, 10 + random() % 400,
					 10 + random() % 400);
		return;
	}

	/* Valid range within output where the surface will still be onscreen.
	 * If this is negative it means that the surface is bigger than
	 * output.
	 */
	panel_height = get_output_panel_height(shell, target_output);
	range_x = target_output->width - view->surface->width;
	range_y = (target_output->height - panel_height) -
		  view->surface->height;

	if (range_x > 0)
		dx = random() % range_x;
	else
		dx = 0;

	if (range_y > 0)
		dy = panel_height + random() % range_y;
	else
		dy = panel_height;

	x = target_output->x + dx;
	y = target_output->y + dy;

	weston_view_set_position(view, x, y);
}

static void
map(struct desktop_shell *shell, struct shell_surface *shsurf,
    int32_t sx, int32_t sy)
{
	struct weston_compositor *compositor = shell->compositor;
	struct weston_seat *seat;
	int panel_height = 0;
	int32_t surf_x, surf_y;

	/* initial positioning, see also configure() */
	switch (shsurf->type) {
	case SHELL_SURFACE_TOPLEVEL:
		if (shsurf->state.fullscreen) {
			center_on_output(shsurf->view, shsurf->fullscreen_output);
			shell_map_fullscreen(shsurf);
		} else if (shsurf->state.maximized) {
			/* use surface configure to set the geometry */
			panel_height = get_output_panel_height(shell, shsurf->output);
			surface_subsurfaces_boundingbox(shsurf->surface,
							&surf_x, &surf_y, NULL, NULL);
			weston_view_set_position(shsurf->view,
						 shsurf->output->x - surf_x,
						 shsurf->output->y +
						 panel_height - surf_y);
		} else if (!shsurf->state.relative) {
			weston_view_set_initial_position(shsurf->view, shell);
		}
		break;
	case SHELL_SURFACE_POPUP:
		shell_map_popup(shsurf);
		break;
	case SHELL_SURFACE_NONE:
		weston_view_set_position(shsurf->view,
					 shsurf->view->geometry.x + sx,
					 shsurf->view->geometry.y + sy);
		break;
	case SHELL_SURFACE_XWAYLAND:
	default:
		;
	}

	/* Surface stacking order, see also activate(). */
	shell_surface_update_layer(shsurf);

	if (shsurf->type != SHELL_SURFACE_NONE) {
		weston_view_update_transform(shsurf->view);
		if (shsurf->state.maximized) {
			shsurf->surface->output = shsurf->output;
			shsurf->view->output = shsurf->output;
		}
	}

	switch (shsurf->type) {
	/* XXX: xwayland's using the same fields for transient type */
	case SHELL_SURFACE_XWAYLAND:
		if (shsurf->transient.flags ==
				WL_SHELL_SURFACE_TRANSIENT_INACTIVE)
			break;
	case SHELL_SURFACE_TOPLEVEL:
		if (shsurf->state.relative &&
		    shsurf->transient.flags == WL_SHELL_SURFACE_TRANSIENT_INACTIVE)
			break;
		if (shell->locked)
			break;
		wl_list_for_each(seat, &compositor->seat_list, link)
			activate(shell, shsurf->surface, seat);
		break;
	case SHELL_SURFACE_POPUP:
	case SHELL_SURFACE_NONE:
	default:
		break;
	}

	if (shsurf->type == SHELL_SURFACE_TOPLEVEL &&
	    !shsurf->state.maximized && !shsurf->state.fullscreen)
	{
		switch (shell->win_animation_type) {
		case ANIMATION_FADE:
			weston_fade_run(shsurf->view, 0.0, 1.0, 300.0, NULL, NULL);
			break;
		case ANIMATION_ZOOM:
			weston_zoom_run(shsurf->view, 0.5, 1.0, NULL, NULL);
			break;
		case ANIMATION_NONE:
		default:
			break;
		}
	}
}

static void
configure(struct desktop_shell *shell, struct weston_surface *surface,
	  float x, float y)
{
	struct shell_surface *shsurf;
	struct weston_view *view;
	int32_t mx, my, surf_x, surf_y;

	shsurf = get_shell_surface(surface);

	assert(shsurf);

	if (shsurf->state.fullscreen)
		shell_configure_fullscreen(shsurf);
	else if (shsurf->state.maximized) {
		/* setting x, y and using configure to change that geometry */
		surface_subsurfaces_boundingbox(shsurf->surface, &surf_x, &surf_y,
		                                                 NULL, NULL);
		mx = shsurf->output->x - surf_x;
		my = shsurf->output->y +
		     get_output_panel_height(shell,shsurf->output) - surf_y;
		weston_view_set_position(shsurf->view, mx, my);
	} else {
		weston_view_set_position(shsurf->view, x, y);
	}

	/* XXX: would a fullscreen surface need the same handling? */
	if (surface->output) {
		wl_list_for_each(view, &surface->views, surface_link)
			weston_view_update_transform(view);

		if (shsurf->state.maximized)
			surface->output = shsurf->output;
	}
}

static void
shell_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
	struct shell_surface *shsurf = get_shell_surface(es);
	struct desktop_shell *shell;
	int type_changed = 0;

	assert(shsurf);

	shell = shsurf->shell;

	if (!weston_surface_is_mapped(es) &&
	    !wl_list_empty(&shsurf->popup.grab_link)) {
		remove_popup_grab(shsurf);
	}

	if (es->width == 0)
		return;

	if (shsurf->state_changed) {
		set_surface_type(shsurf);
		type_changed = 1;
	}

	if (!weston_surface_is_mapped(es)) {
		map(shell, shsurf, sx, sy);
	} else if (type_changed || sx != 0 || sy != 0 ||
		   shsurf->last_width != es->width ||
		   shsurf->last_height != es->height) {
		float from_x, from_y;
		float to_x, to_y;

		if (shsurf->resize_edges) {
			sx = 0;
			sy = 0;
		}

		if (shsurf->resize_edges & WL_SHELL_SURFACE_RESIZE_LEFT)
			sx = shsurf->last_width - es->width;
		if (shsurf->resize_edges & WL_SHELL_SURFACE_RESIZE_TOP)
			sy = shsurf->last_height - es->height;

		shsurf->last_width = es->width;
		shsurf->last_height = es->height;

		weston_view_to_global_float(shsurf->view, 0, 0, &from_x, &from_y);
		weston_view_to_global_float(shsurf->view, sx, sy, &to_x, &to_y);
		configure(shell, es,
			  shsurf->view->geometry.x + to_x - from_x,
			  shsurf->view->geometry.y + to_y - from_y);
	}
}

static void launch_desktop_shell_process(void *data);

static void
desktop_shell_sigchld(struct weston_process *process, int status)
{
	uint32_t time;
	struct desktop_shell *shell =
		container_of(process, struct desktop_shell, child.process);

	shell->child.process.pid = 0;
	shell->child.client = NULL; /* already destroyed by wayland */

	/* if desktop-shell dies more than 5 times in 30 seconds, give up */
	time = weston_compositor_get_time();
	if (time - shell->child.deathstamp > 30000) {
		shell->child.deathstamp = time;
		shell->child.deathcount = 0;
	}

	shell->child.deathcount++;
	if (shell->child.deathcount > 5) {
		weston_log("%s died, giving up.\n", shell->client);
		return;
	}

	weston_log("%s died, respawning...\n", shell->client);
	launch_desktop_shell_process(shell);
	shell_fade_startup(shell);
}

static void
desktop_shell_client_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell;

	shell = container_of(listener, struct desktop_shell,
			     child.client_destroy_listener);

	shell->child.client = NULL;
}

static void
launch_desktop_shell_process(void *data)
{
	struct desktop_shell *shell = data;

	shell->child.client = weston_client_launch(shell->compositor,
						 &shell->child.process,
						 shell->client,
						 desktop_shell_sigchld);

	if (!shell->child.client)
		weston_log("not able to start %s\n", shell->client);

	shell->child.client_destroy_listener.notify =
		desktop_shell_client_destroy;
	wl_client_add_destroy_listener(shell->child.client,
				       &shell->child.client_destroy_listener);
}

static void
handle_shell_client_destroy(struct wl_listener *listener, void *data)
{
	struct shell_client *sc =
		container_of(listener, struct shell_client, destroy_listener);

	if (sc->ping_timer)
		wl_event_source_remove(sc->ping_timer);
	free(sc);
}

static struct shell_client *
shell_client_create(struct wl_client *client, struct desktop_shell *shell,
		    const struct wl_interface *interface, uint32_t id)
{
	struct shell_client *sc;

	sc = zalloc(sizeof *sc);
	if (sc == NULL) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	sc->resource = wl_resource_create(client, interface, 1, id);
	if (sc->resource == NULL) {
		free(sc);
		wl_client_post_no_memory(client);
		return NULL;
	}

	sc->client = client;
	sc->shell = shell;
	sc->destroy_listener.notify = handle_shell_client_destroy;
	wl_client_add_destroy_listener(client, &sc->destroy_listener);

	return sc;
}

static void
bind_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct shell_client *sc;

	sc = shell_client_create(client, shell, &wl_shell_interface, id);
	if (sc)
		wl_resource_set_implementation(sc->resource,
					       &shell_implementation,
					       shell, NULL);
}

static void
bind_xdg_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct shell_client *sc;

	sc = shell_client_create(client, shell, &xdg_shell_interface, id);
	if (sc)
		wl_resource_set_dispatcher(sc->resource,
					   xdg_shell_unversioned_dispatch,
					   NULL, sc, NULL);
}

static void
unbind_desktop_shell(struct wl_resource *resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	if (shell->locked)
		resume_desktop(shell);

	shell->child.desktop_shell = NULL;
	shell->prepare_event_sent = false;
}

static void
bind_desktop_shell(struct wl_client *client,
		   void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &desktop_shell_interface,
				      MIN(version, 2), id);

	if (client == shell->child.client) {
		wl_resource_set_implementation(resource,
					       &desktop_shell_implementation,
					       shell, unbind_desktop_shell);
		shell->child.desktop_shell = resource;

		if (version < 2)
			shell_fade_startup(shell);

		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "permission to bind desktop_shell denied");
	wl_resource_destroy(resource);
}

static void
screensaver_configure(struct weston_surface *surface, int32_t sx, int32_t sy)
{
	struct desktop_shell *shell = surface->configure_private;
	struct weston_view *view;

	if (surface->width == 0)
		return;

	/* XXX: starting weston-screensaver beforehand does not work */
	if (!shell->locked)
		return;

	view = container_of(surface->views.next, struct weston_view, surface_link);
	center_on_output(view, surface->output);

	if (wl_list_empty(&view->layer_link)) {
		wl_list_insert(shell->lock_layer.view_list.prev,
			       &view->layer_link);
		weston_view_update_transform(view);
		wl_event_source_timer_update(shell->screensaver.timer,
					     shell->screensaver.duration);
		shell_fade(shell, FADE_IN);
	}
}

static void
screensaver_set_surface(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *surface_resource,
			struct wl_resource *output_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct weston_output *output = wl_resource_get_user_data(output_resource);
	struct weston_view *view, *next;

	/* Make sure we only have one view */
	wl_list_for_each_safe(view, next, &surface->views, surface_link)
		weston_view_destroy(view);
	weston_view_create(surface);

	surface->configure = screensaver_configure;
	surface->configure_private = shell;
	surface->output = output;
}

static const struct screensaver_interface screensaver_implementation = {
	screensaver_set_surface
};

static void
unbind_screensaver(struct wl_resource *resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	shell->screensaver.binding = NULL;
}

static void
bind_screensaver(struct wl_client *client,
		 void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &screensaver_interface, 1, id);

	if (shell->screensaver.binding == NULL) {
		wl_resource_set_implementation(resource,
					       &screensaver_implementation,
					       shell, unbind_screensaver);
		shell->screensaver.binding = resource;
		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "interface object already bound");
	wl_resource_destroy(resource);
}

struct switcher {
	struct desktop_shell *shell;
	struct weston_surface *current;
	struct wl_listener listener;
	struct weston_keyboard_grab grab;
	struct wl_array minimized_array;
};

static void
switcher_next(struct switcher *switcher)
{
	struct weston_view *view;
	struct weston_surface *first = NULL, *prev = NULL, *next = NULL;
	struct shell_surface *shsurf;
	struct workspace *ws = get_current_workspace(switcher->shell);

	 /* temporary re-display minimized surfaces */
	struct weston_view *tmp;
	struct weston_view **minimized;
	wl_list_for_each_safe(view, tmp, &switcher->shell->minimized_layer.view_list, layer_link) {
		wl_list_remove(&view->layer_link);
		wl_list_insert(&ws->layer.view_list, &view->layer_link);
		minimized = wl_array_add(&switcher->minimized_array, sizeof *minimized);
		*minimized = view;
	}

	wl_list_for_each(view, &ws->layer.view_list, layer_link) {
		shsurf = get_shell_surface(view->surface);
		if (shsurf &&
		    shsurf->type == SHELL_SURFACE_TOPLEVEL &&
		    shsurf->parent == NULL) {
			if (first == NULL)
				first = view->surface;
			if (prev == switcher->current)
				next = view->surface;
			prev = view->surface;
			view->alpha = 0.25;
			weston_view_geometry_dirty(view);
			weston_surface_damage(view->surface);
		}

		if (is_black_surface(view->surface, NULL)) {
			view->alpha = 0.25;
			weston_view_geometry_dirty(view);
			weston_surface_damage(view->surface);
		}
	}

	if (next == NULL)
		next = first;

	if (next == NULL)
		return;

	wl_list_remove(&switcher->listener.link);
	wl_signal_add(&next->destroy_signal, &switcher->listener);

	switcher->current = next;
	wl_list_for_each(view, &next->views, surface_link)
		view->alpha = 1.0;

	shsurf = get_shell_surface(switcher->current);
	if (shsurf && shsurf->state.fullscreen)
		shsurf->fullscreen.black_view->alpha = 1.0;
}

static void
switcher_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct switcher *switcher =
		container_of(listener, struct switcher, listener);

	switcher_next(switcher);
}

static void
switcher_destroy(struct switcher *switcher)
{
	struct weston_view *view;
	struct weston_keyboard *keyboard = switcher->grab.keyboard;
	struct workspace *ws = get_current_workspace(switcher->shell);

	wl_list_for_each(view, &ws->layer.view_list, layer_link) {
		if (is_focus_view(view))
			continue;

		view->alpha = 1.0;
		weston_surface_damage(view->surface);
	}

	if (switcher->current)
		activate(switcher->shell, switcher->current,
			 (struct weston_seat *) keyboard->seat);
	wl_list_remove(&switcher->listener.link);
	weston_keyboard_end_grab(keyboard);
	if (keyboard->input_method_resource)
		keyboard->grab = &keyboard->input_method_grab;

	 /* re-hide surfaces that were temporary shown during the switch */
	struct weston_view **minimized;
	wl_array_for_each(minimized, &switcher->minimized_array) {
		/* with the exception of the current selected */
		if ((*minimized)->surface != switcher->current) {
			wl_list_remove(&(*minimized)->layer_link);
			wl_list_insert(&switcher->shell->minimized_layer.view_list, &(*minimized)->layer_link);
			weston_view_damage_below(*minimized);
		}
	}
	wl_array_release(&switcher->minimized_array);

	free(switcher);
}

static void
switcher_key(struct weston_keyboard_grab *grab,
	     uint32_t time, uint32_t key, uint32_t state_w)
{
	struct switcher *switcher = container_of(grab, struct switcher, grab);
	enum wl_keyboard_key_state state = state_w;

	if (key == KEY_TAB && state == WL_KEYBOARD_KEY_STATE_PRESSED)
		switcher_next(switcher);
}

static void
switcher_modifier(struct weston_keyboard_grab *grab, uint32_t serial,
		  uint32_t mods_depressed, uint32_t mods_latched,
		  uint32_t mods_locked, uint32_t group)
{
	struct switcher *switcher = container_of(grab, struct switcher, grab);
	struct weston_seat *seat = (struct weston_seat *) grab->keyboard->seat;

	if ((seat->modifier_state & switcher->shell->binding_modifier) == 0)
		switcher_destroy(switcher);
}

static void
switcher_cancel(struct weston_keyboard_grab *grab)
{
	struct switcher *switcher = container_of(grab, struct switcher, grab);

	switcher_destroy(switcher);
}

static const struct weston_keyboard_grab_interface switcher_grab = {
	switcher_key,
	switcher_modifier,
	switcher_cancel,
};

static void
switcher_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		 void *data)
{
	struct desktop_shell *shell = data;
	struct switcher *switcher;

	switcher = malloc(sizeof *switcher);
	switcher->shell = shell;
	switcher->current = NULL;
	switcher->listener.notify = switcher_handle_surface_destroy;
	wl_list_init(&switcher->listener.link);
	wl_array_init(&switcher->minimized_array);

	restore_all_output_modes(shell->compositor);
	lower_fullscreen_layer(switcher->shell);
	switcher->grab.interface = &switcher_grab;
	weston_keyboard_start_grab(seat->keyboard, &switcher->grab);
	weston_keyboard_set_focus(seat->keyboard, NULL);
	switcher_next(switcher);
}

static void
backlight_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		  void *data)
{
	struct weston_compositor *compositor = data;
	struct weston_output *output;
	long backlight_new = 0;

	/* TODO: we're limiting to simple use cases, where we assume just
	 * control on the primary display. We'd have to extend later if we
	 * ever get support for setting backlights on random desktop LCD
	 * panels though */
	output = get_default_output(compositor);
	if (!output)
		return;

	if (!output->set_backlight)
		return;

	if (key == KEY_F9 || key == KEY_BRIGHTNESSDOWN)
		backlight_new = output->backlight_current - 25;
	else if (key == KEY_F10 || key == KEY_BRIGHTNESSUP)
		backlight_new = output->backlight_current + 25;

	if (backlight_new < 5)
		backlight_new = 5;
	if (backlight_new > 255)
		backlight_new = 255;

	output->backlight_current = backlight_new;
	output->set_backlight(output, output->backlight_current);
}

struct debug_binding_grab {
	struct weston_keyboard_grab grab;
	struct weston_seat *seat;
	uint32_t key[2];
	int key_released[2];
};

static void
debug_binding_key(struct weston_keyboard_grab *grab, uint32_t time,
		  uint32_t key, uint32_t state)
{
	struct debug_binding_grab *db = (struct debug_binding_grab *) grab;
	struct weston_compositor *ec = db->seat->compositor;
	struct wl_display *display = ec->wl_display;
	struct wl_resource *resource;
	uint32_t serial;
	int send = 0, terminate = 0;
	int check_binding = 1;
	int i;
	struct wl_list *resource_list;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		/* Do not run bindings on key releases */
		check_binding = 0;

		for (i = 0; i < 2; i++)
			if (key == db->key[i])
				db->key_released[i] = 1;

		if (db->key_released[0] && db->key_released[1]) {
			/* All key releases been swalled so end the grab */
			terminate = 1;
		} else if (key != db->key[0] && key != db->key[1]) {
			/* Should not swallow release of other keys */
			send = 1;
		}
	} else if (key == db->key[0] && !db->key_released[0]) {
		/* Do not check bindings for the first press of the binding
		 * key. This allows it to be used as a debug shortcut.
		 * We still need to swallow this event. */
		check_binding = 0;
	} else if (db->key[1]) {
		/* If we already ran a binding don't process another one since
		 * we can't keep track of all the binding keys that were
		 * pressed in order to swallow the release events. */
		send = 1;
		check_binding = 0;
	}

	if (check_binding) {
		if (weston_compositor_run_debug_binding(ec, db->seat, time,
							key, state)) {
			/* We ran a binding so swallow the press and keep the
			 * grab to swallow the released too. */
			send = 0;
			terminate = 0;
			db->key[1] = key;
		} else {
			/* Terminate the grab since the key pressed is not a
			 * debug binding key. */
			send = 1;
			terminate = 1;
		}
	}

	if (send) {
		serial = wl_display_next_serial(display);
		resource_list = &grab->keyboard->focus_resource_list;
		wl_resource_for_each(resource, resource_list) {
			wl_keyboard_send_key(resource, serial, time, key, state);
		}
	}

	if (terminate) {
		weston_keyboard_end_grab(grab->keyboard);
		if (grab->keyboard->input_method_resource)
			grab->keyboard->grab = &grab->keyboard->input_method_grab;
		free(db);
	}
}

static void
debug_binding_modifiers(struct weston_keyboard_grab *grab, uint32_t serial,
			uint32_t mods_depressed, uint32_t mods_latched,
			uint32_t mods_locked, uint32_t group)
{
	struct wl_resource *resource;
	struct wl_list *resource_list;

	resource_list = &grab->keyboard->focus_resource_list;

	wl_resource_for_each(resource, resource_list) {
		wl_keyboard_send_modifiers(resource, serial, mods_depressed,
					   mods_latched, mods_locked, group);
	}
}

static void
debug_binding_cancel(struct weston_keyboard_grab *grab)
{
	struct debug_binding_grab *db = (struct debug_binding_grab *) grab;

	weston_keyboard_end_grab(grab->keyboard);
	free(db);
}

struct weston_keyboard_grab_interface debug_binding_keyboard_grab = {
	debug_binding_key,
	debug_binding_modifiers,
	debug_binding_cancel,
};

static void
debug_binding(struct weston_seat *seat, uint32_t time, uint32_t key, void *data)
{
	struct debug_binding_grab *grab;

	grab = calloc(1, sizeof *grab);
	if (!grab)
		return;

	grab->seat = (struct weston_seat *) seat;
	grab->key[0] = key;
	grab->grab.interface = &debug_binding_keyboard_grab;
	weston_keyboard_start_grab(seat->keyboard, &grab->grab);
}

static void
force_kill_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		   void *data)
{
	struct weston_surface *focus_surface;
	struct wl_client *client;
	struct desktop_shell *shell = data;
	struct weston_compositor *compositor = shell->compositor;
	pid_t pid;

	focus_surface = seat->keyboard->focus;
	if (!focus_surface)
		return;

	wl_signal_emit(&compositor->kill_signal, focus_surface);

	client = wl_resource_get_client(focus_surface->resource);
	wl_client_get_credentials(client, &pid, NULL, NULL);

	/* Skip clients that we launched ourselves (the credentials of
	 * the socketpair is ours) */
	if (pid == getpid())
		return;

	kill(pid, SIGKILL);
}

static void
workspace_up_binding(struct weston_seat *seat, uint32_t time,
		     uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;
	if (new_index != 0)
		new_index--;

	change_workspace(shell, new_index);
}

static void
workspace_down_binding(struct weston_seat *seat, uint32_t time,
		       uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;
	if (new_index < shell->workspaces.num - 1)
		new_index++;

	change_workspace(shell, new_index);
}

static void
workspace_f_binding(struct weston_seat *seat, uint32_t time,
		    uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index;

	if (shell->locked)
		return;
	new_index = key - KEY_F1;
	if (new_index >= shell->workspaces.num)
		new_index = shell->workspaces.num - 1;

	change_workspace(shell, new_index);
}

static void
workspace_move_surface_up_binding(struct weston_seat *seat, uint32_t time,
				  uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;

	if (new_index != 0)
		new_index--;

	take_surface_to_workspace_by_seat(shell, seat, new_index);
}

static void
workspace_move_surface_down_binding(struct weston_seat *seat, uint32_t time,
				    uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;

	if (new_index < shell->workspaces.num - 1)
		new_index++;

	take_surface_to_workspace_by_seat(shell, seat, new_index);
}

static void
shell_reposition_view_on_output_destroy(struct weston_view *view)
{
	struct weston_output *output, *first_output;
	struct weston_compositor *ec = view->surface->compositor;
	struct shell_surface *shsurf;
	float x, y;
	int visible;

	x = view->geometry.x;
	y = view->geometry.y;

	/* At this point the destroyed output is not in the list anymore.
	 * If the view is still visible somewhere, we leave where it is,
	 * otherwise, move it to the first output. */
	visible = 0;
	wl_list_for_each(output, &ec->output_list, link) {
		if (pixman_region32_contains_point(&output->region,
						   x, y, NULL)) {
			visible = 1;
			break;
		}
	}

	if (!visible) {
		first_output = container_of(ec->output_list.next,
					    struct weston_output, link);

		x = first_output->x + first_output->width / 4;
		y = first_output->y + first_output->height / 4;
	}

	weston_view_set_position(view, x, y);

	shsurf = get_shell_surface(view->surface);

	if (shsurf) {
		shsurf->saved_position_valid = false;
		shsurf->next_state.maximized = false;
		shsurf->next_state.fullscreen = false;
		shsurf->state_changed = true;
	}
}

static void
shell_reposition_views_on_output_destroy(struct shell_output *shell_output)
{
	struct desktop_shell *shell = shell_output->shell;
	struct weston_output *output = shell_output->output;
	struct weston_layer *layer;
	struct weston_view *view;

	/* Move all views in the layers owned by the shell */
	wl_list_for_each(layer, shell->fullscreen_layer.link.prev, link) {
		wl_list_for_each(view, &layer->view_list, layer_link) {
			if (view->output != output)
				continue;

			shell_reposition_view_on_output_destroy(view);
		}

		/* We don't start from the beggining of the layer list, so
		 * make sure we don't wrap around it. */
		if (layer == &shell->background_layer)
			break;
	}
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct shell_output *output_listener =
		container_of(listener, struct shell_output, destroy_listener);

	shell_reposition_views_on_output_destroy(output_listener);

	wl_list_remove(&output_listener->destroy_listener.link);
	wl_list_remove(&output_listener->link);
	free(output_listener);
}

static void
create_shell_output(struct desktop_shell *shell,
					struct weston_output *output)
{
	struct shell_output *shell_output;

	shell_output = zalloc(sizeof *shell_output);
	if (shell_output == NULL)
		return;

	shell_output->output = output;
	shell_output->shell = shell;
	shell_output->destroy_listener.notify = handle_output_destroy;
	wl_signal_add(&output->destroy_signal,
		      &shell_output->destroy_listener);
	wl_list_insert(shell->output_list.prev, &shell_output->link);
}

static void
handle_output_create(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, output_create_listener);
	struct weston_output *output = (struct weston_output *)data;

	create_shell_output(shell, output);
}

static void
handle_output_move(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell;
	struct weston_output *output;
	struct weston_layer *layer;
	struct weston_view *view;
	float x, y;

	shell = container_of(listener, struct desktop_shell,
			     output_move_listener);
	output = data;

	/* Move all views in the layers owned by the shell */
	wl_list_for_each(layer, shell->fullscreen_layer.link.prev, link) {
		wl_list_for_each(view, &layer->view_list, layer_link) {
			if (view->output != output)
				continue;

			x = view->geometry.x + output->move_x;
			y = view->geometry.y + output->move_y;
			weston_view_set_position(view, x, y);
		}

		/* We don't start from the beggining of the layer list, so
		 * make sure we don't wrap around it. */
		if (layer == &shell->background_layer)
			break;
	}
}

static void
setup_output_destroy_handler(struct weston_compositor *ec,
							struct desktop_shell *shell)
{
	struct weston_output *output;

	wl_list_init(&shell->output_list);
	wl_list_for_each(output, &ec->output_list, link)
		create_shell_output(shell, output);

	shell->output_create_listener.notify = handle_output_create;
	wl_signal_add(&ec->output_created_signal,
				&shell->output_create_listener);

	shell->output_move_listener.notify = handle_output_move;
	wl_signal_add(&ec->output_moved_signal, &shell->output_move_listener);
}

static void
shell_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, destroy_listener);
	struct workspace **ws;
	struct shell_output *shell_output, *tmp;

	/* Force state to unlocked so we don't try to fade */
	shell->locked = false;
	if (shell->child.client)
		wl_client_destroy(shell->child.client);

	wl_list_remove(&shell->idle_listener.link);
	wl_list_remove(&shell->wake_listener.link);

	input_panel_destroy(shell);

	wl_list_for_each_safe(shell_output, tmp, &shell->output_list, link) {
		wl_list_remove(&shell_output->destroy_listener.link);
		wl_list_remove(&shell_output->link);
		free(shell_output);
	}

	wl_list_remove(&shell->output_create_listener.link);

	wl_array_for_each(ws, &shell->workspaces.array)
		workspace_destroy(*ws);
	wl_array_release(&shell->workspaces.array);

	free(shell->screensaver.path);
	free(shell->client);
	free(shell);
}

static void
shell_add_bindings(struct weston_compositor *ec, struct desktop_shell *shell)
{
	uint32_t mod;
	int i, num_workspace_bindings;

	/* fixed bindings */
	weston_compositor_add_key_binding(ec, KEY_BACKSPACE,
				          MODIFIER_CTRL | MODIFIER_ALT,
				          terminate_binding, ec);
	weston_compositor_add_button_binding(ec, BTN_LEFT, 0,
					     click_to_activate_binding,
					     shell);
	weston_compositor_add_button_binding(ec, BTN_RIGHT, 0,
					     click_to_activate_binding,
					     shell);
	weston_compositor_add_touch_binding(ec, 0,
					    touch_to_activate_binding,
					    shell);
	weston_compositor_add_axis_binding(ec, WL_POINTER_AXIS_VERTICAL_SCROLL,
				           MODIFIER_SUPER | MODIFIER_ALT,
				           surface_opacity_binding, NULL);
	weston_compositor_add_axis_binding(ec, WL_POINTER_AXIS_VERTICAL_SCROLL,
					   MODIFIER_SUPER, zoom_axis_binding,
					   NULL);

	/* configurable bindings */
	mod = shell->binding_modifier;
	weston_compositor_add_key_binding(ec, KEY_PAGEUP, mod,
					  zoom_key_binding, NULL);
	weston_compositor_add_key_binding(ec, KEY_PAGEDOWN, mod,
					  zoom_key_binding, NULL);
	weston_compositor_add_key_binding(ec, KEY_M, mod | MODIFIER_SHIFT,
					  maximize_binding, NULL);
	weston_compositor_add_key_binding(ec, KEY_F, mod | MODIFIER_SHIFT,
					  fullscreen_binding, NULL);
	weston_compositor_add_button_binding(ec, BTN_LEFT, mod, move_binding,
					     shell);
	weston_compositor_add_touch_binding(ec, mod, touch_move_binding, shell);
	weston_compositor_add_button_binding(ec, BTN_MIDDLE, mod,
					     resize_binding, shell);
	weston_compositor_add_button_binding(ec, BTN_LEFT,
					     mod | MODIFIER_SHIFT,
					     resize_binding, shell);

	if (ec->capabilities & WESTON_CAP_ROTATION_ANY)
		weston_compositor_add_button_binding(ec, BTN_RIGHT, mod,
						     rotate_binding, NULL);

	weston_compositor_add_key_binding(ec, KEY_TAB, mod, switcher_binding,
					  shell);
	weston_compositor_add_key_binding(ec, KEY_F9, mod, backlight_binding,
					  ec);
	weston_compositor_add_key_binding(ec, KEY_BRIGHTNESSDOWN, 0,
				          backlight_binding, ec);
	weston_compositor_add_key_binding(ec, KEY_F10, mod, backlight_binding,
					  ec);
	weston_compositor_add_key_binding(ec, KEY_BRIGHTNESSUP, 0,
				          backlight_binding, ec);
	weston_compositor_add_key_binding(ec, KEY_K, mod,
				          force_kill_binding, shell);
	weston_compositor_add_key_binding(ec, KEY_UP, mod,
					  workspace_up_binding, shell);
	weston_compositor_add_key_binding(ec, KEY_DOWN, mod,
					  workspace_down_binding, shell);
	weston_compositor_add_key_binding(ec, KEY_UP, mod | MODIFIER_SHIFT,
					  workspace_move_surface_up_binding,
					  shell);
	weston_compositor_add_key_binding(ec, KEY_DOWN, mod | MODIFIER_SHIFT,
					  workspace_move_surface_down_binding,
					  shell);

	if (shell->exposay_modifier)
		weston_compositor_add_modifier_binding(ec, shell->exposay_modifier,
						       exposay_binding, shell);

	/* Add bindings for mod+F[1-6] for workspace 1 to 6. */
	if (shell->workspaces.num > 1) {
		num_workspace_bindings = shell->workspaces.num;
		if (num_workspace_bindings > 6)
			num_workspace_bindings = 6;
		for (i = 0; i < num_workspace_bindings; i++)
			weston_compositor_add_key_binding(ec, KEY_F1 + i, mod,
							  workspace_f_binding,
							  shell);
	}

	/* Debug bindings */
	weston_compositor_add_key_binding(ec, KEY_SPACE, mod | MODIFIER_SHIFT,
					  debug_binding, shell);
}

WL_EXPORT int
module_init(struct weston_compositor *ec,
	    int *argc, char *argv[])
{
	struct weston_seat *seat;
	struct desktop_shell *shell;
	struct workspace **pws;
	unsigned int i;
	struct wl_event_loop *loop;

	shell = zalloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	shell->compositor = ec;

	shell->destroy_listener.notify = shell_destroy;
	wl_signal_add(&ec->destroy_signal, &shell->destroy_listener);
	shell->idle_listener.notify = idle_handler;
	wl_signal_add(&ec->idle_signal, &shell->idle_listener);
	shell->wake_listener.notify = wake_handler;
	wl_signal_add(&ec->wake_signal, &shell->wake_listener);

	ec->shell_interface.shell = shell;
	ec->shell_interface.create_shell_surface = create_shell_surface;
	ec->shell_interface.get_primary_view = get_primary_view;
	ec->shell_interface.set_toplevel = set_toplevel;
	ec->shell_interface.set_transient = set_transient;
	ec->shell_interface.set_fullscreen = set_fullscreen;
	ec->shell_interface.set_xwayland = set_xwayland;
	ec->shell_interface.move = surface_move;
	ec->shell_interface.resize = surface_resize;
	ec->shell_interface.set_title = set_title;

	weston_layer_init(&shell->fullscreen_layer, &ec->cursor_layer.link);
	weston_layer_init(&shell->panel_layer, &shell->fullscreen_layer.link);
	weston_layer_init(&shell->background_layer, &shell->panel_layer.link);
	weston_layer_init(&shell->lock_layer, NULL);
	weston_layer_init(&shell->input_panel_layer, NULL);

	wl_array_init(&shell->workspaces.array);
	wl_list_init(&shell->workspaces.client_list);

	if (input_panel_setup(shell) < 0)
		return -1;

	shell_configuration(shell);

	shell->exposay.state_cur = EXPOSAY_LAYOUT_INACTIVE;
	shell->exposay.state_target = EXPOSAY_TARGET_CANCEL;

	for (i = 0; i < shell->workspaces.num; i++) {
		pws = wl_array_add(&shell->workspaces.array, sizeof *pws);
		if (pws == NULL)
			return -1;

		*pws = workspace_create();
		if (*pws == NULL)
			return -1;
	}
	activate_workspace(shell, 0);

	weston_layer_init(&shell->minimized_layer, NULL);

	wl_list_init(&shell->workspaces.anim_sticky_list);
	wl_list_init(&shell->workspaces.animation.link);
	shell->workspaces.animation.frame = animate_workspace_change_frame;

	if (wl_global_create(ec->wl_display, &wl_shell_interface, 1,
				  shell, bind_shell) == NULL)
		return -1;

	if (wl_global_create(ec->wl_display, &xdg_shell_interface, 1,
				  shell, bind_xdg_shell) == NULL)
		return -1;

	if (wl_global_create(ec->wl_display,
			     &desktop_shell_interface, 2,
			     shell, bind_desktop_shell) == NULL)
		return -1;

	if (wl_global_create(ec->wl_display, &screensaver_interface, 1,
			     shell, bind_screensaver) == NULL)
		return -1;

	if (wl_global_create(ec->wl_display, &workspace_manager_interface, 1,
			     shell, bind_workspace_manager) == NULL)
		return -1;

	shell->child.deathstamp = weston_compositor_get_time();

	setup_output_destroy_handler(ec, shell);

	loop = wl_display_get_event_loop(ec->wl_display);
	wl_event_loop_add_idle(loop, launch_desktop_shell_process, shell);

	shell->screensaver.timer =
		wl_event_loop_add_timer(loop, screensaver_timeout, shell);

	wl_list_for_each(seat, &ec->seat_list, link) {
		create_pointer_focus_listener(seat);
		create_keyboard_focus_listener(seat);
	}

	screenshooter_create(ec);

	shell_add_bindings(ec, shell);

	shell_fade_init(shell);

	return 0;
}
