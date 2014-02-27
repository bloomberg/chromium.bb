// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate_android.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/google_location_settings_helper.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
enum GeolocationInfoBarDelegateAndroidEvent {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.

  // User allowed the page to use geolocation.
  GEOLOCATION_INFO_BAR_DELEGATE_ANDROID_EVENT_ALLOW = 0,

  // User opened geolocation settings.
  GEOLOCATION_INFO_BAR_DELEGATE_ANDROID_EVENT_SETTINGS = 1,

  // NOTE: Add entries only immediately above this line.
  GEOLOCATION_INFO_BAR_DELEGATE_ANDROID_EVENT_COUNT = 2
};

void RecordUmaEvent(GeolocationInfoBarDelegateAndroidEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Geolocation.InfoBarDelegateAndroid.Event",
      event, GEOLOCATION_INFO_BAR_DELEGATE_ANDROID_EVENT_COUNT);
}
}  // namespace

GeolocationInfoBarDelegateAndroid::GeolocationInfoBarDelegateAndroid(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame_url,
    int contents_unique_id,
    const std::string& display_languages,
    const std::string& accept_button_label)
    : GeolocationInfoBarDelegate(controller, id, requesting_frame_url,
                                 contents_unique_id, display_languages,
                                 accept_button_label_),
      google_location_settings_helper_(
          GoogleLocationSettingsHelper::Create()),
      accept_button_label_(accept_button_label) {
}

GeolocationInfoBarDelegateAndroid::~GeolocationInfoBarDelegateAndroid() {
}

bool GeolocationInfoBarDelegateAndroid::Accept() {
  set_user_has_interacted();

  // Accept button text could be either 'Allow' or 'Google Location Settings'.
  // If 'Allow' we follow the regular flow.
  if (google_location_settings_helper_->IsGoogleAppsLocationSettingEnabled()) {
    RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_ANDROID_EVENT_ALLOW);
    return GeolocationInfoBarDelegate::Accept();
  }

  // If 'Google Location Settings', we need to open the system Google Location
  // Settings activity.
  RecordUmaEvent(GEOLOCATION_INFO_BAR_DELEGATE_ANDROID_EVENT_SETTINGS);
  google_location_settings_helper_->ShowGoogleLocationSettings();
  SetPermission(false, false);
  return true;
}

base::string16 GeolocationInfoBarDelegateAndroid::GetButtonLabel(
    InfoBarButton button) const {
  return (button == BUTTON_OK) ?
      base::UTF8ToUTF16(accept_button_label_ ) :
      l10n_util::GetStringUTF16(IDS_GEOLOCATION_DENY_BUTTON);
}
