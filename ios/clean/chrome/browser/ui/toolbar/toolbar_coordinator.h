// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_

#import "ios/clean/chrome/browser/browser_coordinator.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

// Coordinator to run a toolbar -- a UI element housing controls.
@interface ToolbarCoordinator : BrowserCoordinator<CRWWebStateObserver>
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
