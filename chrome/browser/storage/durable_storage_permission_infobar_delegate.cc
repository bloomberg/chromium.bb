// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/url_formatter.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* DurableStoragePermissionInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages,
    ContentSettingsType type) {
  return infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new DurableStoragePermissionInfoBarDelegate(
              controller, id, requesting_frame, display_languages, type))));
}

DurableStoragePermissionInfoBarDelegate::
    DurableStoragePermissionInfoBarDelegate(
        PermissionQueueController* controller,
        const PermissionRequestID& id,
        const GURL& requesting_frame,
        const std::string& display_languages,
        ContentSettingsType type)
    : PermissionInfobarDelegate(controller, id, requesting_frame, type),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {
}

base::string16 DurableStoragePermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_DURABLE_STORAGE_INFOBAR_QUESTION,
      url_formatter::FormatUrl(
          requesting_frame_.GetOrigin(), display_languages_,
          url_formatter::kFormatUrlOmitUsernamePassword |
              url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
          net::UnescapeRule::SPACES, NULL, NULL, NULL));
}
