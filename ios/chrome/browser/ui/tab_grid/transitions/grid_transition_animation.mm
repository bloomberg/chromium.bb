// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_animation.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/util/property_animator_group.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Scale factor for inactive items when a tab is expanded.
const CGFloat kInactiveItemScale = 0.95;
}

@interface GridTransitionAnimation ()
// The property animator group backing the public |animator| property.
@property(nonatomic, readonly) PropertyAnimatorGroup* animations;
// The layout of the grid for this animation.
@property(nonatomic, strong) GridTransitionLayout* layout;
// The duration of the animation.
@property(nonatomic, readonly, assign) NSTimeInterval duration;
// The direction this animation is in.
@property(nonatomic, readonly, assign) GridAnimationDirection direction;
// Convenience properties for getting the size and center of the active cell
// in the grid.
@property(nonatomic, readonly) CGSize activeSize;
@property(nonatomic, readonly) CGPoint activeCenter;
// Corner radius that the active cell will have when it is animated into the
// regulat grid.
@property(nonatomic, assign) CGFloat finalActiveCellCornerRadius;
@end

@implementation GridTransitionAnimation
@synthesize animations = _animations;
@synthesize layout = _layout;
@synthesize duration = _duration;
@synthesize direction = _direction;
@synthesize finalActiveCellCornerRadius = _finalActiveCellCornerRadius;

- (instancetype)initWithLayout:(GridTransitionLayout*)layout
                      duration:(NSTimeInterval)duration
                     direction:(GridAnimationDirection)direction {
  if (self = [super initWithFrame:CGRectZero]) {
    _animations = [[PropertyAnimatorGroup alloc] init];
    _layout = layout;
    _duration = duration;
    _direction = direction;
    _finalActiveCellCornerRadius =
        _layout.activeItem.cell.contentView.layer.cornerRadius;
  }
  return self;
}

- (id<UIViewImplicitlyAnimating>)animator {
  return self.animations;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  self.frame = newSuperview.bounds;
  if (newSuperview && self.subviews.count == 0) {
    [self prepareForAnimationInSuperview:newSuperview];
  }
}

- (void)didMoveToSuperview {
  if (!self.superview)
    return;
  // Positioning the animating items depends on converting points to this
  // view's coordinate system, so wait until it's in a view hierarchy.
  switch (self.direction) {
    case GridAnimationDirectionContracting:
      [self positionExpandedActiveItem];
      [self prepareInactiveItemsForAppearance];
      [self buildContractingAnimations];
      break;
    case GridAnimationDirectionExpanding:
      [self prepareAllItemsForExpansion];
      [self buildExpandingAnimations];
      break;
  }
  // Make sure all of the layout after the view setup is complete before any
  // animations are run.
  [self layoutIfNeeded];
}

#pragma mark - Private Properties

- (CGSize)activeSize {
  return self.layout.activeItem.attributes.size;
}

- (CGPoint)activeCenter {
  return self.layout.activeItem.attributes.center;
}

#pragma mark - Private methods

