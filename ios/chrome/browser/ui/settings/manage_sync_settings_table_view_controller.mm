// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/manage_sync_settings_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/manage_sync_settings_view_controller_model_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ManageSyncSettingsTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      @"manage_sync_settings_view_controller";
  self.title = l10n_util::GetNSString(IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self reloadData];
}

#pragma mark - SettingsControllerProtocol

- (void)viewControllerWasPopped {
  [self.presentationDelegate
      manageSyncSettingsTableViewControllerWasPopped:self];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self.modelDelegate manageSyncSettingsTableViewControllerLoadModel:self];
}

@end
