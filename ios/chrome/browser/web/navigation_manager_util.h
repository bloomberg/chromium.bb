// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_NAVIGATION_MANAGER_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_NAVIGATION_MANAGER_UTIL_H_

#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"

// Utility functions built on web::NavigationManager public API.

// Returns the most recent Committed Item that is not the result of a client or
// server-side redirect from the given Navigation Manager. Returns nullptr if
// there's an error condition on the input |nav_manager|, such as nullptr or no
// non-redirect items.
web::NavigationItem* GetLastCommittedNonRedirectedItem(
    web::NavigationManager* nav_manager);

#endif  // IOS_CHROME_BROWSER_WEB_NAVIGATION_MANAGER_UTIL_H_
