// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_PRIVATE_CONSTRUCTORS_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_PRIVATE_CONSTRUCTORS_H_

#import "ios/web/navigation/crw_session_controller.h"

#include "base/memory/scoped_vector.h"

namespace web {
class BrowserState;
class NavigationItem;
}

// Temporary interface for NavigationManager and tests to create
// CRWSessionControllers. Once CRWSessionController has no users outside of
// web/, these methods can go back into session_controller.h. crbug.com/318974
@interface CRWSessionController (PrivateConstructors)
// Initializes a session controller, supplying a unique textual identifier for
// the window, or nil. |opener| is the tab id of the parent tab. It may be
// nil or empty if there is no parent.
- (id)initWithWindowName:(NSString*)windowName
                openerId:(NSString*)opener
             openedByDOM:(BOOL)openedByDOM
   openerNavigationIndex:(NSInteger)openerIndex
            browserState:(web::BrowserState*)browserState;

// Initializes a session controller, supplying a list of NavigationItem objects
// and the current index in the navigation history.
- (id)initWithNavigationItems:(ScopedVector<web::NavigationItem>)scoped_items
                 currentIndex:(NSUInteger)currentIndex
                 browserState:(web::BrowserState*)browserState;
@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_PRIVATE_CONSTRUCTORS_H_
