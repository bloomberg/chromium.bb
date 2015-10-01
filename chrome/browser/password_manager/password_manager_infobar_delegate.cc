// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

PasswordManagerInfoBarDelegate::~PasswordManagerInfoBarDelegate() {}

PasswordManagerInfoBarDelegate::PasswordManagerInfoBarDelegate()
    : ConfirmInfoBarDelegate(), message_link_range_(gfx::Range()) {}

infobars::InfoBarDelegate::Type PasswordManagerInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarAutomationType
PasswordManagerInfoBarDelegate::GetInfoBarAutomationType() const {
  return PASSWORD_INFOBAR;
}

int PasswordManagerInfoBarDelegate::GetIconId() const {
  return IDR_INFOBAR_SAVE_PASSWORD;
}

bool PasswordManagerInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return !details.is_redirect && ConfirmInfoBarDelegate::ShouldExpire(details);
}

base::string16 PasswordManagerInfoBarDelegate::GetMessageText() const {
  return message_;
}

GURL PasswordManagerInfoBarDelegate::GetLinkURL() const {
  return GURL(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SMART_LOCK_PAGE));
}

bool PasswordManagerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  ConfirmInfoBarDelegate::LinkClicked(disposition);
  return true;
}

void PasswordManagerInfoBarDelegate::SetMessage(const base::string16& message) {
  message_ = message;
}

void PasswordManagerInfoBarDelegate::SetMessageLinkRange(
    const gfx::Range& message_link_range) {
  message_link_range_ = message_link_range;
}
