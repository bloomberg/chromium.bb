// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"

#include "base/logging.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/google/core/common/google_util.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManageSyncSettingsCoordinator () <
    ManageSyncSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate>

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;
// Mediator.
@property(nonatomic, strong) ManageSyncSettingsMediator* mediator;

@end

@implementation ManageSyncSettingsCoordinator

- (void)start {
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  self.mediator =
      [[ManageSyncSettingsMediator alloc] initWithSyncService:syncService];
  self.mediator.syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(self.browserState);
  self.mediator.commandHandler = self;
  self.viewController = [[ManageSyncSettingsTableViewController alloc]
      initWithTableViewStyle:UITableViewStyleGrouped
                 appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;
  self.viewController.modelDelegate = self.mediator;
  self.mediator.consumer = self.viewController;
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

#pragma mark - ManageSyncSettingsCommandHandler

- (void)openPassphraseDialog {
  DCHECK(self.mediator.shouldEncryptionItemBeEnabled);
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowserState:self.browserState];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowserState:self.browserState];
  }
  controllerToPush.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controllerToPush animated:YES];
}

- (void)openWebAppActivityDialog {
}

- (void)openDataFromChromeSyncWebPage {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

@end
