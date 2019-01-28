// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/manage_sync_settings_consumer.h"

@class ManageSyncSettingsTableViewController;
@protocol ManageSyncSettingsTableViewControllerModelDelegate;

// Delegate for presentation events related to
// ManageSyncSettingsTableViewController.
@protocol ManageSyncSettingsTableViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)manageSyncSettingsTableViewControllerWasPopped:
    (ManageSyncSettingsTableViewController*)controller;

@end

// View controller to related to Manage sync settings view.
@interface ManageSyncSettingsTableViewController
    : SettingsRootTableViewController <ManageSyncSettingsConsumer>

// Presentation delegate.
@property(nonatomic, weak)
    id<ManageSyncSettingsTableViewControllerPresentationDelegate>
        presentationDelegate;
@property(nonatomic, weak)
    id<ManageSyncSettingsTableViewControllerModelDelegate>
        modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_H_
