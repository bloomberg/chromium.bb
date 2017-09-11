// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/transitions/animators/zoom_transition_delegate.h"

@protocol TabGridToolbarCommands;

// Container for the tab grid. This container contains the toolbar of the tab
// grid. It also displays the tab grid associated with the current mode
// (incognito/normal).
@interface TabGridContainerViewController
    : UIViewController<ZoomTransitionDelegate>

@property(nonatomic, weak) id<TabGridToolbarCommands> dispatcher;

// The tab grid to be displayed. Setting this property displays the |tabGrid|.
@property(nonatomic, weak) UIViewController* tabGrid;

@property(nonatomic, assign, getter=isIncognito) BOOL incognito;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONTAINER_VIEW_CONTROLLER_H_
