// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab/sc_top_toolbar_tab_coordinator.h"

#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCTopToolbarTabCoordinator

@synthesize baseViewController;

- (void)start {
  UIViewController* topToolbarTabViewController =
      [[TopToolbarTabViewController alloc] init];
  topToolbarTabViewController.title = @"Top toolbar tab";
  [self.baseViewController pushViewController:topToolbarTabViewController
                                     animated:YES];
}

@end
