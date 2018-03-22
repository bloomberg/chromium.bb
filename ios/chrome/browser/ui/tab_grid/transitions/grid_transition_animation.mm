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
@property(nonatomic, assign) CGFloat xScale;
@property(nonatomic, assign) CGFloat yScale;
@property(nonatomic, readonly) CGSize selectedSize;
@property(nonatomic, readonly) CGPoint selectedCenter;
@property(nonatomic, assign) CGFloat finalSelectedCellCornerRadius;
@end

@implementation GridTransitionAnimation
@synthesize delegate = _delegate;
@synthesize layout = _layout;
@synthesize xScale = _xScale;
@synthesize yScale = _yScale;
@synthesize finalSelectedCellCornerRadius = _finalSelectedCellCornerRadius;

- (instancetype)initWithLayout:(GridTransitionLayout*)layout
                      delegate:(id<GridTransitionAnimationDelegate>)delegate {
  if (self = [super initWithFrame:CGRectZero]) {
    _layout = layout;
    _delegate = delegate;
    _finalSelectedCellCornerRadius =
        _layout.selectedItem.cell.contentView.layer.cornerRadius;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  self.frame = newSuperview.bounds;
  if (newSuperview && self.subviews.count == 0) {
    [self prepareForAnimationInSuperview:newSuperview];
  }
}

- (void)didMoveToSuperview {
  // Positioning the animating items depends on converting points to this
  // view's coordinate system, so wait until it's in a view hierarchy.
  [self positionSelectedItemInExpandedGrid];
  [self positionUnselectedItemsInExpandedGrid];
}

#pragma mark - Private Properties

- (CGSize)selectedSize {
  return self.layout.selectedItem.attributes.size;
}

- (CGPoint)selectedCenter {
  return self.layout.selectedItem.attributes.center;
}

#pragma mark - Public methods

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
                     animations:^{
                       [self positionSelectedItemInRegularGrid];
                     }
                     completion:completion];
  } else {
    // Multiple cell case.
    // Run animation (B) up to |staggeredDuration|.
    [UIView animateWithDuration:staggeredDuration
                          delay:0.0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:^{
                       [self positionSelectedItemInRegularGrid];
                     }
                     completion:nil];

    // Run animation (C) for |staggeredDuration| up to the end of the
    // transition.
    [UIView animateWithDuration:staggeredDuration
                          delay:duration - staggeredDuration
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:^{
                       [self positionUnselectedItemsInRegularGrid];
                     }
                     completion:completion];
  }
}

#pragma mark - Private methods

// Perfrom the initial setup for the animation, computing scale based on the
// superview size and adding the transition cells to the view hierarchy.
- (void)prepareForAnimationInSuperview:(UIView*)newSuperview {
  // Extract some useful metrics from the animation superview.
  CGSize animationSize = newSuperview.bounds.size;

  // Compute the scale of the transition grid (which is at the proportional size
  // of the superview).
  self.xScale = animationSize.width / self.selectedSize.width;
  self.yScale = animationSize.height / self.selectedSize.height;

  for (GridTransitionLayoutItem* item in self.layout.items) {
    [self addSubview:item.cell];
  }
}

// Positions the selected item in the expanded grid position with a zero corner
// radius.
- (void)positionSelectedItemInExpandedGrid {
  [self positionAndScaleItemInExpandedGrid:self.layout.selectedItem];
  UICollectionViewCell* cell = self.layout.selectedItem.cell;
  cell.contentView.layer.cornerRadius = 0.0;
}

// Positions all of the non-selected items in their expanded grid positions.
- (void)positionUnselectedItemsInExpandedGrid {
  // Lay out the transition grid add it as subviews.
  for (GridTransitionLayoutItem* item in self.layout.items) {
    if (item == self.layout.selectedItem)
      continue;
    [self positionAndScaleItemInExpandedGrid:item];
  }
}

// Positions the selected item in the regular grid position with its final
// corner radius.
- (void)positionSelectedItemInRegularGrid {
  [self positionAndScaleItemInRegularGrid:self.layout.selectedItem];
  UICollectionViewCell* cell = self.layout.selectedItem.cell;
  cell.contentView.layer.cornerRadius = self.finalSelectedCellCornerRadius;
}

// Positions all of the non-selected items in their regular grid positions.
- (void)positionUnselectedItemsInRegularGrid {
  for (GridTransitionLayoutItem* item in self.layout.items) {
    if (item == self.layout.selectedItem)
      continue;
    [self positionAndScaleItemInRegularGrid:item];
  }
}

// Positions |item| in its regular grid position.
- (void)positionAndScaleItemInRegularGrid:(GridTransitionLayoutItem*)item {
  UIView* cell = item.cell;
  cell.center =
      [self.superview convertPoint:item.attributes.center fromView:nil];
  cell.transform = CGAffineTransformIdentity;
}

// Positions |item| in its expanded grid position.
- (void)positionAndScaleItemInExpandedGrid:(GridTransitionLayoutItem*)item {
  UICollectionViewCell* cell = item.cell;
  cell.bounds = item.attributes.bounds;
  // Add a scale transform to the cell so it matches the x-scale of the
  // open tab. Scaling is only based on the x-scale so that the aspect ratio of
  // the cell will be preserved.
  cell.transform =
      CGAffineTransformScale(cell.transform, self.xScale, self.xScale);
  cell.center = [self expandedCenterForItem:item];
}

// Returns the center point for an item in the expanded grid position. This is
// computed by scaling its center point relative to the selected item's center
// point. The scaling factors are the ratios of the animation view's height and
// width to the selected cell's height and width.
- (CGPoint)expandedCenterForItem:(GridTransitionLayoutItem*)item {
  // Convert item center from window coordinates.
  CGPoint gridCenter = [self convertPoint:item.attributes.center fromView:nil];
  // Map that to the scale and position of the transition grid.
  return CGPointMake(
      self.center.x + ((gridCenter.x - self.selectedCenter.x) * self.xScale),
      self.center.y + ((gridCenter.y - self.selectedCenter.y) * self.yScale));
}

@end
