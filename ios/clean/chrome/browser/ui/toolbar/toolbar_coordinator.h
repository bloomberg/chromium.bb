// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"

namespace web {
class WebState;
}

// Coordinator to run a toolbar -- a UI element housing controls.
@interface CleanToolbarCoordinator : BrowserCoordinator
// The web state this CleanToolbarCoordinator is handling.
@property(nonatomic, assign) web::WebState* webState;

// By default, this component does not interact with the tab strip. Setting
// |usesTabStrip| to YES, allows interaction with the tab strip.
@property(nonatomic, assign) BOOL usesTabStrip;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
