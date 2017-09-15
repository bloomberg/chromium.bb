// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"
#import "ios/clean/chrome/browser/url_opening.h"

@class TabCoordinator;

// Coordinator that drives a UI showing a scrollable grid of tabs,
// which each represent a web browsing tab that can be expanded by tapping.
@interface TabGridCoordinator : BrowserCoordinator<URLOpening>

// The TabCoordinator for the active Tab.
@property(nonatomic, weak, readonly) TabCoordinator* activeTabCoordinator;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
