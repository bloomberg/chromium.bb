// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_confirm_infobar_delegate_android.h"

#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

GeolocationConfirmInfoBarDelegateAndroid::
GeolocationConfirmInfoBarDelegateAndroid(
    InfoBarTabHelper* infobar_helper,
    GeolocationInfoBarQueueController* controller,
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame_url,
    const std::string& display_languages)
    : GeolocationConfirmInfoBarDelegate(infobar_helper,
                                        controller,
                                        render_process_id,
                                        render_view_id,
                                        bridge_id,
                                        requesting_frame_url,
                                        display_languages) {
}

string16 GeolocationConfirmInfoBarDelegateAndroid::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_GEOLOCATION_ALLOW_BUTTON : IDS_GEOLOCATION_DENY_BUTTON);
}
