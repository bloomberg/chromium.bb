// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/settings/settings_coordinator.h"

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsCoordinator ()<SettingsNavigationControllerDelegate>
@property(nonatomic, strong) SettingsNavigationController* viewController;
@end

@implementation SettingsCoordinator
@synthesize viewController = _viewController;

#pragma mark - BrowserCoordinator

- (void)start {
  self.viewController = [SettingsNavigationController
      newSettingsMainControllerWithMainBrowserState:self.browser
                                                        ->browser_state()
                                           delegate:self];
  [super start];
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
  [static_cast<id>(self.browser->dispatcher()) closeSettings];
}

@end
