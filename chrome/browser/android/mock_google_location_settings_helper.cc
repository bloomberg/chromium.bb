// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/mock_google_location_settings_helper.h"

bool MockGoogleLocationSettingsHelper::master_location_enabled = false;
bool MockGoogleLocationSettingsHelper::google_apps_location_enabled = false;

// Factory function
GoogleLocationSettingsHelper* GoogleLocationSettingsHelper::Create() {
  return new MockGoogleLocationSettingsHelper();
}

MockGoogleLocationSettingsHelper::MockGoogleLocationSettingsHelper()
    : GoogleLocationSettingsHelper() {
}

MockGoogleLocationSettingsHelper::~MockGoogleLocationSettingsHelper() {
}

void MockGoogleLocationSettingsHelper::SetLocationStatus(
    bool master, bool google_apps) {
  master_location_enabled = master;
  google_apps_location_enabled = google_apps;
}

bool MockGoogleLocationSettingsHelper::IsGoogleAppsLocationSettingEnabled() {
  return google_apps_location_enabled;
}

bool MockGoogleLocationSettingsHelper::IsMasterLocationSettingEnabled() {
  return master_location_enabled;
}

bool MockGoogleLocationSettingsHelper::IsSystemLocationEnabled() {
  return IsMasterLocationSettingEnabled() &&
      IsGoogleAppsLocationSettingEnabled();
}
