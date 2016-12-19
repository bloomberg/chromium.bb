// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PHYSICAL_WEB_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PHYSICAL_WEB_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

#include "components/prefs/pref_service.h"

// This View Controller is responsible for managing the settings related to
// the Physical Web.
@interface PhysicalWebCollectionViewController
    : SettingsRootCollectionViewController

// The designated initializer.
- (instancetype)initWithPrefs:(PrefService*)prefs NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

// Given the current Physical Web preference state, determine whether the
// preference should be rendered as enabled.
+ (BOOL)shouldEnableForPreferenceState:(int)preferenceState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PHYSICAL_WEB_COLLECTION_VIEW_CONTROLLER_H_
