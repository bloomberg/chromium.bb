// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/url_formatter.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"


// static
infobars::InfoBar* GeolocationInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages) {
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new GeolocationInfoBarDelegate(
          controller, id, requesting_frame, display_languages))));
}

GeolocationInfoBarDelegate::GeolocationInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages)
    : PermissionInfobarDelegate(controller, id, requesting_frame,
                                CONTENT_SETTINGS_TYPE_GEOLOCATION),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {
}

GeolocationInfoBarDelegate::~GeolocationInfoBarDelegate() {
}

int GeolocationInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_GEOLOCATION;
}

base::string16 GeolocationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_GEOLOCATION_INFOBAR_QUESTION,
      url_formatter::FormatUrl(
          requesting_frame_, display_languages_,
          url_formatter::kFormatUrlOmitUsernamePassword |
              url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
          net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
}
