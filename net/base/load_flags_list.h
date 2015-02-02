// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the list of load flags and their values. For the enum values,
// include the file "net/base/load_flags.h".
//
// Here we define the values using a macro LOAD_FLAG, so it can be
// expanded differently in some places (for example, to automatically
// map a load flag value to its symbolic name).

LOAD_FLAG(NORMAL, 0)

// This is "normal reload", meaning an if-none-match/if-modified-since query
LOAD_FLAG(VALIDATE_CACHE, 1 << 0)

// This is "shift-reload", meaning a "pragma: no-cache" end-to-end fetch
LOAD_FLAG(BYPASS_CACHE, 1 << 1)

// This is a back/forward style navigation where the cached content should
// be preferred over any protocol specific cache validation.
LOAD_FLAG(PREFERRING_CACHE, 1 << 2)

// This is a navigation that will fail if it cannot serve the requested
// resource from the cache (or some equivalent local store).
LOAD_FLAG(ONLY_FROM_CACHE, 1 << 3)

// Indicate that if the request fails at the network level in a way that
// indicates the source is unreachable, the request should fail over
// to as if LOAD_PREFERRING_CACHE had been set.
LOAD_FLAG(FROM_CACHE_IF_OFFLINE, 1 << 4)

// This is a navigation that will not use the cache at all.  It does not
// impact the HTTP request headers.
LOAD_FLAG(DISABLE_CACHE, 1 << 5)

// If present, causes certificate revocation checks to be skipped on secure
// connections.
LOAD_FLAG(DISABLE_CERT_REVOCATION_CHECKING, 1 << 6)

// This load will not make any changes to cookies, including storing new
// cookies or updating existing ones.
LOAD_FLAG(DO_NOT_SAVE_COOKIES, 1 << 7)

// Do not resolve proxies. This override is used when downloading PAC files
// to avoid having a circular dependency.
LOAD_FLAG(BYPASS_PROXY, 1 << 8)

// Indicate this request is for a download, as opposed to viewing.
LOAD_FLAG(IS_DOWNLOAD, 1 << 9)

// Requires EV certificate verification.
LOAD_FLAG(VERIFY_EV_CERT, 1 << 10)

// This load will not send any cookies.
LOAD_FLAG(DO_NOT_SEND_COOKIES, 1 << 11)

// This load will not send authentication data (user name/password)
// to the server (as opposed to the proxy).
LOAD_FLAG(DO_NOT_SEND_AUTH_DATA, 1 << 12)

// This should only be used for testing (set by HttpNetworkTransaction).
LOAD_FLAG(IGNORE_ALL_CERT_ERRORS, 1 << 13)

// Indicate that this is a top level frame, so that we don't assume it is a
// subresource and speculatively pre-connect or pre-resolve when a referring
// page is loaded.
LOAD_FLAG(MAIN_FRAME, 1 << 14)

// Indicate that this is a sub frame, and hence it might have subresources that
// should be speculatively resolved, or even speculatively preconnected.
LOAD_FLAG(SUB_FRAME, 1 << 15)

// If present, intercept actual request/response headers from network stack
// and report them to renderer. This includes cookies, so the flag is only
// respected if renderer has CanReadRawCookies capability in the security
// policy.
LOAD_FLAG(REPORT_RAW_HEADERS, 1 << 16)

// Indicates that this load was motivated by the rel=prefetch feature,
// and is (in theory) not intended for the current frame.
LOAD_FLAG(PREFETCH, 1 << 17)

// Indicates that this is a load that ignores limits and should complete
// immediately.
LOAD_FLAG(IGNORE_LIMITS, 1 << 18)

// Indicates that the operation is somewhat likely to be due to an
// explicit user action. This can be used as a hint to treat the
// request with higher priority.
LOAD_FLAG(MAYBE_USER_GESTURE, 1 << 19)

// Indicates that the username:password portion of the URL should not
// be honored, but that other forms of authority may be used.
LOAD_FLAG(DO_NOT_USE_EMBEDDED_IDENTITY, 1 << 20)

// Send request directly to the origin if the effective proxy is the data
// reduction proxy.
// TODO(rcs): Remove this flag as soon as http://crbug.com/339237 is resolved.
LOAD_FLAG(BYPASS_DATA_REDUCTION_PROXY, 1 << 21)

// Indicates the the request is an asynchronous revalidation.
LOAD_FLAG(ASYNC_REVALIDATION, 1 << 22)
