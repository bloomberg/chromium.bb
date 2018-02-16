// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/main/main_coordinator.h"
#import "ios/chrome/browser/ui/main/view_controller_swapping.h"

@protocol TabSwitcher;

@interface TabGridCoordinator : MainCoordinator<ViewControllerSwapping>

@property(nonatomic, readonly) id<TabSwitcher> tabSwitcher;

// If this property is YES, calls to |showTabSwitcher:completion:| and
// |showTabViewController:completion:| will present the given view controllers
// without animation.  This should only be used by unittests.
@property(nonatomic, readwrite, assign) BOOL animationsDisabledForTesting;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
