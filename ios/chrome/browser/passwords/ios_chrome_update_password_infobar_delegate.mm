// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"

#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/chrome/browser/passwords/update_password_infobar_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::PasswordFormManager;

// static
void IOSChromeUpdatePasswordInfoBarDelegate::Create(
    bool is_smart_lock_branding_enabled,
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<PasswordFormManager> form_manager) {
  DCHECK(infobar_manager);
  auto delegate = base::WrapUnique(new IOSChromeUpdatePasswordInfoBarDelegate(
      is_smart_lock_branding_enabled, std::move(form_manager)));
  std::unique_ptr<InfoBarIOS> infobar(new InfoBarIOS(std::move(delegate)));
  base::scoped_nsobject<UpdatePasswordInfoBarController> controller(
      [[UpdatePasswordInfoBarController alloc] initWithDelegate:infobar.get()]);
  infobar->SetController(controller);
  infobar_manager->AddInfoBar(std::move(infobar));
}

IOSChromeUpdatePasswordInfoBarDelegate::
    ~IOSChromeUpdatePasswordInfoBarDelegate() {}

IOSChromeUpdatePasswordInfoBarDelegate::IOSChromeUpdatePasswordInfoBarDelegate(
    bool is_smart_lock_branding_enabled,
    std::unique_ptr<PasswordFormManager> form_manager)
    : IOSChromePasswordManagerInfoBarDelegate(is_smart_lock_branding_enabled,
                                              std::move(form_manager)) {
  selected_account_ = form_to_save()->preferred_match()->username_value;
}

bool IOSChromeUpdatePasswordInfoBarDelegate::ShowMultipleAccounts() const {
  // If a password is overriden, we know that the preferred match account is
  // correct, so should not provide the option to choose a different account.
  return form_to_save()->best_matches().size() > 1 &&
         !form_to_save()->password_overridden();
}

NSArray* IOSChromeUpdatePasswordInfoBarDelegate::GetAccounts() const {
  NSMutableArray* usernames = [NSMutableArray array];
  for (const auto& match : form_to_save()->best_matches()) {
    [usernames addObject:base::SysUTF16ToNSString(match.first)];
  }
  return usernames;
}

base::string16 IOSChromeUpdatePasswordInfoBarDelegate::GetBranding() const {
  return l10n_util::GetStringUTF16(
      is_smart_lock_branding_enabled()
          ? IDS_IOS_PASSWORD_MANAGER_SMART_LOCK_FOR_PASSWORDS
          : IDS_IOS_SHORT_PRODUCT_NAME);
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSChromeUpdatePasswordInfoBarDelegate::GetIdentifier() const {
  return UPDATE_PASSWORD_INFOBAR_DELEGATE;
}

base::string16 IOSChromeUpdatePasswordInfoBarDelegate::GetMessageText() const {
  return selected_account_.length() > 0
             ? l10n_util::GetStringFUTF16(
                   IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD_FOR_ACCOUNT,
                   GetBranding(), selected_account_)
             : l10n_util::GetStringFUTF16(
                   IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD, GetBranding());
}

int IOSChromeUpdatePasswordInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 IOSChromeUpdatePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON);
}

bool IOSChromeUpdatePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save());
  if (ShowMultipleAccounts()) {
    form_to_save()->Update(
        *form_to_save()->best_matches().at(selected_account_));
  } else {
    form_to_save()->Update(form_to_save()->pending_credentials());
  }
  return true;
}
