// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_COORDINATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"

namespace web {
class WebState;
}

// Coordinator that runs a tab: A composed UI consisting of a web view plus
// additional UI components such as a toolbar.
@interface TabCoordinator : BrowserCoordinator

// The WebState representing the web page that will be displayed in this tab.
// Calling code should assign this before starting this coordinator.
@property(nonatomic, assign) web::WebState* webState;

// An opaque key provided by this coordinator's parent which can be passed in
// to a transition animation to synchronize the presentation with the presenting
// view controller, if any.
@property(nonatomic, copy) NSObject* presentationKey;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_COORDINATOR_H_
