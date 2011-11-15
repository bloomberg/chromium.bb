/*
 * Copyright © 2011 Intel Corporation
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

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <linux/input.h>

#include "compositor.h"
#include "tablet-shell-server-protocol.h"

/*
 * TODO: Don't fade back from black until we've received a lockscreen
 * attachment.
 */

enum {
	STATE_STARTING,
	STATE_LOCKED,
	STATE_HOME,
	STATE_SWITCHER,
	STATE_TASK
};

struct tablet_shell {
	struct wl_resource resource;

	struct wlsc_shell shell;

	struct wlsc_compositor *compositor;
	struct wlsc_process process;
	struct wlsc_input_device *device;
	struct wl_client *client;

	struct wlsc_surface *surface;

	struct wlsc_surface *lockscreen_surface;
	struct wl_listener lockscreen_listener;

	struct wlsc_surface *home_surface;

	struct wlsc_surface *switcher_surface;
	struct wl_listener switcher_listener;

	struct tablet_client *current_client;

	int state, previous_state;
	int long_press_active;
	struct wl_event_source *long_press_source;
};

struct tablet_client {
	struct wl_resource resource;
	struct tablet_shell *shell;
	struct wl_client *client;
	struct wlsc_surface *surface;
	char *name;
};

struct tablet_zoom {
	struct wlsc_surface *surface;
	struct wlsc_animation animation;
	struct wlsc_spring spring;
	struct wlsc_transform transform;
	struct wl_listener listener;
	struct tablet_shell *shell;
	void (*done)(struct tablet_zoom *zoom);
};


static void
tablet_shell_sigchld(struct wlsc_process *process, int status)
{
	struct tablet_shell *shell =
		container_of(process, struct tablet_shell, process);

	shell->process.pid = 0;

	fprintf(stderr, "meego-ux-daemon crashed, exit code %d\n", status);
}

static void
tablet_shell_set_state(struct tablet_shell *shell, int state)
{
	static const char *states[] = {
		"STARTING", "LOCKED", "HOME", "SWITCHER", "TASK"
	};

	fprintf(stderr, "switching to state %s (from %s)\n",
		states[state], states[shell->state]);
	shell->previous_state = shell->state;
	shell->state = state;
}

static void
tablet_zoom_destroy(struct tablet_zoom *zoom)
{
	wl_list_remove(&zoom->animation.link);
	zoom->surface->transform = NULL;
	if (zoom->done)
		zoom->done(zoom);
	free(zoom);
}

static void
handle_zoom_surface_destroy(struct wl_listener *listener,
			    struct wl_resource *resource, uint32_t time)
{
	struct tablet_zoom *zoom =
		container_of(listener, struct tablet_zoom, listener);

	fprintf(stderr, "animation surface gone\n");
	tablet_zoom_destroy(zoom);
}

static void
tablet_zoom_frame(struct wlsc_animation *animation,
			struct wlsc_output *output, uint32_t msecs)
{
	struct tablet_zoom *zoom =
		container_of(animation, struct tablet_zoom, animation);
	struct wlsc_surface *es = zoom->surface;
	GLfloat scale;

	wlsc_spring_update(&zoom->spring, msecs);

	if (wlsc_spring_done(&zoom->spring)) {
		fprintf(stderr, "animation done\n");
		tablet_zoom_destroy(zoom);
	}

	scale = zoom->spring.current;
	wlsc_matrix_init(&zoom->transform.matrix);
	wlsc_matrix_translate(&zoom->transform.matrix,
			      -es->width / 2.0, -es->height / 2.0, 0);
	wlsc_matrix_scale(&zoom->transform.matrix, scale, scale, scale);
	wlsc_matrix_translate(&zoom->transform.matrix,
			      es->width / 2.0, es->height / 2.0, 0);

	scale = 1.0 / zoom->spring.current;
	wlsc_matrix_init(&zoom->transform.inverse);
	wlsc_matrix_scale(&zoom->transform.inverse, scale, scale, scale);