- (void)buildContractingAnimations {
  // The transition is structured as two or four separate animations. They are
  // timed based on various sub-durations and delays which are expressed as
  // fractions of the overall animation duration.
  CGFloat partialDuration = 0.6;
  CGFloat briefDuration = partialDuration * 0.5;
  CGFloat delay = 0.4;
  CGFloat shortDelay = 0.2;

  // If there's only one cell, the animation has two parts.
  //   (A) Zooming the active cell into position.
  //   (B) Fading in the active cell's auxilliary view.
  //   (C) Rounding the corners of the active cell.
  //
  //  {0%}------------------[A]----------{60%}
  //                 {40%}--[B]---------------{70%}
  //  {0%}---[C]---{30%}

  // If there's more than once cell, the animation adds two more parts:
  //   (D) Scaling up the inactive cells.
  //   (E) Fading the inactive cells to 100% opacity.
  // The overall timing is as follows:
  //
  //  {0%}------------------[A]----------{60%}
  //                 {40%}--[B]---------------{70%}
  //  {0%}---[C]---{30%}
  //           {20%}--[D]-------------------------{100%}
  //           {20%}--[E]----------------------{80%}
  //
  // All animations are timed ease-in (so more motion happens later), except
  // for C which is relatively small in space and short in duration; it has
  // linear timing so it doesn't seem instantaneous.
  // (Changing the timing constants above will change the timing % values)

  // A: Zoom the active cell into position.
  auto zoomActiveCellAnimation = ^{
    [self positionAndScaleActiveItemInGrid];
  };
  auto zoomActiveCellKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:0
                              relativeDuration:partialDuration
                                    animations:zoomActiveCellAnimation];
  UIViewPropertyAnimator* zoomActiveCell = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseIn
            animations:zoomActiveCellKeyframeAnimation];
  [self.animations addAnimator:zoomActiveCell];

  // B: Fade in the active cell's auxillary view
  UIView* auxillaryView = self.layout.activeItem.auxillaryView;
  auto fadeInAuxillaryKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:delay
                              relativeDuration:briefDuration
                                    animations:^{
                                      auxillaryView.alpha = 1.0;
                                    }];
  UIViewPropertyAnimator* fadeInAuxillary = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseIn
            animations:fadeInAuxillaryKeyframeAnimation];
  [self.animations addAnimator:fadeInAuxillary];

  // C: Round the corners of the active cell.
  UICollectionViewCell* cell = self.layout.activeItem.cell;
  cell.contentView.layer.cornerRadius = 0.0;
  auto roundCornersAnimation = ^{
    cell.contentView.layer.cornerRadius = self.finalActiveCellCornerRadius;
  };
  auto roundCornersKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:0
                              relativeDuration:briefDuration
                                    animations:roundCornersAnimation];
  UIViewPropertyAnimator* roundCorners = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveLinear
            animations:roundCornersKeyframeAnimation];
  [self.animations addAnimator:roundCorners];

  if (self.layout.items.count == 1) {
    // Single cell case.
    return;
  }

  // Additional animations for multiple cells.
  // D: Scale up inactive cells.
  auto scaleUpCellsAnimation = ^{
    for (GridTransitionLayoutItem* item in self.layout.items) {
      if (item == self.layout.activeItem)
        continue;
      item.cell.transform = CGAffineTransformIdentity;
    }
  };

  auto scaleUpCellsKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:shortDelay
                              relativeDuration:1 - shortDelay
                                    animations:scaleUpCellsAnimation];
  UIViewPropertyAnimator* scaleUpCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseIn
            animations:scaleUpCellsKeyframeAnimation];
  [self.animations addAnimator:scaleUpCells];

  // E: Fade in inactive cells.
  auto fadeInCellsAnimation = ^{
    for (GridTransitionLayoutItem* item in self.layout.items) {
      if (item == self.layout.activeItem)
        continue;
      item.cell.alpha = 1.0;
    }
  };
  auto fadeInCellsKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:delay
                              relativeDuration:partialDuration
                                    animations:fadeInCellsAnimation];
  UIViewPropertyAnimator* fadeInCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseIn
            animations:fadeInCellsKeyframeAnimation];
  [self.animations addAnimator:fadeInCells];
}

- (void)buildExpandingAnimations {
  // The transition is structured as two or four separate animations. They are
  // timed based on two sub-durations which are expressed as fractions of the
  // overall animation duration.
  CGFloat partialDuration = 0.66;
  CGFloat briefDuration = 0.23;

  // If there's only one cell, the animation has three parts:
  //   (A) Zooming the active cell out into the expanded position.
  //   (B) Fading out the active cell's auxilliary view.
  //   (C) Squaring the corners of the active cell.
  // These parts are timed over |duration| like this:
  //
  //  {0%}--[A]-----------------------------------{100%}
  //  {0%}--[B]--{23%}
  //                                {77%}---[C]---{100%}

  // If there's more than once cell, the animation adds:
  //   (C) Scaling the inactive cells to 95%
  //   (D) Fading out the inactive cells.
  // The overall timing is as follows:
  //
  //  {0%}--[A]-----------------------------------{100%}
  //  {0%}--[B]--{23%}
  //                                {77%}---[C]---{100%}
  //  {0%}--[D]-----------------------------------{100%}
  //  {0%}--[E]-----------------{66%}
  //
  // All animations are timed ease-out (so more motion happens sooner), except
  // for C which is relatively small in space and short in duration; it has
  // linear timing so it doesn't seem instantaneous.

  // A: Zoom the active cell into position.
  UIViewPropertyAnimator* zoomActiveCell = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:^{
              [self positionExpandedActiveItem];
            }];
  [self.animations addAnimator:zoomActiveCell];

  // B: Fade out the active cell's auxillary view.
  UIView* auxillaryView = self.layout.activeItem.auxillaryView;
  auto fadeOutAuxilliaryAnimation =
      [self keyframeAnimationWithRelativeStart:0
                              relativeDuration:briefDuration
                                    animations:^{
                                      auxillaryView.alpha = 0.0;
                                    }];
  UIViewPropertyAnimator* fadeOutAuxilliary = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:fadeOutAuxilliaryAnimation];
  [self.animations addAnimator:fadeOutAuxilliary];

  // C: Square the active cell's corners.
  UICollectionViewCell* cell = self.layout.activeItem.cell;
  auto squareCornersAnimation = ^{
    cell.contentView.layer.cornerRadius = 0.0;
  };
  auto squareCornersKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:1.0 - briefDuration
                              relativeDuration:briefDuration
                                    animations:squareCornersAnimation];
  UIViewPropertyAnimator* squareCorners = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveLinear
            animations:squareCornersKeyframeAnimation];
  [self.animations addAnimator:squareCorners];

  // If there's only a single cell, that's all.
  if (self.layout.items.count == 1)
    return;

  // Additional animations for multiple cells.
  // D: Scale down inactive cells.
  auto scaleDownCellsAnimation = ^{
    for (GridTransitionLayoutItem* item in self.layout.items) {
      if (item == self.layout.activeItem)
        continue;
      item.cell.transform = CGAffineTransformScale(
          item.cell.transform, kInactiveItemScale, kInactiveItemScale);
    }
  };
  UIViewPropertyAnimator* scaleDownCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:scaleDownCellsAnimation];
  [self.animations addAnimator:scaleDownCells];

  // E: Fade out inactive cells.
  auto fadeOutCellsAnimation = ^{
    for (GridTransitionLayoutItem* item in self.layout.items) {
      if (item == self.layout.activeItem)
        continue;
      item.cell.alpha = 0.0;
    }
  };
  auto fadeOutCellsKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:0
                              relativeDuration:partialDuration
                                    animations:fadeOutCellsAnimation];
  UIViewPropertyAnimator* fadeOutCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:fadeOutCellsKeyframeAnimation];
  [self.animations addAnimator:fadeOutCells];
}

