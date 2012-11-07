// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/testshell/testshell_google_location_settings_helper.h"

// Factory function
GoogleLocationSettingsHelper* GoogleLocationSettingsHelper::Create() {
  return new TestShellGoogleLocationSettingsHelper();
}

TestShellGoogleLocationSettingsHelper::TestShellGoogleLocationSettingsHelper()
    : GoogleLocationSettingsHelper() {
}

TestShellGoogleLocationSettingsHelper::
    ~TestShellGoogleLocationSettingsHelper() {
}

std::string TestShellGoogleLocationSettingsHelper::GetAcceptButtonLabel() {
  return "Allow";
}

void TestShellGoogleLocationSettingsHelper::ShowGoogleLocationSettings() {
}

bool TestShellGoogleLocationSettingsHelper::
    IsGoogleAppsLocationSettingEnabled() {
  return true;
}

bool TestShellGoogleLocationSettingsHelper::IsMasterLocationSettingEnabled() {
  return true;
}
