/*
 * Copyright © 2017 Pekka Paalanen <pq@iki.fi>
 * Copyright © 2018 Zodiac Inflight Innovations
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

#include "weston-debug.h"
#include "helpers.h"
#include "compositor.h"

#include "weston-debug-server-protocol.h"

#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

/** Main weston-debug context
 *
 * One per weston_compositor.
 *
 * \internal
 */
struct weston_debug_compositor {
	struct weston_compositor *compositor;
	struct wl_listener compositor_destroy_listener;
	struct wl_global *global;
	struct wl_list scope_list; /**< weston_debug_scope::compositor_link */
};

/** weston-debug message scope
 *
 * This is used for scoping debugging messages. Clients can subscribe to
 * only the scopes they are interested in. A scope is identified by its name
 * (also referred to as debug stream name).
 */
struct weston_debug_scope {
	char *name;
	char *desc;
	weston_debug_scope_cb begin_cb;
	void *user_data;
	struct wl_list stream_list; /**< weston_debug_stream::scope_link */
	struct wl_list compositor_link;
};

/** A debug stream created by a client
 *
 * A client provides a file descriptor for the server to write debug
 * messages into. A weston_debug_stream is associated to one
 * weston_debug_scope via the scope name, and the scope provides the messages.
 * There can be several streams for the same scope, all streams getting the
 * same messages.
 */
struct weston_debug_stream {
	int fd;				/**< client provided fd */
	struct wl_resource *resource;	/**< weston_debug_stream_v1 object */
	struct wl_list scope_link;
};

static struct weston_debug_scope *
get_scope(struct weston_debug_compositor *wdc, const char *name)
{
	struct weston_debug_scope *scope;

	wl_list_for_each(scope, &wdc->scope_list, compositor_link)
		if (strcmp(name, scope->name) == 0)
			return scope;

	return NULL;
}

static void
stream_close_unlink(struct weston_debug_stream *stream)
{
	if (stream->fd != -1)
		close(stream->fd);
	stream->fd = -1;

	wl_list_remove(&stream->scope_link);
	wl_list_init(&stream->scope_link);
}

static void WL_PRINTF(2, 3)
stream_close_on_failure(struct weston_debug_stream *stream,
			const char *fmt, ...)
{
	char *msg;
	va_list ap;
	int ret;

	stream_close_unlink(stream);

	va_start(ap, fmt);
	ret = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (ret > 0) {
		weston_debug_stream_v1_send_failure(stream->resource, msg);
		free(msg);
	} else {
		weston_debug_stream_v1_send_failure(stream->resource,
						    "MEMFAIL");
	}
}

static struct weston_debug_stream *
stream_create(struct weston_debug_compositor *wdc, const char *name,
	      int32_t streamfd, struct wl_resource *stream_resource)
{
	struct weston_debug_stream *stream;
	struct weston_debug_scope *scope;

	stream = zalloc(sizeof *stream);
	if (!stream)
		return NULL;

	stream->fd = streamfd;
	stream->resource = stream_resource;

	scope = get_scope(wdc, name);
	if (scope) {
		wl_list_insert(&scope->stream_list, &stream->scope_link);

		if (scope->begin_cb)
			scope->begin_cb(stream, scope->user_data);
	} else {
		wl_list_init(&stream->scope_link);
		stream_close_on_failure(stream,
					"Debug stream name '%s' is unknown.",
					name);
	}

	return stream;
}

static void
stream_destroy(struct wl_resource *stream_resource)
{
	struct weston_debug_stream *stream;

	stream = wl_resource_get_user_data(stream_resource);

	if (stream->fd != -1)
		close(stream->fd);
	wl_list_remove(&stream->scope_link);
	free(stream);
}

static void
weston_debug_stream_destroy(struct wl_client *client,
			    struct wl_resource *stream_resource)
{
	wl_resource_destroy(stream_resource);
}

