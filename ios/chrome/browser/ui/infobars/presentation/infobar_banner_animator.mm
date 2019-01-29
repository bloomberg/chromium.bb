// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation InfobarBannerAnimator

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 1;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Set up the keys for the "base" view/VC and the "presented" view/VC. These
  // will be used to fetch the associated objects later.
  NSString* baseViewKey = self.presenting ? UITransitionContextFromViewKey
                                          : UITransitionContextToViewKey;
  NSString* presentedViewControllerKey =
      self.presenting ? UITransitionContextToViewControllerKey
                      : UITransitionContextFromViewControllerKey;
  NSString* presentedViewKey = self.presenting ? UITransitionContextToViewKey
                                               : UITransitionContextFromViewKey;

  // Get views and view controllers for this transition.
  UIView* baseView = [transitionContext viewForKey:baseViewKey];
  UIViewController* presentedViewController =
      [transitionContext viewControllerForKey:presentedViewControllerKey];
  UIView* presentedView = [transitionContext viewForKey:presentedViewKey];

  // Always add the destination view to the container.
  UIView* containerView = [transitionContext containerView];
  if (self.presenting) {
    [containerView addSubview:presentedView];
  } else {
    [containerView addSubview:baseView];
  }

  // Set the initial frame and Compute the final frame for the presented view.
  CGRect presentedViewFinalFrame = CGRectZero;

  if (self.presenting) {
    presentedViewFinalFrame =
        [transitionContext finalFrameForViewController:presentedViewController];
    CGRect presentedViewStartFrame = presentedViewFinalFrame;
    presentedViewStartFrame.origin.y = -CGRectGetWidth(containerView.bounds);
    presentedView.frame = presentedViewStartFrame;
  } else {
    presentedViewFinalFrame = presentedView.frame;
    presentedViewFinalFrame.origin.y = -CGRectGetWidth(containerView.bounds);
  }

  // Animate using the animator's own duration value.
  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      delay:0
      usingSpringWithDamping:0.85
      initialSpringVelocity:0
      options:UIViewAnimationOptionTransitionNone
      animations:^{
        presentedView.frame = presentedViewFinalFrame;
      }
      completion:^(BOOL finished) {
        BOOL success = ![transitionContext transitionWasCancelled];

        // If presentation failed, remove the view.
        if (self.presenting && !success) {
          [presentedView removeFromSuperview];
        }

        // Notify UIKit that the transition has finished
        [transitionContext completeTransition:success];
      }];
}

@end
