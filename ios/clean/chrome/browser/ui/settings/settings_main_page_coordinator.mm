// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/settings/settings_main_page_coordinator.h"

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/clean/chrome/browser/ui/settings/material_cell_catalog_coordinator.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/shared/chrome/browser/ui/settings/settings_main_page_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsMainPageCoordinator ()<SettingsMainPageCommands>
@property(nonatomic, strong) SettingsCollectionViewController* viewController;
@end

@implementation SettingsMainPageCoordinator
@synthesize viewController = _viewController;

- (void)start {
  DCHECK(!self.browser->browser_state()->IsOffTheRecord());
  // TODO(crbug.com/738881): Clean up the dispatcher mess here.
  self.viewController = [[SettingsCollectionViewController alloc]
      initWithBrowserState:self.browser->browser_state()
                dispatcher:nil];
  [self.browser->dispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(SettingsMainPageCommands)];
  self.viewController.settingsMainPageDispatcher =
      static_cast<id<SettingsMainPageCommands>>(self.browser->dispatcher());
  [super start];
}

#pragma mark - SettingsMainPageViewControllerDelegate

- (void)showMaterialCellCatalog {
  MaterialCellCatalogCoordinator* coordinator =
      [[MaterialCellCatalogCoordinator alloc] init];
  [self addChildCoordinator:coordinator];
  [coordinator start];
}

#pragma mark - BrowserCoordinator

- (void)childCoordinatorDidStart:(BrowserCoordinator*)coordinator {
  [self.viewController.navigationController
      pushViewController:coordinator.viewController
                animated:YES];
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  [self.viewController.navigationController
      popToViewController:self.viewController
                 animated:YES];
}

@end
