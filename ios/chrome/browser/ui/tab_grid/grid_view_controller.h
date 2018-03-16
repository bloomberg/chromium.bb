// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid_theme.h"

@protocol GridImageDataSource;
@class GridTransitionLayout;
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
// Tells the receiver that the last item was closed in |gridViewController|.
- (void)lastItemWasClosedInGridViewController:
    (GridViewController*)gridViewController;
// Tells the receiver that the first item was added in |gridViewController|.
- (void)firstItemWasAddedInGridViewController:
    (GridViewController*)gridViewController;
@end

// A view controller that contains a grid of items.
@interface GridViewController : UIViewController<GridConsumer>
// The gridView is accessible to manage the content inset behavior.
@property(nonatomic, readonly) UIScrollView* gridView;
// The view that is shown when there are no items.
@property(nonatomic, strong) UIView* emptyStateView;
// Returns YES if the grid has no items.
@property(nonatomic, readonly, getter=isGridEmpty) BOOL gridEmpty;
// The number of items in the grid. Not all of the items may be visible.
@property(nonatomic, readonly) NSUInteger itemCount;
// The visual look of the grid.
@property(nonatomic, assign) GridTheme theme;
// Delegate is informed of user interactions in the grid UI.
@property(nonatomic, weak) id<GridViewControllerDelegate> delegate;
// Data source for images.
@property(nonatomic, weak) id<GridImageDataSource> imageDataSource;
// YES if the selected cell is visible in the grid.
@property(nonatomic, readonly, getter=isSelectedCellVisible)
    BOOL selectedCellVisible;

// Returns the layout of the grid for use in an animated transition.
- (GridTransitionLayout*)transitionLayout;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_VIEW_CONTROLLER_H_
