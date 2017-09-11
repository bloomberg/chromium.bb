// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_view_controller.h"

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_consumer.h"
#import "ios/clean/chrome/browser/ui/transitions/animators/zoom_transition_delegate.h"

@protocol TabGridCommands;

// View controller with a grid of tabs.
@interface TabGridViewController
    : TabCollectionViewController<TabGridConsumer, ZoomTransitionDelegate>
// Dispatcher to handle commands.
@property(nonatomic, weak) id<TabGridCommands> dispatcher;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
