// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"


// static
infobars::InfoBar* GeolocationInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages) {
  const content::NavigationEntry* committed_entry =
      infobar_service->web_contents()->GetController().GetLastCommittedEntry();
  GeolocationInfoBarDelegate* const delegate = new GeolocationInfoBarDelegate(
          controller, id, requesting_frame,
          committed_entry ? committed_entry->GetUniqueID() : 0,
          display_languages);

  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(delegate)));
}

GeolocationInfoBarDelegate::GeolocationInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    int contents_unique_id,
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
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_, display_languages_,
                     net::kFormatUrlOmitUsernamePassword |
                     net::kFormatUrlOmitTrailingSlashOnBareHostname,
                     net::UnescapeRule::SPACES, NULL, NULL, NULL));
}
