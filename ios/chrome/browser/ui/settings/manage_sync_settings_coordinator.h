// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class ManageSyncSettingsCoordinator;

// Delegate for ManageSyncSettingsCoordinator.
@protocol ManageSyncSettingsCoordinatorDelegate <NSObject>

// Called when the view controller is popped out from navigation controller.
- (void)manageSyncSettingsCoordinatorWasPopped:
    (ManageSyncSettingsCoordinator*)coordinator;

@end

// Coordinator for the Manage Sync Settings TableView Controller.
@interface ManageSyncSettingsCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<ManageSyncSettingsCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
