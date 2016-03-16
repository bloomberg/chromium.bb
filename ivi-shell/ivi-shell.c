/*
 * Copyright (C) 2013 DENSO CORPORATION
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * ivi-shell supports a type of shell for In-Vehicle Infotainment system.
 * In-Vehicle Infotainment system traditionally manages surfaces with global
 * identification. A protocol, ivi_application, supports such a feature
 * by implementing a request, ivi_application::surface_creation defined in
 * ivi_application.xml.
 *
 *  The ivi-shell explicitly loads a module to add business logic like how to
 *  layout surfaces by using internal ivi-layout APIs.
 */
#include "config.h"

#include <string.h>
#include <dlfcn.h>
#include <limits.h>
#include <assert.h>
#include <linux/input.h>

#include "ivi-shell.h"
#include "ivi-application-server-protocol.h"
#include "ivi-layout-export.h"
#include "ivi-layout-shell.h"
#include "shared/helpers.h"

/* Representation of ivi_surface protocol object. */
struct ivi_shell_surface
{
	struct wl_resource* resource;
	struct ivi_shell *shell;
	struct ivi_layout_surface *layout_surface;

	struct weston_surface *surface;
	struct wl_listener surface_destroy_listener;

	uint32_t id_surface;

	int32_t width;
	int32_t height;

	struct wl_list link;
};

struct ivi_shell_setting
{
	char *ivi_module;
	int developermode;
};

/*
 * Implementation of ivi_surface
 */

static void
ivi_shell_surface_configure(struct weston_surface *, int32_t, int32_t);

static struct ivi_shell_surface *
get_ivi_shell_surface(struct weston_surface *surface)
{
	struct ivi_shell_surface *shsurf;

	if (surface->configure != ivi_shell_surface_configure)
		return NULL;

	shsurf = surface->configure_private;
	assert(shsurf);
	assert(shsurf->surface == surface);

	return shsurf;
}

void
shell_surface_send_configure(struct weston_surface *surface,
			     int32_t width, int32_t height)
{
	struct ivi_shell_surface *shsurf;

	shsurf = get_ivi_shell_surface(surface);
	assert(shsurf);
	if (!shsurf)
		return;

	if (shsurf->resource)
		ivi_surface_send_configure(shsurf->resource, width, height);
}

static void
ivi_shell_surface_configure(struct weston_surface *surface,
			    int32_t sx, int32_t sy)
{
	struct ivi_shell_surface *ivisurf = get_ivi_shell_surface(surface);

	assert(ivisurf);
	if (!ivisurf)
		return;

	if (surface->width == 0 || surface->height == 0)
		return;

	if (ivisurf->width != surface->width ||
	    ivisurf->height != surface->height) {
		ivisurf->width  = surface->width;
		ivisurf->height = surface->height;

		ivi_layout_surface_configure(ivisurf->layout_surface,
					     surface->width, surface->height);
	}
}

static int
ivi_shell_surface_get_label(struct weston_surface *surface,
			    char *buf,
			    size_t len)
{
	struct ivi_shell_surface *shell_surf = get_ivi_shell_surface(surface);

	if (!shell_surf)
		return snprintf(buf, len, "unidentified window in ivi-shell");

	return snprintf(buf, len, "ivi-surface %#x", shell_surf->id_surface);
}

static void
layout_surface_cleanup(struct ivi_shell_surface *ivisurf)
{
	assert(ivisurf->layout_surface != NULL);

	ivi_layout_surface_destroy(ivisurf->layout_surface);
	ivisurf->layout_surface = NULL;

	ivisurf->surface->configure = NULL;
	ivisurf->surface->configure_private = NULL;
	weston_surface_set_label_func(ivisurf->surface, NULL);
	ivisurf->surface = NULL;

	// destroy weston_surface destroy signal.
	wl_list_remove(&ivisurf->surface_destroy_listener.link);
}

/*
 * The ivi_surface wl_resource destructor.
 *
 * Gets called via ivi_surface.destroy request or automatic wl_client clean-up.
 */
static void
shell_destroy_shell_surface(struct wl_resource *resource)
{
	struct ivi_shell_surface *ivisurf = wl_resource_get_user_data(resource);

	if (ivisurf == NULL)
		return;

	assert(ivisurf->resource == resource);

	if (ivisurf->layout_surface != NULL)
		layout_surface_cleanup(ivisurf);

	wl_list_remove(&ivisurf->link);

	free(ivisurf);
}

/* Gets called through the weston_surface destroy signal. */
static void
shell_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct ivi_shell_surface *ivisurf =
			container_of(listener, struct ivi_shell_surface,
				     surface_destroy_listener);

	assert(ivisurf != NULL);

	if (ivisurf->layout_surface != NULL)
		layout_surface_cleanup(ivisurf);
}

