// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_

#import "ios/clean/chrome/browser/browser_coordinator.h"

namespace web {
class WebState;
}

// Coordinator to run a toolbar -- a UI element housing controls.
@interface ToolbarCoordinator : BrowserCoordinator
// The web state this ToolbarCoordinator is handling.
@property(nonatomic, assign) web::WebState* webState;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
