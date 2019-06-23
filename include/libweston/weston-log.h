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

#ifndef WESTON_LOG_H
#define WESTON_LOG_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct weston_compositor;
struct weston_log_context;
struct wl_display;
struct weston_log_subscriber;
struct weston_log_subscription;

void
weston_compositor_enable_debug_protocol(struct weston_compositor *);

bool
weston_compositor_is_debug_protocol_enabled(struct weston_compositor *);

struct weston_log_scope;
struct weston_debug_stream;

/** weston_log_scope callback
 *
 * @param scope The log scope.
 * @param user_data The \c user_data argument given to
 * weston_compositor_add_log_scope()
 *
 * @memberof weston_log_scope
 */
typedef void (*weston_log_scope_cb)(struct weston_log_subscription *sub,
				    void *user_data);

struct weston_log_scope *
weston_compositor_add_log_scope(struct weston_log_context *compositor,
				  const char *name,
				  const char *description,
				  weston_log_scope_cb new_subscriber,
				  void *user_data);

void
weston_compositor_log_scope_destroy(struct weston_log_scope *scope);

bool
weston_log_scope_is_enabled(struct weston_log_scope *scope);

void
weston_log_scope_write(struct weston_log_scope *scope,
			 const char *data, size_t len);

void
weston_log_scope_vprintf(struct weston_log_scope *scope,
			   const char *fmt, va_list ap);

void
weston_log_scope_printf(struct weston_log_scope *scope,
			  const char *fmt, ...)
			  __attribute__ ((format (printf, 2, 3)));
void
weston_log_subscription_printf(struct weston_log_subscription *scope,
				const char *fmt, ...)
			  __attribute__ ((format (printf, 2, 3)));
void
weston_log_scope_complete(struct weston_log_scope *scope);

void
weston_log_subscription_complete(struct weston_log_subscription *sub);

char *
weston_log_scope_timestamp(struct weston_log_scope *scope,
			     char *buf, size_t len);

void
weston_log_subscribe(struct weston_log_context *log_ctx,
		     struct weston_log_subscriber *subscriber,
		     const char *scope_name);

struct weston_log_subscriber *
weston_log_subscriber_create_log(FILE *dump_to);

void
weston_log_subscriber_destroy_log(struct weston_log_subscriber *sub);

struct weston_log_subscriber *
weston_log_subscriber_create_flight_rec(size_t size);

void
weston_log_subscriber_destroy_flight_rec(struct weston_log_subscriber *sub);

void
weston_log_subscriber_display_flight_rec(struct weston_log_subscriber *sub);

#ifdef  __cplusplus
}
#endif

#endif /* WESTON_LOG_H */
