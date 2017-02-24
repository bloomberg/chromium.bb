// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tab_strip_actions.h"
#import "ios/clean/chrome/browser/ui/animators/zoom_transition_animator.h"
#import "ios/clean/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_coordinator.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Placeholder "experiment" flag. Change this to YES to have the toolbar at the
// bottom.
const BOOL kUseBottomToolbar = NO;
}  // namespace

@interface TabCoordinator ()<CRWWebStateObserver,
                             UIViewControllerTransitioningDelegate>
@property(nonatomic, strong) TabContainerViewController* viewController;
@end

@implementation TabCoordinator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

@synthesize presentationKey = _presentationKey;
@synthesize viewController = _viewController;
@synthesize webState = _webState;

- (void)start {
  self.viewController = [self newTabContainer];
  self.viewController.transitioningDelegate = self;
  self.viewController.modalPresentationStyle = UIModalPresentationCustom;
  _webStateObserver =
      base::MakeUnique<web::WebStateObserverBridge>(self.webState, self);

  WebCoordinator* webCoordinator = [[WebCoordinator alloc] init];
  webCoordinator.webState = self.webState;
  [self addChildCoordinator:webCoordinator];
  // Unset the base view controller, so |webCoordinator| doesn't present its
  // view controller.
  webCoordinator.context.baseViewController = nil;
  [webCoordinator start];

  ToolbarCoordinator* toolbarCoordinator = [[ToolbarCoordinator alloc] init];
  toolbarCoordinator.webState = self.webState;
  [self addChildCoordinator:toolbarCoordinator];

  // Unset the base view controller, so |toolbarCoordinator| doesn't present
  // its view controller.
  toolbarCoordinator.context.baseViewController = nil;
  [toolbarCoordinator start];

  self.viewController.toolbarViewController = toolbarCoordinator.viewController;
  self.viewController.contentViewController = webCoordinator.viewController;

  // PLACEHOLDER: Replace this placeholder with an actual tab strip view
  // controller.
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button addTarget:nil
                action:@selector(hideTabStrip:)
      forControlEvents:UIControlEventTouchUpInside];
  [button setTitle:@"Hide Strip" forState:UIControlStateNormal];
  button.frame = CGRectMake(10, 10, 100, 100);

  UIViewController* tabStripViewController = [[UIViewController alloc] init];
  tabStripViewController.view.backgroundColor = [UIColor blackColor];
  [tabStripViewController.view addSubview:button];
  self.viewController.tabStripViewController = tabStripViewController;

  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:self.context.animated
                         completion:nil];
  _webStateObserver.reset();
}

- (BOOL)canAddOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator {
  // This coordinator will always accept overlay coordinators.
  return YES;
}

#pragma mark - Experiment support

// Create and return a new view controller for use as a tab container;
// experimental configurations determine which subclass of
// TabContainerViewController to return.
- (TabContainerViewController*)newTabContainer {
  if (kUseBottomToolbar) {
    return [[BottomToolbarTabViewController alloc] init];
  }
  return [[TopToolbarTabViewController alloc] init];
}

#pragma mark - CRWWebStateObserver

// This will eventually be called in -didFinishNavigation and perhaps as an
// optimization in some equivalent to loadURL.
- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:(const web::LoadCommittedDetails&)details {
  if (webState->GetLastCommittedURL() == GURL("chrome://newtab/")) {
    NTPCoordinator* ntpCoordinator = [[NTPCoordinator alloc] init];
    [self addChildCoordinator:ntpCoordinator];
    ntpCoordinator.context.baseViewController = nil;
    [ntpCoordinator start];
    self.viewController.contentViewController = ntpCoordinator.viewController;
  }
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
