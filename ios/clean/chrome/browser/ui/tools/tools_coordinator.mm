// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_animator.h"
#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_controller.h"
#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolsCoordinator ()<UIViewControllerTransitioningDelegate>
@property(nonatomic, strong) MenuViewController* menuViewController;
@end

@implementation ToolsCoordinator
@synthesize toolbarCommandHandler = _toolbarCommandHandler;
@synthesize menuViewController = _menuViewController;

#pragma mark - BrowserCoordinator

- (void)start {
  self.menuViewController = [[MenuViewController alloc] init];
  self.menuViewController.modalPresentationStyle = UIModalPresentationCustom;
  self.menuViewController.transitioningDelegate = self;

  [self.context.baseViewController presentViewController:self.menuViewController
                                                animated:self.context.animated
                                              completion:nil];
}

- (void)stop {
  [self.menuViewController.presentingViewController
      dismissViewControllerAnimated:self.context.animated
                         completion:nil];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  ZoomTransitionAnimator* animator = [[ZoomTransitionAnimator alloc] init];
  animator.presenting = YES;
  [animator selectDelegate:@[ source, presenting ]];
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  ZoomTransitionAnimator* animator = [[ZoomTransitionAnimator alloc] init];
  animator.presenting = NO;
  [animator selectDelegate:@[ dismissed.presentingViewController ]];
  return animator;
}

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  MenuPresentationController* menuPresentation =
      [[MenuPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  menuPresentation.toolbarCommandHandler = self.toolbarCommandHandler;
  return menuPresentation;
}

@end