	wlsc_surface_damage(es);
}

static struct tablet_zoom *
tablet_zoom_run(struct tablet_shell *shell,
		      struct wlsc_surface *surface,
		      GLfloat start, GLfloat stop)
{
	struct tablet_zoom *zoom;

	fprintf(stderr, "starting animation for surface %p\n", surface);

	zoom = malloc(sizeof *zoom);
	if (!zoom)
		return NULL;

	zoom->shell = shell;
	zoom->surface = surface;
	zoom->done = NULL;
	surface->transform = &zoom->transform;
	wlsc_spring_init(&zoom->spring, 200.0, start, stop);
	zoom->spring.timestamp = wlsc_compositor_get_time();
	zoom->animation.frame = tablet_zoom_frame;
	tablet_zoom_frame(&zoom->animation, NULL,
				zoom->spring.timestamp);

	zoom->listener.func = handle_zoom_surface_destroy;
	wl_list_insert(surface->surface.resource.destroy_listener_list.prev,
		       &zoom->listener.link);

	wl_list_insert(shell->compositor->animation_list.prev,
		       &zoom->animation.link);

	return zoom;
}

static void
tablet_shell_map(struct wlsc_shell *base, struct wlsc_surface *surface,
		       int32_t width, int32_t height)
{
	struct tablet_shell *shell =
		container_of(base, struct tablet_shell, shell);

	surface->x = 0;
	surface->y = 0;

	if (surface == shell->lockscreen_surface) {
		wlsc_compositor_fade(shell->compositor, 0.0);
		wlsc_compositor_wake(shell->compositor);
	} else if (surface == shell->switcher_surface) {
		/* */
	} else if (surface == shell->home_surface) {
		if (shell->state == STATE_STARTING) {
			tablet_shell_set_state(shell, STATE_LOCKED);
			shell->previous_state = STATE_HOME;
			wl_resource_post_event(&shell->resource,
					       TABLET_SHELL_SHOW_LOCKSCREEN);
		}
	} else if (shell->current_client &&
		   shell->current_client->surface != surface &&
		   shell->current_client->client == surface->surface.resource.client) {
		tablet_shell_set_state(shell, STATE_TASK);
		shell->current_client->surface = surface;
		tablet_zoom_run(shell, surface, 0.3, 1.0);
	}

	wl_list_insert(&shell->compositor->surface_list, &surface->link);
	wlsc_surface_configure(surface, surface->x, surface->y, width, height);
}

static void
tablet_shell_configure(struct wlsc_shell *base,
			     struct wlsc_surface *surface,
			     int32_t x, int32_t y,
			     int32_t width, int32_t height)
{
	wlsc_surface_configure(surface, x, y, width, height);
}

static void
handle_lockscreen_surface_destroy(struct wl_listener *listener,
				  struct wl_resource *resource, uint32_t time)
{
	struct tablet_shell *shell =
		container_of(listener,
			     struct tablet_shell, lockscreen_listener);

	shell->lockscreen_surface = NULL;
	tablet_shell_set_state(shell, shell->previous_state);
}

static void
tablet_shell_set_lockscreen(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct wlsc_surface *es = surface_resource->data;
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) shell->compositor->input_device;

	es->x = 0;
	es->y = 0;
	wlsc_surface_activate(es, device, wlsc_compositor_get_time());
	shell->lockscreen_surface = es;

	shell->lockscreen_listener.func = handle_lockscreen_surface_destroy;
	wl_list_insert(es->surface.resource.destroy_listener_list.prev,
		       &shell->lockscreen_listener.link);
}

static void
handle_switcher_surface_destroy(struct wl_listener *listener,
				struct wl_resource *resource, uint32_t time)
{
	struct tablet_shell *shell =
		container_of(listener,
			     struct tablet_shell, switcher_listener);

