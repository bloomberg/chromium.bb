// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MOCK_GOOGLE_LOCATION_SETTINGS_HELPER_H_
#define CHROME_BROWSER_ANDROID_MOCK_GOOGLE_LOCATION_SETTINGS_HELPER_H_

#include "chrome/browser/android/google_location_settings_helper.h"

// Mock implementation of GoogleLocationSettingsHelper for unit tests.
class MockGoogleLocationSettingsHelper : public GoogleLocationSettingsHelper {
 public:
  static void SetLocationStatus(bool master, bool google_apps);
  static bool WasGoogleLocationSettingsCalled();

  // GoogleLocationSettingsHelper implementation:
  virtual std::string GetAcceptButtonLabel(bool allow) OVERRIDE;
  virtual void ShowGoogleLocationSettings() OVERRIDE;
  virtual bool IsMasterLocationSettingEnabled() OVERRIDE;
  virtual bool IsGoogleAppsLocationSettingEnabled() OVERRIDE;
  virtual bool IsAllowLabel() OVERRIDE;

 protected:
  MockGoogleLocationSettingsHelper();
  virtual ~MockGoogleLocationSettingsHelper();

 private:
  friend class GoogleLocationSettingsHelper;

  static bool master_location_enabled;
  static bool google_apps_location_enabled;
  static bool was_google_location_settings_called;

  DISALLOW_COPY_AND_ASSIGN(MockGoogleLocationSettingsHelper);
};

#endif  // CHROME_BROWSER_ANDROID_MOCK_GOOGLE_LOCATION_SETTINGS_HELPER_H_
