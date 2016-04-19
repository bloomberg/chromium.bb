// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* GeolocationInfoBarDelegateAndroid::Create(
    InfoBarService* infobar_service,
    const GURL& requesting_frame,
    const PermissionSetCallback& callback) {
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new GeolocationInfoBarDelegateAndroid(requesting_frame, callback))));
}

GeolocationInfoBarDelegateAndroid::GeolocationInfoBarDelegateAndroid(
    const GURL& requesting_frame,
    const PermissionSetCallback& callback)
    : PermissionInfobarDelegate(requesting_frame,
                                content::PermissionType::GEOLOCATION,
                                CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                callback),
      requesting_frame_(requesting_frame) {}

GeolocationInfoBarDelegateAndroid::~GeolocationInfoBarDelegateAndroid() {}

infobars::InfoBarDelegate::InfoBarIdentifier
GeolocationInfoBarDelegateAndroid::GetIdentifier() const {
  return GEOLOCATION_INFOBAR_DELEGATE_ANDROID;
}

int GeolocationInfoBarDelegateAndroid::GetIconId() const {
  return IDR_ANDROID_INFOBAR_GEOLOCATION;
}

base::string16 GeolocationInfoBarDelegateAndroid::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
                                    url_formatter::FormatUrlForSecurityDisplay(
                                        requesting_frame_));
}