	shell->switcher_surface = NULL;
	if (shell->state != STATE_LOCKED)
		tablet_shell_set_state(shell, shell->previous_state);
}

static void
tablet_shell_set_switcher(struct wl_client *client,
			  struct wl_resource *resource,
			  struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) shell->compositor->input_device;
	struct wlsc_surface *es = surface_resource->data;

	/* FIXME: Switcher should be centered and the compositor
	 * should do the tinting of the background.  With the cache
	 * layer idea, we should be able to hit the framerate on the
	 * fade/zoom in. */
	shell->switcher_surface = es;
	shell->switcher_surface->x = 0;
	shell->switcher_surface->y = 0;

	wlsc_surface_activate(es, device, wlsc_compositor_get_time());

	shell->switcher_listener.func = handle_switcher_surface_destroy;
	wl_list_insert(es->surface.resource.destroy_listener_list.prev,
		       &shell->switcher_listener.link);
}

static void
tablet_shell_set_homescreen(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) shell->compositor->input_device;

	shell->home_surface = surface_resource->data;
	shell->home_surface->x = 0;
	shell->home_surface->y = 0;

	wlsc_surface_activate(shell->home_surface, device,
			      wlsc_compositor_get_time());
}

static void
minimize_zoom_done(struct tablet_zoom *zoom)
{
	struct tablet_shell *shell = zoom->shell;
	struct wlsc_compositor *compositor = shell->compositor;
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) compositor->input_device;

	wlsc_surface_activate(shell->home_surface,
			      device, wlsc_compositor_get_time());
}

static void
tablet_shell_switch_to(struct tablet_shell *shell,
			     struct wlsc_surface *surface)
{
	struct wlsc_compositor *compositor = shell->compositor;
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) compositor->input_device;
	struct wlsc_surface *current;
	struct tablet_zoom *zoom;

	if (shell->state == STATE_SWITCHER) {
		wl_list_remove(&shell->switcher_listener.link);
		shell->switcher_surface = NULL;
	};

	if (surface == shell->home_surface) {
		tablet_shell_set_state(shell, STATE_HOME);

		if (shell->current_client && shell->current_client->surface) {
			current = shell->current_client->surface;
			zoom = tablet_zoom_run(shell, current, 1.0, 0.3);
			zoom->done = minimize_zoom_done;
		}
	} else {
		fprintf(stderr, "switch to %p\n", surface);
		wlsc_surface_activate(surface, device,
				      wlsc_compositor_get_time());
		tablet_shell_set_state(shell, STATE_TASK);
		tablet_zoom_run(shell, surface, 0.3, 1.0);
	}
}

static void
tablet_shell_show_grid(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct wlsc_surface *es = surface_resource->data;

	tablet_shell_switch_to(shell, es);
}

static void
tablet_shell_show_panels(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct wlsc_surface *es = surface_resource->data;

	tablet_shell_switch_to(shell, es);
}

static void
destroy_tablet_client(struct wl_resource *resource)
{
	struct tablet_client *tablet_client =
		container_of(resource, struct tablet_client, resource);

	free(tablet_client->name);
	free(tablet_client);
}

static void
tablet_client_destroy(struct wl_client *client,
		      struct wl_resource *resource)
{
	wl_resource_destroy(resource, wlsc_compositor_get_time());
}

static void
tablet_client_activate(struct wl_client *client, struct wl_resource *resource)
{
	struct tablet_client *tablet_client = resource->data;
	struct tablet_shell *shell = tablet_client->shell;

	shell->current_client = tablet_client;
	if (!tablet_client->surface)
		return;

	tablet_shell_switch_to(shell, tablet_client->surface);
}

static const struct tablet_client_interface tablet_client_implementation = {
	tablet_client_destroy,
	tablet_client_activate
};

