// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_animator.h"
#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_controller.h"
#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolsCoordinator ()<UIViewControllerTransitioningDelegate>
@property(nonatomic, strong) MenuViewController* menuViewController;
@property(nonatomic, strong) ToolsMediator* mediator;
@end

@implementation ToolsCoordinator
@synthesize menuViewController = _menuViewController;
@synthesize mediator = _mediator;

#pragma mark - BrowserCoordinator

- (void)start {
  self.menuViewController = [[MenuViewController alloc] init];
  self.menuViewController.modalPresentationStyle = UIModalPresentationCustom;
  self.menuViewController.transitioningDelegate = self;
  self.menuViewController.dispatcher =
      static_cast<id>(self.browser->dispatcher());
  _mediator = [[ToolsMediator alloc] initWithConsumer:self.menuViewController];

  [self.context.baseViewController presentViewController:self.menuViewController
                                                animated:self.context.animated
                                              completion:nil];
  [super start];
}

- (void)stop {
  [super stop];
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
  menuPresentation.dispatcher = static_cast<id>(self.browser->dispatcher());
  return menuPresentation;
}

@end
