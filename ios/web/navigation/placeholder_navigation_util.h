// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_PLACEHOLDER_NAVIGATION_UTIL_H_
#define IOS_WEB_NAVIGATION_PLACEHOLDER_NAVIGATION_UTIL_H_

#include "url/gurl.h"

// Utility functions for creating and managing URLs for placeholder navigations.
// A placeholder navigation is an "about:blank" page loaded into the WKWebView
// that corresponds to Native View or WebUI URL. This navigation is inserted to
// generate a WKBackForwardListItem for the Native View or WebUI URL in the
// WebView so that the WKBackForwardList contains the full list of user-visible
// navigations.
// See "Handling App-specific URLs" section in go/bling-navigation-experiment
// for more details.

namespace web {
namespace placeholder_navigation_util {

// Returns true if |URL| is a placeholder navigation URL.
bool IsPlaceholderUrl(const GURL& url);

// Creates the URL for the placeholder navigation required for Native View and
// WebUI URLs.
GURL CreatePlaceholderUrlForUrl(const GURL& original_url);

// Extracts the original URL from the placeholder URL.
GURL ExtractUrlFromPlaceholderUrl(const GURL& url);

}  // namespace placeholder_navigation_util
}  // namespace web

#endif  // IOS_WEB_NAVIGATION_PLACEHOLDER_NAVIGATION_UTIL_H_