static const struct weston_debug_stream_v1_interface
						weston_debug_stream_impl = {
	weston_debug_stream_destroy
};

static void
weston_debug_destroy(struct wl_client *client,
		     struct wl_resource *global_resource)
{
	wl_resource_destroy(global_resource);
}

static void
weston_debug_subscribe(struct wl_client *client,
		       struct wl_resource *global_resource,
		       const char *name,
		       int32_t streamfd,
		       uint32_t new_stream_id)
{
	struct weston_debug_compositor *wdc;
	struct wl_resource *stream_resource;
	uint32_t version;
	struct weston_debug_stream *stream;

	wdc = wl_resource_get_user_data(global_resource);
	version = wl_resource_get_version(global_resource);

	stream_resource = wl_resource_create(client,
					&weston_debug_stream_v1_interface,
					version, new_stream_id);
	if (!stream_resource)
		goto fail;

	stream = stream_create(wdc, name, streamfd, stream_resource);
	if (!stream)
		goto fail;

	wl_resource_set_implementation(stream_resource,
				       &weston_debug_stream_impl,
				       stream, stream_destroy);
	return;

fail:
	close(streamfd);
	wl_client_post_no_memory(client);
}

static const struct weston_debug_v1_interface weston_debug_impl = {
	weston_debug_destroy,
	weston_debug_subscribe
};

static void
bind_weston_debug(struct wl_client *client,
		   void *data, uint32_t version, uint32_t id)
{
	struct weston_debug_compositor *wdc = data;
	struct weston_debug_scope *scope;
	struct wl_resource *resource;

	resource = wl_resource_create(client,
				      &weston_debug_v1_interface,
				      version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &weston_debug_impl,
				       wdc, NULL);

       wl_list_for_each(scope, &wdc->scope_list, compositor_link) {
		weston_debug_v1_send_available(resource, scope->name,
					       scope->desc);
       }
}

/** Initialize weston-debug structure
 *
 * \param compositor The libweston compositor.
 * \return 0 on success, -1 on failure.
 *
 * weston_debug_compositor is a singleton for each weston_compositor.
 *
 * Sets weston_compositor::weston_debug.
 *
 * \internal
 */
int
weston_debug_compositor_create(struct weston_compositor *compositor)
{
	struct weston_debug_compositor *wdc;

	if (compositor->weston_debug)
		return -1;

	wdc = zalloc(sizeof *wdc);
	if (!wdc)
		return -1;

	wdc->compositor = compositor;
	wl_list_init(&wdc->scope_list);

	compositor->weston_debug = wdc;

	return 0;
}

/** Destroy weston_debug_compositor structure
 *
 * \param compositor The libweston compositor whose weston-debug to tear down.
 *
 * Clears weston_compositor::weston_debug.
 *
 * \internal
 */
void
weston_debug_compositor_destroy(struct weston_compositor *compositor)
{
	struct weston_debug_compositor *wdc = compositor->weston_debug;
	struct weston_debug_scope *scope;

	if (wdc->global)
		wl_global_destroy(wdc->global);

	wl_list_for_each(scope, &wdc->scope_list, compositor_link)
		weston_log("Internal warning: debug scope '%s' has not been destroyed.\n",
			   scope->name);

	/* Remove head to not crash if scope removed later. */
	wl_list_remove(&wdc->scope_list);

	free(wdc);

	compositor->weston_debug = NULL;
}

/** Enable weston-debug protocol extension
 *
 * \param compositor The libweston compositor where to enable.
 *
 * This enables the weston_debug_v1 Wayland protocol extension which any client
 * can use to get debug messsages from the compositor.
 *
 * WARNING: This feature should not be used in production. If a client
 * provides a file descriptor that blocks writes, it will block the whole
 * compositor indefinitely.
 *
 * There is no control on which client is allowed to subscribe to debug
 * messages. Any and all clients are allowed.
 *
 * The debug extension is disabled by default, and once enabled, cannot be
 * disabled again.
 */
