// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/google_location_settings_helper.h"

// Temporary stubs for a 3 way patch

std::string GoogleLocationSettingsHelper::GetAcceptButtonLabel(bool allow) {
  return std::string();
}

bool GoogleLocationSettingsHelper::IsAllowLabel() {
  return false;
}

void GoogleLocationSettingsHelper::ShowGoogleLocationSettings() {
}

bool GoogleLocationSettingsHelper::IsMasterLocationSettingEnabled() {
  return false;
}

bool GoogleLocationSettingsHelper::IsGoogleAppsLocationSettingEnabled() {
  return false;
}

bool GoogleLocationSettingsHelper::IsSystemLocationEnabled() {
  return IsMasterLocationSettingEnabled() &&
      IsGoogleAppsLocationSettingEnabled();
}
