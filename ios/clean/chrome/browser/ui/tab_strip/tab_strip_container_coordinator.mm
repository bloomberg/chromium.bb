// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_container_coordinator.h"

#include <memory>

#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/clean/chrome/browser/ui/animators/zoom_transition_animator.h"
#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_container_view_controller.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripContainerCoordinator ()<
    UIViewControllerTransitioningDelegate>
@property(nonatomic, strong) TabStripContainerViewController* viewController;
@end

@implementation TabStripContainerCoordinator

@synthesize presentationKey = _presentationKey;
@synthesize viewController = _viewController;
@synthesize webState = _webState;

- (void)start {
  self.viewController = [[TabStripContainerViewController alloc] init];
  self.viewController.transitioningDelegate = self;
  self.viewController.modalPresentationStyle = UIModalPresentationCustom;

  TabCoordinator* tabCoordinator = [[TabCoordinator alloc] init];
  tabCoordinator.webState = self.webState;
  [self addChildCoordinator:tabCoordinator];
  // Unset the base view controller, so |tabCoordinator| doesn't present
  // its view controller.
  tabCoordinator.context.baseViewController = nil;
  [tabCoordinator start];

  // PLACEHOLDER: Replace this placeholder with an actual tab strip view
  // controller.
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button addTarget:nil
                action:@selector(showTabGrid:)
      forControlEvents:UIControlEventTouchUpInside];
  [button setTitle:@"Tab grid" forState:UIControlStateNormal];
  button.frame = CGRectMake(10, 10, 100, 100);

  UIViewController* tabStripViewController = [[UIViewController alloc] init];
  tabStripViewController.view.backgroundColor = [UIColor blackColor];
  [tabStripViewController.view addSubview:button];
  self.viewController.tabStripViewController = tabStripViewController;
  self.viewController.contentViewController = tabCoordinator.viewController;

  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:self.context.animated
                         completion:nil];
  self.viewController = nil;
}

- (BOOL)canAddOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator {
  // This coordinator will always accept overlay coordinators.
  return YES;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  ZoomTransitionAnimator* animator = [[ZoomTransitionAnimator alloc] init];
  animator.presenting = YES;
  animator.presentationKey = self.presentationKey;
  [animator selectDelegate:@[ source, presenting ]];
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  ZoomTransitionAnimator* animator = [[ZoomTransitionAnimator alloc] init];
  animator.presenting = NO;
  animator.presentationKey = self.presentationKey;
  [animator selectDelegate:@[ dismissed.presentingViewController ]];
  return animator;
}

@end
