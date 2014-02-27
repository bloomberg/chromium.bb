// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/mock_google_location_settings_helper.h"

bool MockGoogleLocationSettingsHelper::master_location_enabled = false;
bool MockGoogleLocationSettingsHelper::google_apps_location_enabled = false;
bool MockGoogleLocationSettingsHelper::was_google_location_settings_called
    = false;

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

std::string MockGoogleLocationSettingsHelper::GetAcceptButtonLabel(bool allow) {
  return IsAllowLabel() ? "Allow" : "Settings";
}

void MockGoogleLocationSettingsHelper::ShowGoogleLocationSettings() {
  was_google_location_settings_called = true;
}

bool MockGoogleLocationSettingsHelper::IsGoogleAppsLocationSettingEnabled() {
  return google_apps_location_enabled;
}

bool MockGoogleLocationSettingsHelper::IsMasterLocationSettingEnabled() {
  return master_location_enabled;
}

bool MockGoogleLocationSettingsHelper::WasGoogleLocationSettingsCalled() {
  return was_google_location_settings_called;
}

bool MockGoogleLocationSettingsHelper::IsAllowLabel() {
  return google_apps_location_enabled;
}
