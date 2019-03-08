// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_animator.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_presentation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation InfobarBannerTransitionDriver

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  InfobarBannerPresentationController* presentationController =
      [[InfobarBannerPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  InfobarBannerAnimator* animator = [[InfobarBannerAnimator alloc] init];
  animator.presenting = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  InfobarBannerAnimator* animator = [[InfobarBannerAnimator alloc] init];
  animator.presenting = NO;
  return animator;
}

@end
