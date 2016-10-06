// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_
#define CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_

#include <string>

#include "content/common/content_export.h"

class GURL;

namespace content {

// Returns true if the origin is trustworthy: that is, if its contents can be
// said to have been transferred to the browser in a way that a network attacker
// cannot tamper with or observe.
//
// See https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
bool CONTENT_EXPORT IsOriginSecure(const GURL& url);

// Returns true if the origin can register a service worker.  Scheme must be
// http (localhost only), https, or a custom-set secure scheme.
bool CONTENT_EXPORT OriginCanAccessServiceWorkers(const GURL& url);

// Resets the internal schemes/origins whitelist. Used only for testing.
void CONTENT_EXPORT ResetSchemesAndOriginsWhitelistForTesting();

// Utility functions for extracting Suborigin information from a URL that a
// Suborigin has been serialized into. See
// https://w3c.github.io/webappsec-suborigins/ for more information on
// Suborigins.
//
// Returns true if the url has a serialized Suborigin in the host. Otherwise,
// returns false.
bool CONTENT_EXPORT HasSuborigin(const GURL& url);

// If the host in the URL has a Suborigin serialized into it, returns the name.
// Otherwise, returns the empty string.
std::string CONTENT_EXPORT SuboriginFromUrl(const GURL& url);

// If the URL has a suborigin serialized into it, returns the same URL with the
// suborigin stripped. Otherwise, returns the original URL unchanged.
//
// For example, if given the URL https-so://foobar.example.com, the returned URL
// would be https://example.com. If given the URL https://foobar.com (no
// suborigin), the returned URL would be https://foobar.com.
GURL CONTENT_EXPORT StripSuboriginFromUrl(const GURL& url);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_
