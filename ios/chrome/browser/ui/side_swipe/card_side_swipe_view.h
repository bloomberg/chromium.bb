// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

@class SideSwipeGestureRecognizer;
@protocol SideSwipeToolbarInteracting;
@class TabModel;

@interface SwipeView : UIView {
  UIImageView* image_;
  UIImageView* shadowView_;
  UIImageView* toolbarHolder_;
}
@end

@interface CardSideSwipeView : UIView {
  // The direction of the swipe that initiated this horizontal view.
  UISwipeGestureRecognizerDirection direction_;

  // Card views currently displayed.
  SwipeView* leftCard_;
  SwipeView* rightCard_;

  // Most recent touch location.
  CGPoint currentPoint_;

  // Space reserved at the top for the toolbar.
  CGFloat topMargin_;

  // Tab model.
  __weak TabModel* model_;

  // The image view containing the background image.
  UIImageView* backgroundView_;
}

@property(nonatomic, weak) id<SideSwipeControllerDelegate> delegate;
@property(nonatomic, weak) id<SideSwipeToolbarInteracting>
    toolbarInteractionHandler;
@property(nonatomic, assign) CGFloat topMargin;

- (id)initWithFrame:(CGRect)frame
          topMargin:(CGFloat)margin
              model:(TabModel*)model;
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction;
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_
