// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation IdentityChooserAnimator

@synthesize appearing = _appearing;

#pragma mark - UIViewControllerAnimatedTransitioning

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Get views and view controllers for this transition.
  // The baseView is the view on top of which the VC is presented.
  UIView* baseView = [transitionContext
      viewForKey:self.appearing ? UITransitionContextFromViewKey
                                : UITransitionContextToViewKey];
  // The VC being presented/dismissed and its view.
  UIViewController* presentedViewController = [transitionContext
      viewControllerForKey:self.appearing
                               ? UITransitionContextToViewControllerKey
                               : UITransitionContextFromViewControllerKey];
  UIView* presentedView = [transitionContext
      viewForKey:self.appearing ? UITransitionContextToViewKey
                                : UITransitionContextFromViewKey];

  // Always add the destination view to the container.
  UIView* containerView = [transitionContext containerView];
  if (self.appearing) {
    [containerView addSubview:presentedView];
  } else {
    [containerView addSubview:baseView];
  }

  presentedView.frame =
      [transitionContext finalFrameForViewController:presentedViewController];
  [transitionContext completeTransition:YES];
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0;
}

@end
