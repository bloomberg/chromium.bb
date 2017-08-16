// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/root/root_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/root/root_container_view_controller.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RootCoordinator ()
@property(nonatomic, strong) RootContainerViewController* viewController;
@property(nonatomic, weak) TabGridCoordinator* tabGridCoordinator;
@end

@implementation RootCoordinator
@synthesize viewController = _viewController;
@synthesize tabGridCoordinator = _tabGridCoordinator;

#pragma mark - BrowserCoordinator

- (void)start {
  self.viewController = [[RootContainerViewController alloc] init];

  TabGridCoordinator* tabGridCoordinator = [[TabGridCoordinator alloc] init];
  [self addChildCoordinator:tabGridCoordinator];
  [tabGridCoordinator start];
  self.tabGridCoordinator = tabGridCoordinator;

  [super start];
}

- (void)stop {
  [super stop];
  // PLACEHOLDER: Remove child coordinators here for now. This might be handled
  // differently later on.
  for (BrowserCoordinator* child in self.children) {
    [self removeChildCoordinator:child];
  }
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  self.viewController.contentViewController = childCoordinator.viewController;
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  self.viewController.contentViewController = nil;
}

- (BOOL)canAddOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator {
  return YES;
}

#pragma mark - URLOpening

- (void)openURL:(NSURL*)URL {
  [self.tabGridCoordinator openURL:URL];
}

@end
