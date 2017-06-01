// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_animator.h"
#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_controller.h"
#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolsCoordinator ()<UIViewControllerTransitioningDelegate>
@property(nonatomic, strong) MenuViewController* viewController;
@property(nonatomic, strong) ToolsMediator* mediator;
@end

@implementation ToolsCoordinator
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;
@synthesize toolsMenuConfiguration = _toolsMenuConfiguration;
@synthesize webState = _webState;

#pragma mark - BrowserCoordinator

- (void)start {
  self.viewController = [[MenuViewController alloc] init];
  self.viewController.modalPresentationStyle = UIModalPresentationCustom;
  self.viewController.transitioningDelegate = self;
  self.viewController.dispatcher = static_cast<id>(self.browser->dispatcher());
  self.mediator =
      [[ToolsMediator alloc] initWithConsumer:self.viewController
                                configuration:self.toolsMenuConfiguration];
  if (self.webState) {
    self.mediator.webState = self.webState;
  }
  [super start];
}

#pragma mark - Setters

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  if (self.mediator) {
    self.mediator.webState = self.webState;
  }
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
