// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NATIVE_APPS_COLLECTION_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NATIVE_APPS_COLLECTION_VIEW_CONTROLLER_PRIVATE_H_

#import "ios/chrome/browser/ui/settings/native_apps_collection_view_controller.h"

// This file is a private header for NativeAppsCollectionViewController.
// It exposes private details for testing purposes.

namespace settings {

// User actions.
typedef enum {
  kNativeAppsActionDidNothing,
  kNativeAppsActionClickedInstall,
  kNativeAppsActionTurnedDefaultBrowserOn,
  kNativeAppsActionTurnedDefaultBrowserOff,
  kNativeAppsActionTurnedAutoOpenOn,
  kNativeAppsActionTurnedAutoOpenOff,
  kNativeAppsActionCount,
} NativeAppsAction;

}  // namespace settings

// Arbitrary value to shift indices (from 0 to N), to a range that doesn't
// include 0, in order to not mistake the default value (0) with the first
// index. That way, an invalid (or default) tag value can be detected.
// That means that all app cells have a tag computed by adding their index and
// this shift.
extern const NSInteger kTagShift;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NATIVE_APPS_COLLECTION_VIEW_CONTROLLER_PRIVATE_H_
