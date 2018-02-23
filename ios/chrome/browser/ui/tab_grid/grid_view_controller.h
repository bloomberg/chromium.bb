// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid_theme.h"

@protocol GridImageDataSource;
@class GridViewController;

// Protocol used to relay relevant user interactions from a grid UI.
@protocol GridViewControllerDelegate
// Tells the receiver that the item at |index| was selected in
// |gridViewController|.
- (void)gridViewController:(GridViewController*)gridViewController
      didSelectItemAtIndex:(NSUInteger)index;
// Tells the receiver that the item at |index| was closed in
// |gridViewController|.
- (void)gridViewController:(GridViewController*)gridViewController
       didCloseItemAtIndex:(NSUInteger)index;
@end

// A view controller that contains a grid of items.
@interface GridViewController : UIViewController<GridConsumer>
// The gridView is accessible to manage the content inset behavior.
@property(nonatomic, readonly) UIScrollView* gridView;
// The visual look of the grid.
@property(nonatomic, assign) GridTheme theme;
// Delegate is informed of user interactions in the grid UI.
@property(nonatomic, weak) id<GridViewControllerDelegate> delegate;
// Data source for images.
@property(nonatomic, weak) id<GridImageDataSource> imageDataSource;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_VIEW_CONTROLLER_H_
