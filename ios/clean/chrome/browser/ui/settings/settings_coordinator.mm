// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/settings/settings_coordinator.h"

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsCoordinator ()<SettingsNavigationControllerDelegate>
@property(nonatomic, strong) SettingsNavigationController* viewController;
@end

@implementation SettingsCoordinator
@synthesize settingsCommandHandler = _settingsCommandHandler;
@synthesize viewController = _viewController;

#pragma mark - BrowserCoordinator

- (void)start {
  self.viewController = [SettingsNavigationController
      newSettingsMainControllerWithMainBrowserState:self.browserState
                                currentBrowserState:self.browserState
                                           delegate:self];
  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:self.context.animated
                         completion:nil];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettingsAndOpenUrl:(OpenUrlCommand*)command {
  // Placeholder implementation to conform to the delegate protocol;
  // for now this just closes the settings without opening a URL.
  [self closeSettings];
}

- (void)closeSettingsAndOpenNewIncognitoTab {
  // Placeholder implementation to conform to the delegate protocol;
  // for now this just closes the settings without opening a new tab.
  [self closeSettings];
}

- (void)closeSettings {
  [self.settingsCommandHandler closeSettings];
}

@end
