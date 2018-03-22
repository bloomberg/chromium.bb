// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_animation.h"

#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GridTransitionAnimation ()
@property(nonatomic, strong) GridTransitionLayout* layout;
@property(nonatomic, weak) id<GridTransitionAnimationDelegate> delegate;
@property(nonatomic, assign) CGFloat finalSelectedCellCornerRadius;
@end

@implementation GridTransitionAnimation
@synthesize delegate = _delegate;
@synthesize layout = _layout;
@synthesize finalSelectedCellCornerRadius = _finalSelectedCellCornerRadius;

- (instancetype)initWithLayout:(GridTransitionLayout*)layout
                      delegate:(id<GridTransitionAnimationDelegate>)delegate {
  if (self = [super initWithFrame:CGRectZero]) {
    _layout = layout;
    _delegate = delegate;
  }
  return self;
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  self.frame = newSuperview.bounds;
  if (newSuperview && self.subviews.count == 0) {
    [self prepareForAnimationInSuperview:newSuperview];
  }
}

- (void)prepareForAnimationInSuperview:(UIView*)newSuperview {
  // Extract some useful metrics from the animation superview.
  CGSize animationSize = newSuperview.bounds.size;
  CGPoint animationCenter = newSuperview.center;

  // Shorthand for selected item attributes.
  CGSize selectedSize = self.layout.selectedItem.attributes.size;
  CGPoint selectedCenter = self.layout.selectedItem.attributes.center;

  // Compute the scale of the transition grid (which is at the propotional size
  // of the superview).
  CGFloat xScale = animationSize.width / selectedSize.width;
  CGFloat yScale = animationSize.height / selectedSize.height;

  // Lay out the transition grid add it as subviews.
  for (GridTransitionLayoutItem* item in self.layout.items) {
    // The state provider vends attributes in UIWindow coordinates.
    // Find where this item is located in |newSuperview|'s coordinates.
    CGPoint gridCenter =
        [newSuperview convertPoint:item.attributes.center fromView:nil];
    // Map that to the scale and position of the transition grid.
    CGPoint center = CGPointMake(
        animationCenter.x + ((gridCenter.x - selectedCenter.x) * xScale),
        animationCenter.y + ((gridCenter.y - selectedCenter.y) * yScale));
    UICollectionViewCell* cell = item.cell;
    cell.bounds = item.attributes.bounds;
    // Add a scale transform to the cell so it matches the x-scale of the
    // open tab.
    cell.transform = CGAffineTransformScale(cell.transform, xScale, xScale);
    cell.center = center;
    if (item == self.layout.selectedItem) {
      self.finalSelectedCellCornerRadius = cell.contentView.layer.cornerRadius;
      cell.contentView.layer.cornerRadius = 0.0;
    }
    [self addSubview:cell];
  }
}

- (void)animateWithDuration:(NSTimeInterval)duration {
  // The transition is structured as two or three separate animations. They are
  // timed based on |staggeredDuration|, which is a configurable fraction
  // of the overall animation duration.
  CGFloat staggeredDuration = duration * 0.7;

  // If there's only one cell, the animation has two parts:
  //   (A) Fading in the selected cell highlight indicator.
  //   (B) Zooming the selected cell into position.
  // These parts are timed like this:
  //
  //                                 |*|----[A]----{100%}
  //  {0%}------------------[B]--------------------{100%}
  //
  //  (|*| is <staggeredDuration>%).
  // Animation B will call the completion handler in this case.

  // If there's more than once cell, the animation has three parts:
  //   (A) Fading in the selected cell highlight indicator.
  //   (B) Zooming the selected cell into position.
  //   (C) Zooming the unselected cells into position.
  // The timing is as follows:
  //
  //                                 |*|----[A]----{100%}
  //  {0%}------------------[B]------|*|
  //           |#|----------[C]--------------------{100%}
  //  (|*| is <staggeredDuration>%).
  //  (|#| is 1-<staggeredDuration>%).
  // Animation C will call the completion handler in this case.

  // TODO(crbug.com/820410): Tune the timing, relative pacing, and curves of
  // these animations.

  UICollectionViewCell* selectedCell = self.layout.selectedItem.cell;

  // Run animation (A) starting at |staggeredDuration|.
  [UIView animateWithDuration:duration - staggeredDuration
                        delay:staggeredDuration
                      options:UIViewAnimationOptionCurveEaseIn
                   animations:^{
                     selectedCell.selected = YES;
                   }
                   completion:nil];

  // Animation for (B): zoom the selected cell into place and round its corners.
  auto selectedCellAnimation = ^{
    selectedCell.center =
        [self.superview convertPoint:self.layout.selectedItem.attributes.center
                            fromView:nil];
    selectedCell.bounds = self.layout.selectedItem.attributes.bounds;
    selectedCell.transform = CGAffineTransformIdentity;
    selectedCell.contentView.layer.cornerRadius =
        self.finalSelectedCellCornerRadius;
  };

  // Completion block to be run when the transition completes.
  auto completion = ^(BOOL finished) {
    // Tell the delegate the animation has completed.
    [self.delegate gridTransitionAnimationDidFinish:finished];
  };

  if (self.layout.items.count == 1) {
    // Single cell case.
    // Run animation (B) for the whole duration without delay.
    [UIView animateWithDuration:duration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:selectedCellAnimation
                     completion:completion];
  } else {
    // Multiple cell case.
    // Run animation (B) up to |staggeredDuration|.
    [UIView animateWithDuration:staggeredDuration
                          delay:0.0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:selectedCellAnimation
                     completion:nil];

    // Animation for (C): zoom other cells into place.
    auto unselectedCellsAnimation = ^{
      for (GridTransitionLayoutItem* item in self.layout.items) {
        if (item == self.layout.selectedItem)
          continue;
        UIView* cell = item.cell;
        // The state provider vends attributes in UIWindow coordinates.
        // Find where this item is located in |newSuperview|'s coordinates.
        cell.center =
            [self.superview convertPoint:item.attributes.center fromView:nil];
        cell.transform = CGAffineTransformIdentity;
      }
    };

    // Run animation (C) for |staggeredDuration| up to the end of the
    // transition.
    [UIView animateWithDuration:staggeredDuration
                          delay:duration - staggeredDuration
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:unselectedCellsAnimation
                     completion:completion];
  }
}

@end