WL_EXPORT void
weston_compositor_enable_debug_protocol(struct weston_compositor *compositor)
{
	struct weston_debug_compositor *wdc = compositor->weston_debug;

	assert(wdc);
	if (wdc->global)
		return;

	wdc->global = wl_global_create(compositor->wl_display,
				       &weston_debug_v1_interface, 1,
				       wdc, bind_weston_debug);
	if (!wdc->global)
		return;

	weston_log("WARNING: debug protocol has been enabled. "
		   "This is a potential denial-of-service attack vector and "
		   "information leak.\n");
}

/** Register a new debug stream name, creating a debug scope
 *
 * \param compositor The libweston compositor where to add.
 * \param name The debug stream/scope name; must not be NULL.
 * \param desc The debug scope description for humans; must not be NULL.
 * \param begin_cb Optional callback when a client subscribes to this scope.
 * \param user_data Optional user data pointer for the callback.
 * \return A valid pointer on success, NULL on failure.
 *
 * This function is used to create a debug scope. All debug message printing
 * happens for a scope, which allows clients to subscribe to the kind of
 * debug messages they want by \c name.
 *
 * \c name must be unique in the \c weston_compositor instance. \c name and
 * \c description must both be provided. The description is printed when a
 * client asks for a list of supported debug scopes.
 *
 * \c begin_cb, if not NULL, is called when a client subscribes to the
 * debug scope creating a debug stream. This is for debug scopes that need
 * to print messages as a response to a client appearing, e.g. printing a
 * list of windows on demand or a static preamble. The argument \c user_data
 * is passed in to the callback and is otherwise unused.
 *
 * For one-shot debug streams, \c begin_cb should finally call
 * weston_debug_stream_complete() to close the stream and tell the client
 * the printing is complete. Otherwise the client expects more to be written
 * to its file descriptor.
 *
 * The debug scope must be destroyed before destroying the
 * \c weston_compositor.
 *
 * \memberof weston_debug_scope
 * \sa weston_debug_stream, weston_debug_scope_cb
 */
WL_EXPORT struct weston_debug_scope *
weston_compositor_add_debug_scope(struct weston_compositor *compositor,
				  const char *name,
				  const char *description,
				  weston_debug_scope_cb begin_cb,
				  void *user_data)
{
	struct weston_debug_compositor *wdc;
	struct weston_debug_scope *scope;

	if (!compositor || !name || !description) {
		weston_log("Error: cannot add a debug scope without name or description.\n");
		return NULL;
	}

	wdc = compositor->weston_debug;
	if (!wdc) {
		weston_log("Error: cannot add debug scope '%s', infra not initialized.\n",
			   name);
		return NULL;
	}

	if (get_scope(wdc, name)){
		weston_log("Error: debug scope named '%s' is already registered.\n",
			   name);
		return NULL;
	}

	scope = zalloc(sizeof *scope);
	if (!scope) {
		weston_log("Error adding debug scope '%s': out of memory.\n",
			   name);
		return NULL;
	}

	scope->name = strdup(name);
	scope->desc = strdup(description);
	scope->begin_cb = begin_cb;
	scope->user_data = user_data;
	wl_list_init(&scope->stream_list);

	if (!scope->name || !scope->desc) {
		weston_log("Error adding debug scope '%s': out of memory.\n",
			   name);
		free(scope->name);
		free(scope->desc);
		free(scope);
		return NULL;
	}

	wl_list_insert(wdc->scope_list.prev, &scope->compositor_link);

	return scope;
}

/** Destroy a debug scope
 *
 * \param scope The debug scope to destroy; may be NULL.
 *
 * Destroys the debug scope, closing all open streams subscribed to it and
 * sending them each a \c weston_debug_stream_v1.failure event.
 *
 * \memberof weston_debug_scope
 */
