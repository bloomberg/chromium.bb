// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_infobar_delegate_android.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/url_formatter.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* DurableStoragePermissionInfoBarDelegateAndroid::Create(
    InfoBarService* infobar_service,
    const GURL& requesting_frame,
    const std::string& display_languages,
    ContentSettingsType type,
    const PermissionSetCallback& callback) {
  return infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new DurableStoragePermissionInfoBarDelegateAndroid(
              requesting_frame, display_languages, type, callback))));
}

DurableStoragePermissionInfoBarDelegateAndroid::
    DurableStoragePermissionInfoBarDelegateAndroid(
        const GURL& requesting_frame,
        const std::string& display_languages,
        ContentSettingsType type,
        const PermissionSetCallback& callback)
    : PermissionInfobarDelegate(requesting_frame, type, callback),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {}

base::string16 DurableStoragePermissionInfoBarDelegateAndroid::GetMessageText()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_DURABLE_STORAGE_INFOBAR_QUESTION,
      url_formatter::FormatUrl(
          requesting_frame_.GetOrigin(), display_languages_,
          url_formatter::kFormatUrlOmitUsernamePassword |
              url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
          net::UnescapeRule::SPACES, NULL, NULL, NULL));
}
