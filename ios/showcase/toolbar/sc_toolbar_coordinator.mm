// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/toolbar/sc_toolbar_coordinator.h"

#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_strip_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Toolbar height.
CGFloat kToolbarHeight = 50.0f;
}  // namespace

@interface SCToolbarCoordinator ()
@property(nonatomic, strong) ProtocolAlerter* alerter;
@end

@implementation SCToolbarCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize alerter = _alerter;

- (void)start {
  self.alerter = [[ProtocolAlerter alloc] initWithProtocols:@[
    @protocol(NavigationCommands), @protocol(TabGridCommands),
    @protocol(TabStripCommands), @protocol(ToolsMenuCommands)
  ]];
  self.alerter.baseViewController = self.baseViewController;

  UIViewController* containerViewController = [[UIViewController alloc] init];
  containerViewController.view.backgroundColor = [UIColor whiteColor];
  containerViewController.title = @"Toolbar";

  UIView* containerView = [[UIView alloc] init];
  [containerViewController.view addSubview:containerView];
  containerView.backgroundColor = [UIColor redColor];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;

  id dispatcher =
      static_cast<id<NavigationCommands, TabGridCommands, TabStripCommands,
                     ToolsMenuCommands>>(self.alerter);
  ToolbarViewController* toolbarViewController =
      [[ToolbarViewController alloc] initWithDispatcher:dispatcher];
  [containerViewController addChildViewController:toolbarViewController];
  toolbarViewController.view.frame = containerView.frame;
  [containerView addSubview:toolbarViewController.view];
  [toolbarViewController didMoveToParentViewController:containerViewController];

  [NSLayoutConstraint activateConstraints:@[
    [containerView.heightAnchor constraintEqualToConstant:kToolbarHeight],
    [containerView.leadingAnchor
        constraintEqualToAnchor:containerViewController.view.leadingAnchor],
    [containerView.trailingAnchor
        constraintEqualToAnchor:containerViewController.view.trailingAnchor],
    [containerView.centerYAnchor
        constraintEqualToAnchor:containerViewController.view.centerYAnchor],
  ]];

  [self.baseViewController pushViewController:containerViewController
                                     animated:YES];
}

@end
