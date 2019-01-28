// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/manage_sync_settings_view_controller_model_delegate.h"

// Mediator for the manager sync settings.
@interface ManageSyncSettingsMediator
    : NSObject <ManageSyncSettingsTableViewControllerModelDelegate>

// Consumer.
@property(nonatomic, weak) id<ManageSyncSettingsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_MANAGE_SYNC_SETTINGS_MEDIATOR_H_
