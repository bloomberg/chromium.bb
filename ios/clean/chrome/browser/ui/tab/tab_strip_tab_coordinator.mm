// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_strip_tab_coordinator.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/tab_strip_commands.h"
#import "ios/clean/chrome/browser/ui/tab/tab_strip_tab_container_view_controller.h"
#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_coordinator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripTabCoordinator ()
// Redefine |viewController| with a subclass.
@property(nonatomic, strong) TabStripTabContainerViewController* viewController;
// Defined in super class private category.
@property(nonatomic, weak) CleanToolbarCoordinator* toolbarCoordinator;
@end

@implementation TabStripTabCoordinator
@dynamic viewController;
@dynamic toolbarCoordinator;

#pragma mark - TabCoordinator overrides

// Return a tab container with a tab strip.
- (TabContainerViewController*)newTabContainer {
  return [[TabStripTabContainerViewController alloc] init];
}

#pragma mark - BrowserCoordinator

- (void)start {
  if (self.started)
    return;
  TabStripCoordinator* tabStripCoordinator = [[TabStripCoordinator alloc] init];
  [self addChildCoordinator:tabStripCoordinator];
  [tabStripCoordinator start];
  [self.dispatcher startDispatchingToTarget:self
                                forSelector:@selector(showTabStrip)];

  [super start];

  [self.browser->broadcaster()
      broadcastValue:@"tabStripVisible"
            ofObject:self.viewController
            selector:@selector(broadcastTabStripVisible:)];
}

- (void)stop {
  [super stop];
  [self.dispatcher stopDispatchingToTarget:self];
  [self.browser->broadcaster()
      stopBroadcastingForSelector:@selector(broadcastTabStripVisible:)];
}

- (void)addChildCoordinator:(BrowserCoordinator*)childCoordinator {
  [super addChildCoordinator:childCoordinator];
  if ([childCoordinator isKindOfClass:[CleanToolbarCoordinator class]]) {
    self.toolbarCoordinator.usesTabStrip = YES;
  }
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  [super childCoordinatorDidStart:childCoordinator];
  if ([childCoordinator isKindOfClass:[TabStripCoordinator class]]) {
    self.viewController.tabStripViewController =
        childCoordinator.viewController;
  }
}

#pragma mark - TabStripCommands

- (void)showTabStrip {
  self.viewController.tabStripVisible = YES;
}

@end
