// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/root/sc_root_coordinator.h"

#import "ios/clean/chrome/browser/ui/root/root_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCRootCoordinator

@synthesize baseViewController;

- (void)start {
  RootContainerViewController* viewController =
      [[RootContainerViewController alloc] init];
  UIViewController* contentViewController = [[UIViewController alloc] init];
  contentViewController.view.backgroundColor = [UIColor greenColor];
  viewController.contentViewController = contentViewController;
  viewController.title = @"Root container";
  [self.baseViewController pushViewController:viewController animated:YES];
}

@end