WL_EXPORT void
weston_debug_scope_destroy(struct weston_debug_scope *scope)
{
	struct weston_debug_stream *stream;

	if (!scope)
		return;

	while (!wl_list_empty(&scope->stream_list)) {
		stream = wl_container_of(scope->stream_list.prev,
					 stream, scope_link);

		stream_close_on_failure(stream, "debug name removed");
	}

	wl_list_remove(&scope->compositor_link);
	free(scope->name);
	free(scope->desc);
	free(scope);
}

/** Are there any active subscriptions to the scope?
 *
 * \param scope The debug scope to check; may be NULL.
 * \return True if any streams are open for this scope, false otherwise.
 *
 * As printing some debugging messages may be relatively expensive, one
 * can use this function to determine if there is a need to gather the
 * debugging information at all. If this function returns false, all
 * printing for this scope is dropped, so gathering the information is
 * pointless.
 *
 * The return value of this function should not be stored, as new clients
 * may subscribe to the debug scope later.
 *
 * If the given scope is NULL, this function will always return false,
 * making it safe to use in teardown or destroy code, provided the
 * scope is initialized to NULL before creation and set to NULL after
 * destruction.
 *
 * \memberof weston_debug_scope
 */
WL_EXPORT bool
weston_debug_scope_is_enabled(struct weston_debug_scope *scope)
{
	if (!scope)
		return false;

	return !wl_list_empty(&scope->stream_list);
}

/** Write data into a specific debug stream
 *
 * \param stream The debug stream to write into; must not be NULL.
 * \param data[in] Pointer to the data to write.
 * \param len Number of bytes to write.
 *
 * Writes the given data (binary verbatim) into the debug stream.
 * If \c len is zero or negative, the write is silently dropped.
 *
 * Writing is continued until all data has been written or
 * a write fails. If the write fails due to a signal, it is re-tried.
 * Otherwise on failure, the stream is closed and
 * \c weston_debug_stream_v1.failure event is sent to the client.
 *
 * \memberof weston_debug_stream
 */
WL_EXPORT void
weston_debug_stream_write(struct weston_debug_stream *stream,
			  const char *data, size_t len)
{
	ssize_t len_ = len;
	ssize_t ret;
	int e;

	if (stream->fd == -1)
		return;

	while (len_ > 0) {
		ret = write(stream->fd, data, len_);
		e = errno;
		if (ret < 0) {
			if (e == EINTR)
				continue;

			stream_close_on_failure(stream,
					"Error writing %zd bytes: %s (%d)",
					len_, strerror(e), e);
			break;
		}

		len_ -= ret;
		data += ret;
	}
}

/** Write a formatted string into a specific debug stream (varargs)
 *
 * \param stream The debug stream to write into.
 * \param fmt Printf-style format string.
 * \param ap Formatting arguments.
 *
 * The behavioral details are the same as for weston_debug_stream_write().
 *
 * \memberof weston_debug_stream
 */
WL_EXPORT void
weston_debug_stream_vprintf(struct weston_debug_stream *stream,
			    const char *fmt, va_list ap)
{
	char *str;
	int len;

	len = vasprintf(&str, fmt, ap);
	if (len >= 0) {
		weston_debug_stream_write(stream, str, len);
		free(str);
	} else {
		stream_close_on_failure(stream, "Out of memory");
	}
}

/** Write a formatted string into a specific debug stream
 *
 * \param stream The debug stream to write into.
 * \param fmt Printf-style format string and arguments.
 *
 * The behavioral details are the same as for weston_debug_stream_write().
 *
 * \memberof weston_debug_stream
 */
WL_EXPORT void
weston_debug_stream_printf(struct weston_debug_stream *stream,
			   const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	weston_debug_stream_vprintf(stream, fmt, ap);
	va_end(ap);
}

