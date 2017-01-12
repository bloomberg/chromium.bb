// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_animator.h"

#include "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ZoomTransitionAnimator
@synthesize presenting = _presenting;
@synthesize presentationKey = _presentationKey;
@synthesize delegate = _delegate;

#pragma mark - Public API

- (void)selectDelegate:(NSArray<id<NSObject>>*)possibleDelegates {
  for (id<NSObject> obj in possibleDelegates) {
    if ([obj conformsToProtocol:@protocol(ZoomTransitionDelegate)]) {
      self.delegate = static_cast<id<ZoomTransitionDelegate>>(obj);
      return;
    }
  }
  self.delegate = nil;
}

#pragma mark - UIViewControllerAnimatedTransitioning

- (void)animateTransition:(id<UIViewControllerContextTransitioning>)context {
  UIView* presentedView;
  UIView* presentingView;
  UIViewController* presentedViewController;
  UIViewController* presentingViewController;

  // Map the keyed view/view controllers in |context| to local vars.
  if (self.presenting) {
    presentedView = [context viewForKey:UITransitionContextToViewKey];
    presentingView = [context viewForKey:UITransitionContextFromViewKey];
    presentedViewController =
        [context viewControllerForKey:UITransitionContextToViewControllerKey];
    presentingViewController =
        [context viewControllerForKey:UITransitionContextFromViewControllerKey];
  } else {
    presentedView = [context viewForKey:UITransitionContextFromViewKey];
    presentingView = [context viewForKey:UITransitionContextToViewKey];
    presentedViewController =
        [context viewControllerForKey:UITransitionContextFromViewControllerKey];
    presentingViewController =
        [context viewControllerForKey:UITransitionContextToViewControllerKey];
  }

  // Figure out what to use as the "zoom rectangle" -- the rectangle that the
  // presentation or dismissal will move from or to.
  CGRect rect = CGRectNull;
  UIView* zoomRectView = presentingViewController.view;
  if (self.delegate) {
    rect = [self.delegate rectForZoomWithKey:self.presentationKey
                                      inView:zoomRectView];
  }

  // If no usable rectangle was provided, use a rectangle that's 1/5 of the
  // presenting view's smaller dimension square, positioned in its center.
  if (CGRectIsNull(rect)) {
    CGRect presenterRect = zoomRectView.bounds;
    CGFloat side = std::min(CGRectGetHeight(presenterRect),
                            CGRectGetWidth(presenterRect)) /
                   5.0;

    rect = CGRectMake(CGRectGetMidX(presenterRect) - (side / 2.0),
                      CGRectGetMidY(presenterRect) - (side / 2.0), side, side);
  }

  // Convert that rect into the coordinate space of the container view.
  CGRect zoomRect =
      [context.containerView convertRect:rect fromView:zoomRectView];

  // Set up initial and final values for the animation.
  CGFloat finalAlpha;
  CGRect finalRect;
  if (self.presenting) {
    // In this case, the view being presented isn't yet in a view hierarchy, so
    // it should be added in its initial position (|zoomRect|) and opacity.
    presentedView.frame = zoomRect;
    [context.containerView addSubview:presentedView];
    presentedView.alpha = 0.0;
    // The presented view will finish the animation at the frame vended by
    // |context|, at full opacity.
    finalAlpha = 1.0;
    finalRect = [context finalFrameForViewController:presentedViewController];
  } else {
    // When dismissing, the presented view is already covering the window;
    // the presenting view needs to be added underneath it (at the frame vended
    // by |context|.
    presentingView.frame =
        [context finalFrameForViewController:presentingViewController];
    [context.containerView insertSubview:presentingView
                            belowSubview:presentedView];
    // The presented view will end up faded out, located at |zoomRect|.
    finalAlpha = 0.0;
    finalRect = zoomRect;
  }

  [UIView animateWithDuration:[self transitionDuration:context]
      animations:^{
        presentedView.frame = finalRect;
        presentedView.alpha = finalAlpha;
        [presentedView layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (finished && !self.presenting) {
          [presentedView removeFromSuperview];
        }
        [context completeTransition:finished];
      }];
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)context {
  return 0.35;
}

@end
