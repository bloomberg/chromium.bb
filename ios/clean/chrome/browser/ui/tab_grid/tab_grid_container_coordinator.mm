// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_container_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/settings/settings_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_container_view_controller.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_toolbar_commands.h"
#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridContainerCoordinator ()<SettingsCommands,
                                          TabGridToolbarCommands>

// Tab grid handling the non-incognito tabs.
@property(nonatomic, strong) TabGridCoordinator* normalTabGrid;

// ViewController containing the toolbar and the currently presented TabGrid.
@property(nonatomic, strong) TabGridContainerViewController* viewController;

// Coordinator handling the settings.
@property(nonatomic, weak) SettingsCoordinator* settingsCoordinator;
// Coordinator handling the toolmenu.
@property(nonatomic, weak) ToolsCoordinator* toolsMenuCoordinator;

@end

@implementation TabGridContainerCoordinator

@synthesize viewController = _viewController;
@synthesize normalTabGrid = _normalTabGrid;
@synthesize settingsCoordinator = _settingsCoordinator;
@synthesize toolsMenuCoordinator = _toolsMenuCoordinator;

- (void)start {
  if (self.started)
    return;

  [self.dispatcher startDispatchingToTarget:self
                                forSelector:@selector(showSettings)];

  self.viewController = [[TabGridContainerViewController alloc] init];
  self.viewController.dispatcher = self;
  self.normalTabGrid = [[TabGridCoordinator alloc] init];
  [self addChildCoordinator:self.normalTabGrid];
  [self.normalTabGrid start];

  [super start];
}

- (void)stop {
  [super stop];
  [self.dispatcher stopDispatchingForSelector:@selector(showSettings)];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  if (childCoordinator == self.normalTabGrid) {
    self.viewController.tabGrid = childCoordinator.viewController;
  } else if (childCoordinator == self.toolsMenuCoordinator ||
             childCoordinator == self.settingsCoordinator) {
    [self.viewController presentViewController:childCoordinator.viewController
                                      animated:YES
                                    completion:nil];
  } else {
    NOTREACHED();
  }
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  if (childCoordinator == self.toolsMenuCoordinator ||
      childCoordinator == self.settingsCoordinator) {
    [childCoordinator.viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  } else if (childCoordinator == self.normalTabGrid) {
  } else {
    NOTREACHED();
  }
}

#pragma mark - URLOpening

- (void)openURL:(NSURL*)URL {
  [self.normalTabGrid openURL:URL];
}

#pragma mark - TabGridToolbarCommands

- (void)showToolsMenu {
  [self.dispatcher startDispatchingToTarget:self
                                forSelector:@selector(closeToolsMenu)];
  ToolsCoordinator* toolsCoordinator = [[ToolsCoordinator alloc] init];
  [self addChildCoordinator:toolsCoordinator];
  ToolsMenuConfiguration* menuConfiguration =
      [[ToolsMenuConfiguration alloc] initWithDisplayView:nil];
  menuConfiguration.inTabSwitcher = YES;
  menuConfiguration.noOpenedTabs = self.browser->web_state_list().empty();
  menuConfiguration.inNewTabPage = NO;
  toolsCoordinator.toolsMenuConfiguration = menuConfiguration;
  self.toolsMenuCoordinator = toolsCoordinator;
  [toolsCoordinator start];
}

- (void)closeToolsMenu {
  [self.dispatcher stopDispatchingForSelector:@selector(closeToolsMenu)];
  [self.toolsMenuCoordinator stop];
  [self removeChildCoordinator:self.toolsMenuCoordinator];
}

- (void)toggleIncognito {
  // TODO(crbug.com/762578): implement this.
}

#pragma mark - SettingsCommands

- (void)showSettings {
  [self.dispatcher startDispatchingToTarget:self
                                forSelector:@selector(closeSettings)];
  SettingsCoordinator* settingsCoordinator = [[SettingsCoordinator alloc] init];
  [self addChildCoordinator:settingsCoordinator];
  self.settingsCoordinator = settingsCoordinator;
  [settingsCoordinator start];
}

- (void)closeSettings {
  [self.dispatcher stopDispatchingForSelector:@selector(closeSettings)];
  [self.settingsCoordinator stop];
  [self removeChildCoordinator:self.settingsCoordinator];
  // self.settingsCoordinator should be presumed to be nil after this point.
}

@end
