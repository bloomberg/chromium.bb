// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/transitions/animators/swap_from_above_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SwapFromAboveAnimator

#pragma mark - UIViewControllerAnimatedTransitioning

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.25;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIViewController* toViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIViewController* fromViewController = [transitionContext
      viewControllerForKey:UITransitionContextFromViewControllerKey];
  [transitionContext.containerView addSubview:toViewController.view];
  CGRect insideFrame = transitionContext.containerView.bounds;
  CGRect outsideFrame =
      CGRectOffset(insideFrame, 0, -CGRectGetHeight(insideFrame));
  toViewController.view.frame = outsideFrame;

  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      animations:^{
        fromViewController.view.frame = outsideFrame;
        toViewController.view.frame = insideFrame;
      }
      completion:^(BOOL finished) {
        [transitionContext
            completeTransition:![transitionContext transitionWasCancelled]];
      }];
}

@end
