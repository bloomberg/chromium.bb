// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/browser_coordinator.h"

@protocol SettingsCommands;

// A coordinator for the Settings UI, which is usually presented modally
// on top of whatever other UI is currently active.
@interface SettingsCoordinator : BrowserCoordinator
// Action delegate for this coordinator.
@property(nonatomic, weak) id<SettingsCommands> settingsCommandHandler;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_COORDINATOR_H_
