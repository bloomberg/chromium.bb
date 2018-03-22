// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/transitions/tab_to_grid_animator.h"

#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_state_providing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabToGridAnimator ()
// State provider for this transition.
@property(nonatomic, weak) id<GridTransitionStateProviding> stateProvider;
@end

@implementation TabToGridAnimator
@synthesize stateProvider = _stateProvider;

- (instancetype)initWithStateProvider:
    (id<GridTransitionStateProviding>)stateProvider {
  if ((self = [super init])) {
    _stateProvider = stateProvider;
  }
  return self;
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.4;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Get views and view controllers for this transition.
  UIView* containerView = [transitionContext containerView];
  UIViewController* gridViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIView* gridView =
      [transitionContext viewForKey:UITransitionContextToViewKey];
  UIView* dismissingView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];

  // Extract some useful metrics from the tab view.
  CGSize proxySize = dismissingView.bounds.size;
  CGPoint proxyCenter = dismissingView.center;

  // Add the grid view to the container. This isn't just for the transition;
  // this is how the grid view controller's view is added to the view
  // hierarchy.
  [containerView insertSubview:gridView belowSubview:dismissingView];
  gridView.frame =
      [transitionContext finalFrameForViewController:gridViewController];

  // Get the layout of the grid for the transition.
  GridTransitionLayout* layout =
      [self.stateProvider layoutForTransitionContext:transitionContext];

  // Compute the scale of the transition grid (which is at the propotional size
  // of the actual tab view.
  CGFloat xScale = proxySize.width / layout.selectedItem.attributes.size.width;
  CGFloat yScale =
      proxySize.height / layout.selectedItem.attributes.size.height;

  // Ask the state provider for the views to use when inserting the tab grid.
  UIView* proxyContainer =
      [self.stateProvider proxyContainerForTransitionContext:transitionContext];
  UIView* viewBehindProxies =
      [self.stateProvider proxyPositionForTransitionContext:transitionContext];

  // Lay out the transition grid and add it to the view hierarchy.
  CGFloat finalSelectedCellCornerRadius = 0.0;
  for (GridTransitionLayoutItem* item in layout.items) {
    // The state provider vends attributes in UIWindow coordinates.
    // Find where this item is located in |proxyContainer|'s coordinate.
    CGPoint gridCenter =
        [proxyContainer convertPoint:item.attributes.center fromView:nil];
    // Map that to the scale and position of the transition grid.
    CGPoint center = CGPointMake(
        proxyCenter.x +
            ((gridCenter.x - layout.selectedItem.attributes.center.x) * xScale),
        proxyCenter.y +
            ((gridCenter.y - layout.selectedItem.attributes.center.y) *
             yScale));
    UICollectionViewCell* cell = item.cell;
    cell.bounds = item.attributes.bounds;
    // Add a scale transform to the cell so it matches the x-scale of the
    // open tab.
    cell.transform = CGAffineTransformScale(cell.transform, xScale, xScale);
    cell.center = center;
    if (item == layout.selectedItem) {
      finalSelectedCellCornerRadius = cell.contentView.layer.cornerRadius;
      cell.contentView.layer.cornerRadius = 0.0;
    }
    // Add the cell into the container for the transition.
    [proxyContainer insertSubview:cell aboveSubview:viewBehindProxies];
  }

  // The transition is structured as four separate animations. Three of them
  // are timed based on |staggeredDuration|, which is a configurable fraction
  // of the overall animation duration.
  // (1) Fading out the view being dismissed. This happens during the first 20%
  //     of the overall animation.
  // (2) Fading in the selected cell highlight indicator. This starts after a
  //     delay of |staggeredDuration| and runs to the end of the transition.
  //     This means it starts as soon as (3) ends.
  // (3) Zooming the selected cell into position. This starts immediately and
  //     has a duration of |staggeredDuration|.
  // (4) Zooming all other cells into position. This ends at the end of the
  //     transition and has a duration of |staggeredDuration|.
  //
  // Animation (4) always runs the whole duration of the transition, so it's
  // where the completion block that does overall cleanup is run.

  // In the case where there is only a single cell to animate (the selected
  // one), not all of the above animations are required. Instead of animations
  // (3), and (4), instead the following animation will be used:
  // (5) Zooming the selected cell into position as in (3), but running the
  //     whole duration of the transition.
  // In this case animation (5) will run the completion block.

  // TODO(crbug.com/804539): Factor all of these animations into a single
  // Orchestrator object that the present and dismiss animation can both use.

  // TODO(crbug.com/820410): Tune the timing, relative pacing, and curves of
  // these animations.

  // Completion block to be run when the transition completes.
  auto completion = ^(BOOL finished) {
    // Clean up all of the proxy cells.
    for (GridTransitionLayoutItem* item in layout.items) {
      [item.cell removeFromSuperview];
    }
    // If the transition was cancelled, restore the dismissing view and
    // remove the grid view.
    // If not, remove the dismissing view.
    if (transitionContext.transitionWasCancelled) {
      dismissingView.alpha = 1.0;
      [gridView removeFromSuperview];
    } else {
      [dismissingView removeFromSuperview];
    }
    // Mark the transition as completed.
    [transitionContext completeTransition:YES];
  };

  NSTimeInterval duration = [self transitionDuration:transitionContext];
  CGFloat staggeredDuration = duration * 0.7;
  UICollectionViewCell* selectedCell = layout.selectedItem.cell;

  // (1) Fade out active tab view.
  [UIView animateWithDuration:duration / 5
                   animations:^{
                     dismissingView.alpha = 0;
                   }
                   completion:nil];

  // (2) Show highlight state on selected cell.
  [UIView animateWithDuration:duration - staggeredDuration
                        delay:staggeredDuration
                      options:UIViewAnimationOptionCurveEaseIn
                   animations:^{
                     selectedCell.selected = YES;
                   }
                   completion:nil];

  // Animation block for zooming the selected cell into place and rounding
  // its corners.
  auto selectedCellAnimation = ^{
    selectedCell.center =
        [containerView convertPoint:layout.selectedItem.attributes.center
                           fromView:nil];
    selectedCell.bounds = layout.selectedItem.attributes.bounds;
    selectedCell.transform = CGAffineTransformIdentity;
    selectedCell.contentView.layer.cornerRadius = finalSelectedCellCornerRadius;
  };

  // If there's more than one cell to animate, run animations (3) and (4), and
  // call the completion block after (4). If there's only one cell to animate,
  // run (5) and have it call the completion block.
  if (layout.items.count > 1) {
    // (3) Zoom selected cell into place.
    [UIView animateWithDuration:staggeredDuration
                          delay:0.0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:selectedCellAnimation
                     completion:nil];

    // (4) Zoom other cells into place.
    auto unselectedCellsAnimation = ^{
      for (GridTransitionLayoutItem* item in layout.items) {
        if (item == layout.selectedItem)
          continue;
        UIView* cell = item.cell;
        cell.center =
            [containerView convertPoint:item.attributes.center fromView:nil];
        cell.transform = CGAffineTransformIdentity;
      }
    };

    [UIView animateWithDuration:staggeredDuration
                          delay:duration - staggeredDuration
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:unselectedCellsAnimation
                     completion:completion];
  } else {
    // (5) Zoom selected cell into place over whole duration, with completion.
    [UIView animateWithDuration:duration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:selectedCellAnimation
                     completion:completion];
  }
}

@end
