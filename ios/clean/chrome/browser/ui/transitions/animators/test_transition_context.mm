// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/transitions/animators/test_transition_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestTransitionContext
@synthesize completeTransitionCount = _completeTransitionCount;
@synthesize animated = _animated;
@synthesize interactive = _interactive;
@synthesize transitionWasCancelled = _transitionWasCancelled;
@synthesize presentationStyle = _presentationStyle;
@synthesize targetTransform = _targetTransform;
@synthesize containerView = _containerView;

#pragma mark - UIViewControllerContextTransitioning

- (void)updateInteractiveTransition:(CGFloat)percentComplete {
}

- (void)finishInteractiveTransition {
}

- (void)cancelInteractiveTransition {
}

- (void)pauseInteractiveTransition {
}

- (void)completeTransition:(BOOL)didComplete {
  self.completeTransitionCount++;
}

- (UIViewController*)viewControllerForKey:
    (UITransitionContextViewControllerKey)key {
  return nil;
}

- (UIView*)viewForKey:(UITransitionContextViewKey)key {
  return nil;
}

- (CGRect)initialFrameForViewController:(UIViewController*)vc {
  return CGRectZero;
}

- (CGRect)finalFrameForViewController:(UIViewController*)vc {
  return CGRectZero;
}

@end
