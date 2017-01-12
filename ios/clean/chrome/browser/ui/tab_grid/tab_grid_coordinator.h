// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/browser_coordinator.h"
#import "ios/clean/chrome/browser/url_opening.h"

// Coordinator that drives a UI showing a scrollable grid of tabs,
// which each represent a web browsing tab that can be expanded by tapping.
@interface TabGridCoordinator : BrowserCoordinator<URLOpening>
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
