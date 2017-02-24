// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_delegate.h"

@protocol SettingsCommands;
@protocol TabCommands;
@protocol TabGridCommands;

// The data source for tab grid UI.
// Conceptually the tab grid represents a group of WebState objects (which
// are ultimately the model-layer representation of a browser tab). The data
// source must be able to map between indices and WebStates.
@protocol TabGridDataSource<NSObject>

// The number of tabs to be displayed in the grid.
- (NSUInteger)numberOfTabsInTabGrid;

// Title for the tab at |index| in the grid.
- (NSString*)titleAtIndex:(NSInteger)index;

// Index for the active tab.
- (NSInteger)indexOfActiveTab;

@end

// Controller for a scrolling view displaying square cells that represent
// the user's open tabs.
@interface TabGridViewController : UIViewController<ZoomTransitionDelegate>

// Data source for the tabs to be displayed.
@property(nonatomic, weak) id<TabGridDataSource> dataSource;
// Command handlers.
@property(nonatomic, weak) id<SettingsCommands> settingsCommandHandler;
@property(nonatomic, weak) id<TabCommands> tabCommandHandler;
@property(nonatomic, weak) id<TabGridCommands> tabGridCommandHandler;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
