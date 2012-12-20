// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_confirm_infobar_delegate_android.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/android/google_location_settings_helper.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

GeolocationConfirmInfoBarDelegateAndroid::
    GeolocationConfirmInfoBarDelegateAndroid(
    InfoBarService* infobar_service,
    GeolocationInfoBarQueueController* controller,
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame_url,
    const std::string& display_languages)
    : GeolocationConfirmInfoBarDelegate(infobar_service, controller, id,
                                        requesting_frame_url,
                                        display_languages),
      google_location_settings_helper_(
          GoogleLocationSettingsHelper::Create()) {
}

GeolocationConfirmInfoBarDelegateAndroid::
    ~GeolocationConfirmInfoBarDelegateAndroid() {
}

bool GeolocationConfirmInfoBarDelegateAndroid::Accept() {
  // Accept button text could be either 'Allow' or 'Google Location Settings'.
  // If 'Allow' we follow the regular flow.
  if (google_location_settings_helper_->IsGoogleAppsLocationSettingEnabled())
    return GeolocationConfirmInfoBarDelegate::Accept();

  // If 'Google Location Settings', we need to open the system Google Location
  // Settings activity.
  google_location_settings_helper_->ShowGoogleLocationSettings();
  SetPermission(false, false);
  return true;
}

string16 GeolocationConfirmInfoBarDelegateAndroid::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return UTF8ToUTF16(
        google_location_settings_helper_->GetAcceptButtonLabel());
  }
  return l10n_util::GetStringUTF16(IDS_GEOLOCATION_DENY_BUTTON);
}
