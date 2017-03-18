// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_NAVIGATION_MANAGER_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_NAVIGATION_MANAGER_UTIL_H_

#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"

// Utility functions built on web::NavigationManager public API.

// Returns the most recent navigation item that is not the result of an HTTP
// Redirect. Returns nullptr if |nav_manager| is nullptr or is empty.
// If all items are redirects, the first item is returned.
web::NavigationItem* GetLastNonRedirectedItem(
    web::NavigationManager* nav_manager);

#endif  // IOS_CHROME_BROWSER_WEB_NAVIGATION_MANAGER_UTIL_H_
