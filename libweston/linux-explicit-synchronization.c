/*
 * Copyright © 2018 Collabora, Ltd.
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

#include "config.h"

#include <inttypes.h>

#include "compositor.h"
#include "linux-explicit-synchronization.h"
#include "linux-explicit-synchronization-unstable-v1-server-protocol.h"

static void
destroy_linux_surface_synchronization(struct wl_resource *resource)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(resource);

	if (surface)
		surface->synchronization_resource = NULL;
}

static void
linux_surface_synchronization_destroy(struct wl_client *client,
				      struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
linux_surface_synchronization_set_acquire_fence(struct wl_client *client,
						struct wl_resource *resource,
						int32_t fd)
{
	wl_client_post_no_memory(client);
}

static void
linux_surface_synchronization_get_release(struct wl_client *client,
					  struct wl_resource *resource,
					  uint32_t id)
{
	wl_client_post_no_memory(client);
}

const struct zwp_linux_surface_synchronization_v1_interface
linux_surface_synchronization_implementation = {
	linux_surface_synchronization_destroy,
	linux_surface_synchronization_set_acquire_fence,
	linux_surface_synchronization_get_release,
};

static void
linux_explicit_synchronization_destroy(struct wl_client *client,
				       struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
linux_explicit_synchronization_get_synchronization(struct wl_client *client,
						   struct wl_resource *resource,
						   uint32_t id,
						   struct wl_resource *surface_resource)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);

	if (surface->synchronization_resource) {
		wl_resource_post_error(
			resource,
			ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_V1_ERROR_SYNCHRONIZATION_EXISTS,
			"wl_surface@%"PRIu32" already has a synchronization object",
			wl_resource_get_id(surface_resource));
		return;
	}

	surface->synchronization_resource =
		wl_resource_create(client,
				   &zwp_linux_surface_synchronization_v1_interface,
				   wl_resource_get_version(resource), id);
	if (!surface->synchronization_resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(surface->synchronization_resource,
				       &linux_surface_synchronization_implementation,
				       surface,
				       destroy_linux_surface_synchronization);
}

static const struct zwp_linux_explicit_synchronization_v1_interface
linux_explicit_synchronization_implementation = {
	linux_explicit_synchronization_destroy,
	linux_explicit_synchronization_get_synchronization
};

static void
bind_linux_explicit_synchronization(struct wl_client *client,
				    void *data, uint32_t version,
				    uint32_t id)
{
	struct weston_compositor *compositor = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client,
			&zwp_linux_explicit_synchronization_v1_interface,
			version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource,
	       &linux_explicit_synchronization_implementation,
	       compositor, NULL);
}

/** Advertise linux_explicit_synchronization support
 *
 * Calling this initializes the zwp_linux_explicit_synchronization_v1
 * protocol support, so that the interface will be advertised to clients.
 * Essentially it creates a global. Do not call this function multiple times
 * in the compositor's lifetime. There is no way to deinit explicitly, globals
 * will be reaped when the wl_display gets destroyed.
 *
 * \param compositor The compositor to init for.
 * \return Zero on success, -1 on failure.
 */
WL_EXPORT int
linux_explicit_synchronization_setup(struct weston_compositor *compositor)
{
	/* TODO: Update to minor version 2 when the next version of
	 * wayland-protocols that contains it is released. */
	if (!wl_global_create(compositor->wl_display,
			      &zwp_linux_explicit_synchronization_v1_interface,
			      1, compositor,
			      bind_linux_explicit_synchronization))
		return -1;

	return 0;
}
