// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/generated_password_saved_infobar_delegate_android.h"

#include <cstddef>

#include "chrome/browser/android/chromium_application.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

GeneratedPasswordSavedInfoBarDelegateAndroid::
    ~GeneratedPasswordSavedInfoBarDelegateAndroid() {}

void GeneratedPasswordSavedInfoBarDelegateAndroid::OnInlineLinkClicked() const {
  chrome::android::ChromiumApplication::ShowPasswordSettings();
}

GeneratedPasswordSavedInfoBarDelegateAndroid::
    GeneratedPasswordSavedInfoBarDelegateAndroid()
    : button_label_(l10n_util::GetStringUTF16(IDS_OK)) {
  base::string16 link = l10n_util::GetStringUTF16(
      IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_LINK);
  size_t offset;
  message_text_ = l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT_INFOBAR, link, &offset);
  inline_link_range_ = gfx::Range(offset, offset + link.length());
}

infobars::InfoBarDelegate::Type
GeneratedPasswordSavedInfoBarDelegateAndroid::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int GeneratedPasswordSavedInfoBarDelegateAndroid::GetIconID() const {
  return IDR_INFOBAR_SAVE_PASSWORD;
}
