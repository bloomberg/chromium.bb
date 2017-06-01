// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_COORDINATOR_H_

#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator.h"

@class ToolsMenuConfiguration;
namespace web {
class WebState;
}

// Coordinator that shows an inteface for the user to select a
// tool or action to use.
@interface ToolsCoordinator : BrowserCoordinator
// The current Toolbar configuration for this coordinator.
@property(nonatomic, strong, nonnull)
    ToolsMenuConfiguration* toolsMenuConfiguration;
// The web state this ToolbarCoordinator is handling.
@property(nonatomic, assign, nullable) web::WebState* webState;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_COORDINATOR_H_
