// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/shell/chrome_shell_google_location_settings_helper.h"

// Factory function
GoogleLocationSettingsHelper* GoogleLocationSettingsHelper::Create() {
  return new ChromeShellGoogleLocationSettingsHelper();
}

ChromeShellGoogleLocationSettingsHelper::
    ChromeShellGoogleLocationSettingsHelper()
    : GoogleLocationSettingsHelper() {
}

ChromeShellGoogleLocationSettingsHelper::
    ~ChromeShellGoogleLocationSettingsHelper() {
}

std::string ChromeShellGoogleLocationSettingsHelper::GetAcceptButtonLabel(
    bool allow) {
  return "Allow";
}

void ChromeShellGoogleLocationSettingsHelper::ShowGoogleLocationSettings() {
}

bool ChromeShellGoogleLocationSettingsHelper::
    IsGoogleAppsLocationSettingEnabled() {
  return true;
}

bool ChromeShellGoogleLocationSettingsHelper::IsMasterLocationSettingEnabled() {
  return true;
}

bool ChromeShellGoogleLocationSettingsHelper::IsAllowLabel() {
  return true;
}
