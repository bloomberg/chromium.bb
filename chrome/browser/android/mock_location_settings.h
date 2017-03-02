// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MOCK_LOCATION_SETTINGS_H_
#define CHROME_BROWSER_ANDROID_MOCK_LOCATION_SETTINGS_H_

#include "base/macros.h"
#include "chrome/browser/android/location_settings.h"

// Mock implementation of LocationSettings for unit tests.
class MockLocationSettings : public LocationSettings {
 public:
  MockLocationSettings();
  ~MockLocationSettings() override;

  static void SetLocationStatus(bool master, bool google_apps);

  bool CanSitesRequestLocationPermission(
      content::WebContents* web_contents) override;

  bool CanPromptToEnableSystemLocationSetting() override;

  void PromptToEnableSystemLocationSetting(
      const LocationSettingsDialogContext prompt_context,
      content::WebContents* web_contents,
      LocationSettingsDialogOutcomeCallback callback) override;

  bool IsMasterLocationSettingEnabled();
  bool IsGoogleAppsLocationSettingEnabled();

 private:
  static bool master_location_enabled;
  static bool google_apps_location_enabled;

  DISALLOW_COPY_AND_ASSIGN(MockLocationSettings);
};

#endif  // CHROME_BROWSER_ANDROID_MOCK_LOCATION_SETTINGS_H_
