// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator.h"

// A coordinator for the Settings UI, which is usually presented modally
// on top of whatever other UI is currently active.
@interface SettingsCoordinator : BrowserCoordinator
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_COORDINATOR_H_