static void
tablet_shell_create_client(struct wl_client *client,
			   struct wl_resource *resource,
			   uint32_t id, const char *name, int fd)
{
	struct tablet_shell *shell = resource->data;
	struct wlsc_compositor *compositor = shell->compositor;
	struct tablet_client *tablet_client;

	tablet_client = malloc(sizeof *tablet_client);
	if (tablet_client == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	tablet_client->client = wl_client_create(compositor->wl_display, fd);
	tablet_client->shell = shell;
	tablet_client->name = strdup(name);

	tablet_client->resource.destroy = destroy_tablet_client;
	tablet_client->resource.object.id = id;
	tablet_client->resource.object.interface =
		&tablet_client_interface;
	tablet_client->resource.object.implementation =
		(void (**)(void)) &tablet_client_implementation;

	wl_client_add_resource(client, &tablet_client->resource);
	tablet_client->surface = NULL;
	shell->current_client = tablet_client;

	fprintf(stderr, "created client %p, id %d, name %s, fd %d\n",
		tablet_client->client, id, name, fd);
}

static const struct tablet_shell_interface tablet_shell_implementation = {
	tablet_shell_set_lockscreen,
	tablet_shell_set_switcher,
	tablet_shell_set_homescreen,
	tablet_shell_show_grid,
	tablet_shell_show_panels,
	tablet_shell_create_client
};

static void
launch_ux_daemon(struct tablet_shell *shell)
{
	struct wlsc_compositor *compositor = shell->compositor;
	char s[32];
	int sv[2], flags;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
		fprintf(stderr, "socketpair failed\n");
		return;
	}

	shell->process.pid = fork();
	shell->process.cleanup = tablet_shell_sigchld;

	switch (shell->process.pid) {
	case 0:
		/* SOCK_CLOEXEC closes both ends, so we need to unset
		 * the flag on the client fd. */
		flags = fcntl(sv[1], F_GETFD);
		if (flags != -1)
			fcntl(sv[1], F_SETFD, flags & ~FD_CLOEXEC);

		snprintf(s, sizeof s, "%d", sv[1]);
		setenv("WAYLAND_SOCKET", s, 1);
		setenv("QT_QPA_PLATFORM", "waylandgl", 1);
		if (execl("/usr/libexec/meego-ux-daemon",
			  "/usr/libexec/meego-ux-daemon", NULL) < 0)
			fprintf(stderr, "exec failed: %m\n");
		exit(-1);

	default:
		close(sv[1]);
		shell->client =
			wl_client_create(compositor->wl_display, sv[0]);
		wlsc_watch_process(&shell->process);
		break;

	case -1:
		fprintf(stderr, "failed to fork\n");
		break;
	}
}

static void
toggle_switcher(struct tablet_shell *shell)
{
	switch (shell->state) {
	case STATE_SWITCHER:
		wl_resource_post_event(&shell->resource,
				     TABLET_SHELL_HIDE_SWITCHER);
		break;
	default:
		wl_resource_post_event(&shell->resource,
				       TABLET_SHELL_SHOW_SWITCHER);
		tablet_shell_set_state(shell, STATE_SWITCHER);
		break;
	}
}

static void
tablet_shell_lock(struct wlsc_shell *base)
{
	struct tablet_shell *shell =
		container_of(base, struct tablet_shell, shell);

	if (shell->state == STATE_LOCKED)
		return;
	if (shell->state == STATE_SWITCHER)
		wl_resource_post_event(&shell->resource,
				       TABLET_SHELL_HIDE_SWITCHER);

	wl_resource_post_event(&shell->resource,
			       TABLET_SHELL_SHOW_LOCKSCREEN);
	
	tablet_shell_set_state(shell, STATE_LOCKED);
}

static void
tablet_shell_unlock(struct wlsc_shell *base)
{
	struct tablet_shell *shell =
		container_of(base, struct tablet_shell, shell);

	wlsc_compositor_wake(shell->compositor);
}

