// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/manage_sync_settings_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/settings/manage_sync_settings_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManageSyncSettingsCoordinator () <
    ManageSyncSettingsTableViewControllerPresentationDelegate>

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;

@end

@implementation ManageSyncSettingsCoordinator

- (void)start {
  self.viewController = [[ManageSyncSettingsTableViewController alloc]
      initWithTableViewStyle:UITableViewStyleGrouped
                 appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  self.viewController.presentationDelegate = self;
  DCHECK(self.navigationController);
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
}

#pragma mark - ManageSyncSettingsTableViewControllerPresentationDelegate

- (void)manageSyncSettingsTableViewControllerWasPopped:
    (ManageSyncSettingsTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate manageSyncSettingsCoordinatorWasPopped:self];
}

@end
