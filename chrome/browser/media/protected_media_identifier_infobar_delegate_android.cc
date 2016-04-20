// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_infobar_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* ProtectedMediaIdentifierInfoBarDelegateAndroid::Create(
    InfoBarService* infobar_service,
    const GURL& requesting_frame,
    const PermissionSetCallback& callback) {
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new ProtectedMediaIdentifierInfoBarDelegateAndroid(requesting_frame,
                                                             callback))));
}

ProtectedMediaIdentifierInfoBarDelegateAndroid::
    ProtectedMediaIdentifierInfoBarDelegateAndroid(
        const GURL& requesting_frame,
        const PermissionSetCallback& callback)
    : PermissionInfobarDelegate(
          requesting_frame,
          content::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
          CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
          callback),
      requesting_frame_(requesting_frame) {}

ProtectedMediaIdentifierInfoBarDelegateAndroid::
    ~ProtectedMediaIdentifierInfoBarDelegateAndroid() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ProtectedMediaIdentifierInfoBarDelegateAndroid::GetIdentifier() const {
  return PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_ANDROID;
}

int ProtectedMediaIdentifierInfoBarDelegateAndroid::GetIconId() const {
  return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
}

int ProtectedMediaIdentifierInfoBarDelegateAndroid::GetMessageResourceId()
    const {
  return IDS_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_QUESTION;
}

base::string16 ProtectedMediaIdentifierInfoBarDelegateAndroid::GetLinkText()
    const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL ProtectedMediaIdentifierInfoBarDelegateAndroid::GetLinkURL() const {
  return GURL(chrome::kEnhancedPlaybackNotificationLearnMoreURL);
}
