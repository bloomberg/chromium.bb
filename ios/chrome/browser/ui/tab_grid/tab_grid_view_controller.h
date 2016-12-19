// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/animators/zoom_transition_delegate.h"

// The data source for tab grid UI.
// Conceptually the tab grid represents a group of WebState objects (which
// are ultimately the model-layer representation of a browser tab). The data
// source must be able to map between indices and WebStates.
@protocol TabGridDataSource<NSObject>

// The number of tabs to be displayed in the grid.
- (NSUInteger)numberOfTabsInTabGrid;

// Title for the tab at |index| in the grid.
- (NSString*)titleAtIndex:(NSInteger)index;

@end

@protocol TabGridActionDelegate<NSObject>
- (void)showTabAtIndexPath:(NSIndexPath*)indexPath;
- (void)showTabGrid;
- (void)showSettings;
@end

// Controller for a scrolling view displaying square cells that represent
// the user's open tabs.
@interface TabGridViewController : UIViewController<ZoomTransitionDelegate>

// Data source for the tabs to be displayed.
@property(nonatomic, weak) id<TabGridDataSource> dataSource;
@property(nonatomic, weak) id<TabGridActionDelegate> actionDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
