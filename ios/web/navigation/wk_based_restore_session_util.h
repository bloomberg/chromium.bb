// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains utility functions for WKBasedNavigationManagerImpl.

#ifndef IOS_WEB_NAVIGATION_WK_BASED_RESTORE_SESSION_UTIL_H_
#define IOS_WEB_NAVIGATION_WK_BASED_RESTORE_SESSION_UTIL_H_

#include <memory>
#include <vector>

#include "url/gurl.h"

namespace web {

class NavigationItem;

// Query parameter key used to encode the session history to inject in a
// restore_session.html URL.
extern const char kRestoreSessionSessionQueryKey[];

// Query parameter key used to encode target URL in a restore_session.html URL.
extern const char kRestoreSessionTargetUrlQueryKey[];

// Returns a file:// URL that points to the magic restore_session.html file.
// This is used in unit tests.
GURL GetRestoreSessionBaseUrl();

// Creates a restore_session.html URL with the provided session history encoded
// in the query argument, such that when this URL is loaded in the web view,
// recreates all the history entries in |items| and the current loaded item is
// the entry at |last_committed_item_index|.
GURL CreateRestoreSessionUrl(
    int last_committed_item_index,
    const std::vector<std::unique_ptr<NavigationItem>>& items);

// Returns true if the base URL of |url| is restore_session.html.
bool IsRestoreSessionUrl(const GURL& url);

// Extracts the URL encoded in the 'targetUrl' query component of
// |restore_session_url| to |target_url| and returns true. If no such query
// component exists, returns false.
bool ExtractTargetURL(const GURL& restore_session_url, GURL* target_url);

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_BASED_RESTORE_SESSION_UTIL_H_
