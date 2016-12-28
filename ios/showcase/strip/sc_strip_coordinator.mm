// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/strip/sc_strip_coordinator.h"

#import "ios/chrome/browser/ui/actions/tab_strip_actions.h"
#import "ios/chrome/browser/ui/strip/strip_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCStripCoordinator ()
@property(nonatomic, strong) StripContainerViewController* viewController;
@end

@implementation SCStripCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;

- (void)start {
  UIViewController* blackViewController = [[UIViewController alloc] init];
  blackViewController.view.backgroundColor = [UIColor blackColor];

  UIViewController* greenViewController =
      [self viewControllerWithButtonTitle:@"toggleStrip"
                                   action:@selector(toggleTabStrip:)];
  greenViewController.view.backgroundColor = [UIColor greenColor];

  self.viewController = [[StripContainerViewController alloc] init];
  self.viewController.title = @"Tab strip container";
  self.viewController.stripViewController = blackViewController;
  self.viewController.contentViewController = greenViewController;
  self.baseViewController.navigationBar.translucent = NO;
  [self.baseViewController pushViewController:self.viewController animated:YES];
}

- (UIViewController*)viewControllerWithButtonTitle:(NSString*)title
                                            action:(SEL)action {
  UIViewController* viewController = [[UIViewController alloc] init];
  UIButton* button = [UIButton buttonWithType:UIButtonTypeRoundedRect];
  [button setFrame:CGRectMake(10, 10, 80, 50)];
  [button setTitle:title forState:UIControlStateNormal];
  [viewController.view addSubview:button];
  [button addTarget:nil
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return viewController;
}

@end
