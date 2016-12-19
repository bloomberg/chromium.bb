// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

@class SideSwipeGestureRecognizer;
@class TabModel;
@class WebToolbarController;

@interface SwipeView : UIView {
  base::scoped_nsobject<UIImageView> image_;
  base::scoped_nsobject<UIImageView> shadowView_;
  base::scoped_nsobject<UIImageView> toolbarHolder_;
}
@end

@interface CardSideSwipeView : UIView {
  // The direction of the swipe that initiated this horizontal view.
  UISwipeGestureRecognizerDirection direction_;

  // Card views currently displayed.
  base::scoped_nsobject<SwipeView> leftCard_;
  base::scoped_nsobject<SwipeView> rightCard_;

  // Most recent touch location.
  CGPoint currentPoint_;

  // Space reserved at the top for the toolbar.
  CGFloat topMargin_;

  // Tab model.
  TabModel* model_;  // weak

  // The image view containing the background image.
  base::scoped_nsobject<UIImageView> backgroundView_;
}

@property(nonatomic, assign) id<SideSwipeControllerDelegate> delegate;
@property(nonatomic, assign) CGFloat topMargin;

- (id)initWithFrame:(CGRect)frame
          topMargin:(CGFloat)margin
              model:(TabModel*)model;
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction
                    withToolbar:(WebToolbarController*)toolbarController;
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_
