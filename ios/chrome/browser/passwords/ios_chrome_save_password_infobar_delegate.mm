// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::PasswordFormManager;

// static
void IOSChromeSavePasswordInfoBarDelegate::Create(
    bool is_smart_lock_branding_enabled,
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<PasswordFormManager> form_to_save) {
  DCHECK(infobar_manager);
  auto delegate = base::WrapUnique(new IOSChromeSavePasswordInfoBarDelegate(
      is_smart_lock_branding_enabled, std::move(form_to_save)));
  infobar_manager->AddInfoBar(
      infobar_manager->CreateConfirmInfoBar(std::move(delegate)));
}

IOSChromeSavePasswordInfoBarDelegate::~IOSChromeSavePasswordInfoBarDelegate() {}

IOSChromeSavePasswordInfoBarDelegate::IOSChromeSavePasswordInfoBarDelegate(
    bool is_smart_lock_branding_enabled,
    std::unique_ptr<PasswordFormManager> form_to_save)
    : IOSChromePasswordManagerInfoBarDelegate(is_smart_lock_branding_enabled,
                                              std::move(form_to_save)) {}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSChromeSavePasswordInfoBarDelegate::GetIdentifier() const {
  return IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE;
}

base::string16 IOSChromeSavePasswordInfoBarDelegate::GetMessageText() const {
  if (is_smart_lock_branding_enabled()) {
    return l10n_util::GetStringUTF16(
        IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT_SMART_LOCK_BRANDING);
  }
  return l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
}

base::string16 IOSChromeSavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON
                            : IDS_IOS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
}

bool IOSChromeSavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save());
  form_to_save()->Save();
  set_infobar_response(password_manager::metrics_util::CLICKED_SAVE);
  return true;
}

bool IOSChromeSavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save());
  form_to_save()->PermanentlyBlacklist();
  set_infobar_response(password_manager::metrics_util::CLICKED_NEVER);
  return true;
}
