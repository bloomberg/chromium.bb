// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_NAVIGATION_THROTTLE_UTIL_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_NAVIGATION_THROTTLE_UTIL_H_

#include "content/public/browser/navigation_throttle.h"

namespace safe_browsing {
class SafeBrowsingService;
}

// Creates the CreateSubresourceFilterNavigationThrottle for the main frame and
// the |safe_browsing_service| supporting pver4, |safe_browsing_service| can be
// a nullptr.
content::NavigationThrottle* MaybeCreateSubresourceFilterNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    safe_browsing::SafeBrowsingService* safe_browsing_service);

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_NAVIGATION_THROTTLE_UTIL_H_
