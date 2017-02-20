// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_infobar_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

ProtectedMediaIdentifierInfoBarDelegateAndroid::
    ProtectedMediaIdentifierInfoBarDelegateAndroid(
        const GURL& requesting_frame,
        bool user_gesture,
        Profile* profile,
        const PermissionSetCallback& callback)
    : PermissionInfoBarDelegate(
          requesting_frame,
          CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
          user_gesture,
          profile,
          callback) {}

ProtectedMediaIdentifierInfoBarDelegateAndroid::
    ~ProtectedMediaIdentifierInfoBarDelegateAndroid() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ProtectedMediaIdentifierInfoBarDelegateAndroid::GetIdentifier() const {
  return PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_ANDROID;
}

int ProtectedMediaIdentifierInfoBarDelegateAndroid::GetIconId() const {
  return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
}

base::string16 ProtectedMediaIdentifierInfoBarDelegateAndroid::GetLinkText()
    const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL ProtectedMediaIdentifierInfoBarDelegateAndroid::GetLinkURL() const {
  return GURL(chrome::kEnhancedPlaybackNotificationLearnMoreURL);
}

int ProtectedMediaIdentifierInfoBarDelegateAndroid::GetMessageResourceId()
    const {
  return IDS_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_QUESTION;
}