// Perfroms the initial setup for the animation, computing scale based on the
// superview size and adding the transition cells to the view hierarchy.
- (void)prepareForAnimationInSuperview:(UIView*)newSuperview {
  // Add the selection item first, so it's under ther other views.
  [self addSubview:self.layout.selectionItem.cell];

  // Only show the selection part.
  self.layout.selectionItem.cell.contentView.hidden = YES;
  self.layout.selectionItem.cell.selected = YES;

  // Add the active item last so it's always the top subview.
  for (GridTransitionLayoutItem* item in self.layout.items) {
    if (item == self.layout.activeItem)
      continue;
    [self addSubview:item.cell];
  }
  [self addSubview:self.layout.activeItem.cell];
}

// Positions the active item in the expanded grid position with a zero corner
// radius and a 0% opacity auxilliary view.
- (void)positionExpandedActiveItem {
  GridTransitionLayoutItem* activeItem = self.layout.activeItem;
  UICollectionViewCell* cell = activeItem.cell;
  // Ensure that the cell's subviews are correctly positioned.
  [cell layoutIfNeeded];
  // Position the cell frame so so that the area below the aux view matches the
  // expanded rect.
  // Easiest way to do this is to set the frame to the expanded rect and then
  // add height to it to include the aux view height.
  CGFloat auxHeight = activeItem.auxillaryView.frame.size.height;
  CGRect cellFrame = self.layout.expandedRect;
  cellFrame.size.height += auxHeight;
  cellFrame.origin.y -= auxHeight;
  cell.frame = cellFrame;
  activeItem.auxillaryView.alpha = 0.0;
}

// Positions all of the inactive items in their grid positions.
// Fades and scales each of those items.
- (void)prepareInactiveItemsForAppearance {
  for (GridTransitionLayoutItem* item in self.layout.items) {
    if (item == self.layout.activeItem)
      continue;
    [self positionItemInGrid:item];
    item.cell.alpha = 0.0;
    item.cell.transform = CGAffineTransformScale(
        item.cell.transform, kInactiveItemScale, kInactiveItemScale);
  }
  [self positionItemInGrid:self.layout.selectionItem];
}

// Positions the active item in the regular grid position with its final
// corner radius.
- (void)positionAndScaleActiveItemInGrid {
  UICollectionViewCell* cell = self.layout.activeItem.cell;
  cell.transform = CGAffineTransformIdentity;
  cell.frame = self.layout.activeItem.attributes.frame;
  [self positionItemInGrid:self.layout.activeItem];
}

// Prepares all of the items for an expansion anumation.
- (void)prepareAllItemsForExpansion {
  for (GridTransitionLayoutItem* item in self.layout.items) {
    [self positionItemInGrid:item];
  }
  [self positionItemInGrid:self.layout.selectionItem];
}

// Positions |item| in it grid position.
- (void)positionItemInGrid:(GridTransitionLayoutItem*)item {
  UIView* cell = item.cell;
  CGPoint attributeCenter = item.attributes.center;
  CGPoint newCenter =
      [self.superview convertPoint:attributeCenter fromView:nil];
  cell.center = newCenter;
}

// Helper function to construct keyframe animation blocks.
// Given |start| and |duration| (in the [0.0-1.0] interval), returns an
// animation block which runs |animations| starting at |start| (relative to
// |self.duration|) and running for |duration| (likewise).
- (void (^)(void))keyframeAnimationWithRelativeStart:(double)start
                                    relativeDuration:(double)duration
                                          animations:
                                              (void (^)(void))animations {
  auto keyframe = ^{
    [UIView addKeyframeWithRelativeStartTime:start
                            relativeDuration:duration
                                  animations:animations];
  };
  return ^{
    [UIView animateKeyframesWithDuration:self.duration
                                   delay:0
                                 options:UIViewAnimationOptionLayoutSubviews
                              animations:keyframe
                              completion:nil];
  };
}

@end
