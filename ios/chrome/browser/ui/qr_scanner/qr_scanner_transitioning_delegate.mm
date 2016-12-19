// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_transitioning_delegate.h"

#include "base/ios/block_types.h"

namespace {

// The default animation duration.
const NSTimeInterval kDefaultDuration = 0.25;

enum QRScannerTransition { PRESENT, DISMISS };

}  // namespace

// Animates the QR Scanner transition. If initialized with the |PRESENT|
// transition, positions the QR Scanner view below its presenting view
// controller's view in the container view and animates the presenting view to
// slide up. If initialized with the |DISMISS| transition, positions the
// presenting view controller's view above the QR Scanner view in the container
// view and animates the presenting view to slide down.
@interface QRScannerTransitionAnimator
    : NSObject<UIViewControllerAnimatedTransitioning> {
  QRScannerTransition _transition;
}

- (instancetype)initWithTransition:(QRScannerTransition)transition;

@end

@implementation QRScannerTransitionAnimator

- (instancetype)initWithTransition:(QRScannerTransition)transition {
  self = [super init];
  if (self) {
    _transition = transition;
  }
  return self;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* containerView = [transitionContext containerView];
  UIView* presentedView =
      [transitionContext viewForKey:UITransitionContextToViewKey];
  UIView* presentingView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];

  // Get the final frame for the presented view.
  UIViewController* presentedViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  CGRect finalFrame =
      [transitionContext finalFrameForViewController:presentedViewController];

  CGRect initialFrame;
  ProceduralBlock animations;

  switch (_transition) {
    case PRESENT:
      [containerView insertSubview:presentedView belowSubview:presentingView];
      initialFrame = finalFrame;
      finalFrame.origin.y = -finalFrame.size.height;
      animations = ^void {
        [presentingView setFrame:finalFrame];
      };
      break;
    case DISMISS:
      [containerView insertSubview:presentedView aboveSubview:presentingView];
      initialFrame = finalFrame;
      initialFrame.origin.y = -initialFrame.size.height;
      animations = ^void {
        [presentedView setFrame:finalFrame];
      };
      break;
  }

  // Set the frame for the presented view.
  if (!CGRectIsEmpty(initialFrame)) {
    [presentedView setFrame:initialFrame];
  }
  [presentedView layoutIfNeeded];

  void (^completion)(BOOL) = ^void(BOOL finished) {
    [transitionContext completeTransition:finished];
  };

  // Animate the transition.
  [UIView animateWithDuration:kDefaultDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:animations
                   completion:completion];
}

- (void)animationEnded:(BOOL)transitionCompleted {
  return;
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return kDefaultDuration;
}

@end

@implementation QRScannerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  return [[[QRScannerTransitionAnimator alloc] initWithTransition:PRESENT]
      autorelease];
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[[QRScannerTransitionAnimator alloc] initWithTransition:DISMISS]
      autorelease];
}

@end
