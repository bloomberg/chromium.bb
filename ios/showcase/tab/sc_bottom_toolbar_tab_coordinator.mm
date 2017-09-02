// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab/sc_bottom_toolbar_tab_coordinator.h"

#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCBottomToolbarTabCoordinator

@synthesize baseViewController;

- (void)start {
  TabContainerViewController* bottomToolbarTabViewController =
      [[TabContainerViewController alloc] init];
  bottomToolbarTabViewController.usesBottomToolbar = YES;
  bottomToolbarTabViewController.title = @"Bottom toolbar tab";

  UIViewController* toolbar = [[UIViewController alloc] init];
  toolbar.view.backgroundColor = [UIColor greenColor];
  bottomToolbarTabViewController.toolbarViewController = toolbar;

  UIViewController* content = [[UIViewController alloc] init];
  content.view.backgroundColor = [UIColor whiteColor];
  bottomToolbarTabViewController.contentViewController = content;

  [self.baseViewController pushViewController:bottomToolbarTabViewController
                                     animated:YES];
}

@end
