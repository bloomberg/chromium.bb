// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_grid/tab_grid_transition_state_provider.h"

@protocol GridConsumer;
@protocol GridImageDataSource;
@protocol GridViewControllerDelegate;

// Page enumerates the kinds of grouped tabs.
typedef NS_ENUM(NSUInteger, TabGridPage) {
  TabGridPageIncognitoTabs = 0,
  TabGridPageRegularTabs = 1,
  TabGridPageRemoteTabs = 2,
};

// View controller representing a tab switcher.  The tab switcher has an
// incognito tab grid, regular tab grid, and remote tabs.
@interface TabGridViewController
    : UIViewController<TabGridTransitionStateProvider>

// Consumers send updates from the model layer to the UI layer.
@property(nonatomic, readonly) id<GridConsumer> regularTabsConsumer;
@property(nonatomic, readonly) id<GridConsumer> incognitoTabsConsumer;

// Delegates send updates from the UI layer to the model layer.
@property(nonatomic, weak) id<GridViewControllerDelegate> regularTabsDelegate;
@property(nonatomic, weak) id<GridViewControllerDelegate> incognitoTabsDelegate;

// Data sources provide lazy access to heavy-weight resources.
@property(nonatomic, weak) id<GridImageDataSource> regularTabsImageDataSource;
@property(nonatomic, weak) id<GridImageDataSource> incognitoTabsImageDataSource;

// Current visible page.
@property(nonatomic, assign) TabGridPage currentPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