static void
go_home(struct tablet_shell *shell)
{
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) shell->compositor->input_device;

	if (shell->state == STATE_SWITCHER)
		wl_resource_post_event(&shell->resource,
				       TABLET_SHELL_HIDE_SWITCHER);

	wlsc_surface_activate(shell->home_surface, device,
			      wlsc_compositor_get_time());

	tablet_shell_set_state(shell, STATE_HOME);
}

static int
long_press_handler(void *data)
{
	struct tablet_shell *shell = data;

	shell->long_press_active = 0;
	toggle_switcher(shell);

	return 1;
}

static void
menu_key_binding(struct wl_input_device *device, uint32_t time,
		 uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct tablet_shell *shell = data;

	if (shell->state == STATE_LOCKED)
		return;

	if (state)
		toggle_switcher(shell);
}

static void
home_key_binding(struct wl_input_device *device, uint32_t time,
		 uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct tablet_shell *shell = data;

	if (shell->state == STATE_LOCKED)
		return;

	shell->device = (struct wlsc_input_device *) device;

	if (state) {
		wl_event_source_timer_update(shell->long_press_source, 500);
		shell->long_press_active = 1;
	} else if (shell->long_press_active) {
		wl_event_source_timer_update(shell->long_press_source, 0);
		shell->long_press_active = 0;

		switch (shell->state) {
		case STATE_HOME:
		case STATE_SWITCHER:
			toggle_switcher(shell);
			break;
		default:
			go_home(shell);
			break;
		}
	}
}

static void
tablet_shell_set_selection_focus(struct wlsc_shell *shell,
				       struct wl_selection *selection,
				       struct wl_surface *surface,
				       uint32_t time)
{
}

static void
bind_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct tablet_shell *shell = data;

	if (shell->client != client)
		/* Throw an error or just let the client fail when it
		 * tries to access the object?. */
		return;

	shell->resource.client = client;
	shell->resource.object.id = id;
}

void
shell_init(struct wlsc_compositor *compositor);

WL_EXPORT void
shell_init(struct wlsc_compositor *compositor)
{
	struct tablet_shell *shell;
	struct wl_event_loop *loop;

	shell = malloc(sizeof *shell);
	if (shell == NULL)
		return;

	memset(shell, 0, sizeof *shell);
	shell->compositor = compositor;

	shell->resource.object.interface = &tablet_shell_interface;
	shell->resource.object.implementation =
		(void (**)(void)) &tablet_shell_implementation;

	/* FIXME: This will make the object available to all clients. */
	wl_display_add_global(compositor->wl_display,
			      &wl_shell_interface, shell, bind_shell);

	loop = wl_display_get_event_loop(compositor->wl_display);
	shell->long_press_source =
		wl_event_loop_add_timer(loop, long_press_handler, shell);

	wlsc_compositor_add_binding(compositor, KEY_LEFTMETA, 0, 0,
				    home_key_binding, shell);
	wlsc_compositor_add_binding(compositor, KEY_RIGHTMETA, 0, 0,
				    home_key_binding, shell);
	wlsc_compositor_add_binding(compositor, KEY_LEFTMETA, 0,
				    MODIFIER_SUPER, home_key_binding, shell);
	wlsc_compositor_add_binding(compositor, KEY_RIGHTMETA, 0,
				    MODIFIER_SUPER, home_key_binding, shell);
 	wlsc_compositor_add_binding(compositor, KEY_COMPOSE, 0, 0,
				    menu_key_binding, shell);

	compositor->shell = &shell->shell;

	shell->shell.lock = tablet_shell_lock;
	shell->shell.unlock = tablet_shell_unlock;
	shell->shell.map = tablet_shell_map;
	shell->shell.configure = tablet_shell_configure;
	shell->shell.set_selection_focus =
		tablet_shell_set_selection_focus;
	launch_ux_daemon(shell);

	tablet_shell_set_state(shell, STATE_STARTING);
}
