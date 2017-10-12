/*
 * Copyright © 2017 Pekka Paalanen <pq@iki.fi>
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

#ifndef WESTON_DEBUG_H
#define WESTON_DEBUG_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct weston_compositor;

void
weston_compositor_enable_debug_protocol(struct weston_compositor *);

struct weston_debug_scope;
struct weston_debug_stream;

/** weston_debug_scope callback
 *
 * \param stream The debug stream.
 * \param user_data The \c user_data argument given to
 * weston_compositor_add_debug_scope()
 *
 * \memberof weston_debug_scope
 * \sa weston_debug_stream
 */
typedef void (*weston_debug_scope_cb)(struct weston_debug_stream *stream,
				      void *user_data);

struct weston_debug_scope *
weston_compositor_add_debug_scope(struct weston_compositor *compositor,
				  const char *name,
				  const char *description,
				  weston_debug_scope_cb begin_cb,
				  void *user_data);

void
weston_debug_scope_destroy(struct weston_debug_scope *scope);

bool
weston_debug_scope_is_enabled(struct weston_debug_scope *scope);

void
weston_debug_scope_write(struct weston_debug_scope *scope,
			 const char *data, size_t len);

void
weston_debug_scope_vprintf(struct weston_debug_scope *scope,
			   const char *fmt, va_list ap);

void
weston_debug_scope_printf(struct weston_debug_scope *scope,
			  const char *fmt, ...)
			  __attribute__ ((format (printf, 2, 3)));

void
weston_debug_stream_write(struct weston_debug_stream *stream,
			  const char *data, size_t len);

void
weston_debug_stream_vprintf(struct weston_debug_stream *stream,
			    const char *fmt, va_list ap);

void
weston_debug_stream_printf(struct weston_debug_stream *stream,
			   const char *fmt, ...)
			   __attribute__ ((format (printf, 2, 3)));

void
weston_debug_stream_complete(struct weston_debug_stream *stream);

char *
weston_debug_scope_timestamp(struct weston_debug_scope *scope,
			     char *buf, size_t len);

#ifdef  __cplusplus
}
#endif

#endif /* WESTON_DEBUG_H */