/* Gets called, when a client sends ivi_surface.destroy request. */
static void
surface_destroy(struct wl_client *client, struct wl_resource *resource)
{
	/*
	 * Fires the wl_resource destroy signal, and then calls
	 * ivi_surface wl_resource destructor: shell_destroy_shell_surface()
	 */
	wl_resource_destroy(resource);
}

static const struct ivi_surface_interface surface_implementation = {
	surface_destroy,
};

/**
 * Request handler for ivi_application.surface_create.
 *
 * Creates an ivi_surface protocol object associated with the given wl_surface.
 * ivi_surface protocol object is represented by struct ivi_shell_surface.
 *
 * \param client The client.
 * \param resource The ivi_application protocol object.
 * \param id_surface The IVI surface ID.
 * \param surface_resource The wl_surface protocol object.
 * \param id The protocol object id for the new ivi_surface protocol object.
 *
 * The wl_surface is given the ivi_surface role and associated with a unique
 * IVI ID which is used to identify the surface in a controller
 * (window manager).
 */
static void
application_surface_create(struct wl_client *client,
			   struct wl_resource *resource,
			   uint32_t id_surface,
			   struct wl_resource *surface_resource,
			   uint32_t id)
{
	struct ivi_shell *shell = wl_resource_get_user_data(resource);
	struct ivi_shell_surface *ivisurf;
	struct ivi_layout_surface *layout_surface;
	struct weston_surface *weston_surface =
		wl_resource_get_user_data(surface_resource);
	struct wl_resource *res;

	if (weston_surface_set_role(weston_surface, "ivi_surface",
				    resource, IVI_APPLICATION_ERROR_ROLE) < 0)
		return;

	layout_surface = ivi_layout_surface_create(weston_surface, id_surface);

	/* check if id_ivi is already used for wl_surface*/
	if (layout_surface == NULL) {
		wl_resource_post_error(resource,
				       IVI_APPLICATION_ERROR_IVI_ID,
				       "surface_id is already assigned "
				       "by another app");
		return;
	}

	ivisurf = zalloc(sizeof *ivisurf);
	if (ivisurf == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_list_init(&ivisurf->link);
	wl_list_insert(&shell->ivi_surface_list, &ivisurf->link);

	ivisurf->shell = shell;
	ivisurf->id_surface = id_surface;

	ivisurf->width = 0;
	ivisurf->height = 0;
	ivisurf->layout_surface = layout_surface;

	/*
	 * The following code relies on wl_surface destruction triggering
	 * immediateweston_surface destruction
	 */
	ivisurf->surface_destroy_listener.notify = shell_handle_surface_destroy;
	wl_signal_add(&weston_surface->destroy_signal,
		      &ivisurf->surface_destroy_listener);

	ivisurf->surface = weston_surface;

	weston_surface->configure = ivi_shell_surface_configure;
	weston_surface->configure_private = ivisurf;
	weston_surface_set_label_func(weston_surface,
				      ivi_shell_surface_get_label);

	res = wl_resource_create(client, &ivi_surface_interface, 1, id);
	if (res == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	ivisurf->resource = res;

	wl_resource_set_implementation(res, &surface_implementation,
				       ivisurf, shell_destroy_shell_surface);
}

static const struct ivi_application_interface application_implementation = {
	application_surface_create
};

/*
 * Handle wl_registry.bind of ivi_application global singleton.
 */
static void
bind_ivi_application(struct wl_client *client,
		     void *data, uint32_t version, uint32_t id)
{
	struct ivi_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &ivi_application_interface,
				      1, id);

	wl_resource_set_implementation(resource,
				       &application_implementation,
				       shell, NULL);
}

struct weston_view *
get_default_view(struct weston_surface *surface)
{
	struct ivi_shell_surface *shsurf;
	struct weston_view *view;

	if (!surface || wl_list_empty(&surface->views))
		return NULL;

	shsurf = get_ivi_shell_surface(surface);
	if (shsurf && shsurf->layout_surface) {
		view = ivi_layout_get_weston_view(shsurf->layout_surface);
		if (view)
			return view;
	}

	wl_list_for_each(view, &surface->views, surface_link) {
		if (weston_view_is_mapped(view))
			return view;
	}

	return container_of(surface->views.next,
			    struct weston_view, surface_link);
}

/*
 * Called through the compositor's destroy signal.
 */
static void
shell_destroy(struct wl_listener *listener, void *data)
{
	struct ivi_shell *shell =
		container_of(listener, struct ivi_shell, destroy_listener);
	struct ivi_shell_surface *ivisurf, *next;

	text_backend_destroy(shell->text_backend);
	input_panel_destroy(shell);

	wl_list_for_each_safe(ivisurf, next, &shell->ivi_surface_list, link) {
		wl_list_remove(&ivisurf->link);
		free(ivisurf);
	}

	free(shell);
}

static void
terminate_binding(struct weston_keyboard *keyboard, uint32_t time,
		  uint32_t key, void *data)
{
	struct weston_compositor *compositor = data;

	wl_display_terminate(compositor->wl_display);
}

static void
init_ivi_shell(struct weston_compositor *compositor, struct ivi_shell *shell,
	       const struct ivi_shell_setting *setting)
{
	shell->compositor = compositor;

	wl_list_init(&shell->ivi_surface_list);

	weston_layer_init(&shell->input_panel_layer, NULL);

	if (setting->developermode) {
		weston_install_debug_key_binding(compositor, MODIFIER_SUPER);

		weston_compositor_add_key_binding(compositor, KEY_BACKSPACE,
						  MODIFIER_CTRL | MODIFIER_ALT,
						  terminate_binding,
						  compositor);
	}
}

static int
ivi_shell_setting_create(struct ivi_shell_setting *dest,
			 struct weston_compositor *compositor,
			 int *argc, char *argv[])
{
	int result = 0;
	struct weston_config *config = compositor->config;
	struct weston_config_section *section;

	const struct weston_option ivi_shell_options[] = {
		{ WESTON_OPTION_STRING, "ivi-module", 0, &dest->ivi_module },
	};

	parse_options(ivi_shell_options, ARRAY_LENGTH(ivi_shell_options),
		      argc, argv);

	section = weston_config_get_section(config, "ivi-shell", NULL, NULL);

	if (!dest->ivi_module &&
	    weston_config_section_get_string(section, "ivi-module",
					     &dest->ivi_module, NULL) < 0) {
		weston_log("Error: ivi-shell: No ivi-module set\n");
		result = -1;
	}

	weston_config_section_get_bool(section, "developermode",
				       &dest->developermode, 0);

	return result;
}

static void
activate_binding(struct weston_seat *seat,
		 struct weston_view *focus_view)
{
	struct weston_surface *focus = focus_view->surface;
	struct weston_surface *main_surface =
		weston_surface_get_main_surface(focus);

	if (get_ivi_shell_surface(main_surface) == NULL)
		return;

	weston_surface_activate(focus, seat);
}

static void
click_to_activate_binding(struct weston_pointer *pointer, uint32_t time,
			  uint32_t button, void *data)
{
	if (pointer->grab != &pointer->default_grab)
		return;
	if (pointer->focus == NULL)
		return;

	activate_binding(pointer->seat, pointer->focus);
}

static void
touch_to_activate_binding(struct weston_touch *touch, uint32_t time,
			  void *data)
{
	if (touch->grab != &touch->default_grab)
		return;
	if (touch->focus == NULL)
		return;

	activate_binding(touch->seat, touch->focus);
}

static void
shell_add_bindings(struct weston_compositor *compositor,
		   struct ivi_shell *shell)
{
	weston_compositor_add_button_binding(compositor, BTN_LEFT, 0,
					     click_to_activate_binding,
					     shell);
	weston_compositor_add_button_binding(compositor, BTN_RIGHT, 0,
					     click_to_activate_binding,
					     shell);
	weston_compositor_add_touch_binding(compositor, 0,
					    touch_to_activate_binding,
					    shell);
}

/*
 * Initialization of ivi-shell.
 */
WL_EXPORT int
module_init(struct weston_compositor *compositor,
	    int *argc, char *argv[])
{
	struct ivi_shell *shell;
	struct ivi_shell_setting setting = { };
	int retval = -1;

	shell = zalloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	if (ivi_shell_setting_create(&setting, compositor, argc, argv) != 0)
		return -1;

	init_ivi_shell(compositor, shell, &setting);

	shell->destroy_listener.notify = shell_destroy;
	wl_signal_add(&compositor->destroy_signal, &shell->destroy_listener);

	if (input_panel_setup(shell) < 0)
		goto out_settings;

	shell->text_backend = text_backend_init(compositor);
	if (!shell->text_backend)
		goto out_settings;

	if (wl_global_create(compositor->wl_display,
			     &ivi_application_interface, 1,
			     shell, bind_ivi_application) == NULL)
		goto out_settings;

	ivi_layout_init_with_compositor(compositor);
	shell_add_bindings(compositor, shell);

	/* Call module_init of ivi-modules which are defined in weston.ini */
	if (load_controller_modules(compositor, setting.ivi_module,
				    argc, argv) < 0)
		goto out_settings;

	retval = 0;

out_settings:
	free(setting.ivi_module);

	return retval;
}