/** Close the debug stream and send success event
 *
 * \param stream The debug stream to close.
 *
 * Closes the debug stream and sends \c weston_debug_stream_v1.complete
 * event to the client. This tells the client the debug information dump
 * is complete.
 *
 * \memberof weston_debug_stream
 */
WL_EXPORT void
weston_debug_stream_complete(struct weston_debug_stream *stream)
{
	stream_close_unlink(stream);
	weston_debug_stream_v1_send_complete(stream->resource);
}

/** Write debug data for a scope
 *
 * \param scope The debug scope to write for; may be NULL, in which case
 *              nothing will be written.
 * \param data[in] Pointer to the data to write.
 * \param len Number of bytes to write.
 *
 * Writes the given data to all subscribed clients' streams.
 *
 * The behavioral details for each stream are the same as for
 * weston_debug_stream_write().
 *
 * \memberof weston_debug_scope
 */
WL_EXPORT void
weston_debug_scope_write(struct weston_debug_scope *scope,
			 const char *data, size_t len)
{
	struct weston_debug_stream *stream;

	if (!scope)
		return;

	wl_list_for_each(stream, &scope->stream_list, scope_link)
		weston_debug_stream_write(stream, data, len);
}

/** Write a formatted string for a scope (varargs)
 *
 * \param scope The debug scope to write for; may be NULL, in which case
 *              nothing will be written.
 * \param fmt Printf-style format string.
 * \param ap Formatting arguments.
 *
 * Writes to formatted string to all subscribed clients' streams.
 *
 * The behavioral details for each stream are the same as for
 * weston_debug_stream_write().
 *
 * \memberof weston_debug_scope
 */
WL_EXPORT void
weston_debug_scope_vprintf(struct weston_debug_scope *scope,
			   const char *fmt, va_list ap)
{
	static const char oom[] = "Out of memory";
	char *str;
	int len;

	if (!weston_debug_scope_is_enabled(scope))
		return;

	len = vasprintf(&str, fmt, ap);
	if (len >= 0) {
		weston_debug_scope_write(scope, str, len);
		free(str);
	} else {
		weston_debug_scope_write(scope, oom, sizeof oom - 1);
	}
}

/** Write a formatted string for a scope
 *
 * \param scope The debug scope to write for; may be NULL, in which case
 *              nothing will be written.
 * \param fmt Printf-style format string and arguments.
 *
 * Writes to formatted string to all subscribed clients' streams.
 *
 * The behavioral details for each stream are the same as for
 * weston_debug_stream_write().
 *
 * \memberof weston_debug_scope
 */
WL_EXPORT void
weston_debug_scope_printf(struct weston_debug_scope *scope,
			  const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	weston_debug_scope_vprintf(scope, fmt, ap);
	va_end(ap);
}

/** Write debug scope name and current time into string
 *
 * \param scope[in] debug scope; may be NULL
 * \param buf[out] Buffer to store the string.
 * \param len Available size in the buffer in bytes.
 * \return \c buf
 *
 * Reads the current local wall-clock time and formats it into a string.
 * and append the debug scope name to it, if a scope is available.
 * The string is NUL-terminated, even if truncated.
 */
WL_EXPORT char *
weston_debug_scope_timestamp(struct weston_debug_scope *scope,
			     char *buf, size_t len)
{
	struct timeval tv;
	struct tm *bdt;
	char string[128];
	size_t ret = 0;

	gettimeofday(&tv, NULL);

	bdt = localtime(&tv.tv_sec);
	if (bdt)
		ret = strftime(string, sizeof string,
			       "%Y-%m-%d %H:%M:%S", bdt);

	if (ret > 0) {
		snprintf(buf, len, "[%s.%03ld][%s]", string,
			 tv.tv_usec / 1000,
			 (scope) ? scope->name : "no scope");
	} else {
		snprintf(buf, len, "[?][%s]",
			 (scope) ? scope->name : "no scope");
	}

	return buf;
}
