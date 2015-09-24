// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* GeolocationInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const GURL& requesting_frame,
    const std::string& display_languages,
    const PermissionSetCallback& callback) {
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new GeolocationInfoBarDelegate(
          requesting_frame, display_languages, callback))));
}

GeolocationInfoBarDelegate::GeolocationInfoBarDelegate(
    const GURL& requesting_frame,
    const std::string& display_languages,
    const PermissionSetCallback& callback)
    : PermissionInfobarDelegate(requesting_frame,
                                CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                callback),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {
}

GeolocationInfoBarDelegate::~GeolocationInfoBarDelegate() {
}

int GeolocationInfoBarDelegate::GetIconId() const {
  return IDR_INFOBAR_GEOLOCATION;
}

base::string16 GeolocationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
                                    url_formatter::FormatUrlForSecurityDisplay(
                                        requesting_frame_, display_languages_));
}
