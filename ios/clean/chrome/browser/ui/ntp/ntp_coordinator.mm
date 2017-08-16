// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/clean/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"
#import "ios/clean/chrome/browser/ui/commands/ntp_commands.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_coordinator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_mediator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_view_controller.h"
#import "ios/clean/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPCoordinator ()<NTPCommands>
@property(nonatomic, strong) NTPMediator* mediator;
@property(nonatomic, strong) NTPViewController* viewController;
@end

@implementation NTPCoordinator
@synthesize mediator = _mediator;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[NTPViewController alloc] init];
  self.mediator = [[NTPMediator alloc] initWithConsumer:self.viewController];

  CommandDispatcher* dispatcher = self.browser->dispatcher();
  // NTPCommands
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showNTPHomePanel)];
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showNTPBookmarksPanel)];
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showNTPRecentTabsPanel)];
  self.viewController.dispatcher = static_cast<id>(self.browser->dispatcher());
  [super start];
}

- (void)stop {
  [super stop];
  [self.browser->dispatcher() stopDispatchingToTarget:self];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)coordinator {
  if ([coordinator isKindOfClass:[NTPHomeCoordinator class]]) {
    self.viewController.homeViewController = coordinator.viewController;

  } else if ([coordinator isKindOfClass:[BookmarksCoordinator class]]) {
    if (IsIPadIdiom()) {
      self.viewController.bookmarksViewController = coordinator.viewController;
    } else {
      [self.viewController presentViewController:coordinator.viewController
                                        animated:YES
                                      completion:nil];
    }

  } else if ([coordinator isKindOfClass:[RecentTabsCoordinator class]]) {
    if (IsIPadIdiom()) {
      self.viewController.recentTabsViewController = coordinator.viewController;
    } else {
      [self.viewController presentViewController:coordinator.viewController
                                        animated:YES
                                      completion:nil];
    }
  }
}

#pragma mark - NTPCommands

- (void)showNTPHomePanel {
  NTPHomeCoordinator* panelCoordinator = [[NTPHomeCoordinator alloc] init];
  [self addChildCoordinator:panelCoordinator];
  [panelCoordinator start];
}

- (void)showNTPBookmarksPanel {
  // TODO(crbug.com/740793): Remove alert once this feature is implemented.
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Bookmarks"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.viewController presentViewController:alertController
                                    animated:YES
                                  completion:nil];
}

- (void)showNTPRecentTabsPanel {
  // TODO(crbug.com/740793): Remove alert once this feature is implemented.
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Recent Sites"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.viewController presentViewController:alertController
                                    animated:YES
                                  completion:nil];
}

@end
