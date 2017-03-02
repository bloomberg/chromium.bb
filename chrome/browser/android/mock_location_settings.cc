// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/mock_location_settings.h"

bool MockLocationSettings::master_location_enabled = false;
bool MockLocationSettings::google_apps_location_enabled = false;

MockLocationSettings::MockLocationSettings() : LocationSettings() {
}

MockLocationSettings::~MockLocationSettings() {
}

void MockLocationSettings::SetLocationStatus(
    bool master, bool google_apps) {
  master_location_enabled = master;
  google_apps_location_enabled = google_apps;
}

bool MockLocationSettings::IsGoogleAppsLocationSettingEnabled() {
  return google_apps_location_enabled;
}

bool MockLocationSettings::IsMasterLocationSettingEnabled() {
  return master_location_enabled;
}

bool MockLocationSettings::CanSitesRequestLocationPermission(
    content::WebContents* web_contents) {
  return IsMasterLocationSettingEnabled() &&
      IsGoogleAppsLocationSettingEnabled();
}

bool MockLocationSettings::CanPromptToEnableSystemLocationSetting() {
  return false;
}

void MockLocationSettings::PromptToEnableSystemLocationSetting(
    const LocationSettingsDialogContext prompt_context,
    content::WebContents* web_contents,
    LocationSettingsDialogOutcomeCallback callback) {
  std::move(callback).Run(LocationSettingsDialogOutcome::NO_PROMPT);
}
