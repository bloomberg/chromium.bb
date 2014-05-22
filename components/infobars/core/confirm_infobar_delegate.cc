// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/confirm_infobar_delegate.h"

#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"

using infobars::InfoBarDelegate;

ConfirmInfoBarDelegate::~ConfirmInfoBarDelegate() {
}

InfoBarDelegate::InfoBarAutomationType
    ConfirmInfoBarDelegate::GetInfoBarAutomationType() const {
  return CONFIRM_INFOBAR;
}

int ConfirmInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 ConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_APP_OK : IDS_APP_CANCEL);
}

bool ConfirmInfoBarDelegate::OKButtonTriggersUACPrompt() const {
  return false;
}

bool ConfirmInfoBarDelegate::Accept() {
  return true;
}

bool ConfirmInfoBarDelegate::Cancel() {
  return true;
}

base::string16 ConfirmInfoBarDelegate::GetLinkText() const {
  return base::string16();
}

bool ConfirmInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  return true;
}

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate()
    : InfoBarDelegate() {
}

bool ConfirmInfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  return !details.did_replace_entry &&
      InfoBarDelegate::ShouldExpireInternal(details);
}

// ConfirmInfoBarDelegate::CreateInfoBar() is implemented in platform-specific
// files.

bool ConfirmInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  ConfirmInfoBarDelegate* confirm_delegate =
      delegate->AsConfirmInfoBarDelegate();
  return confirm_delegate &&
      (confirm_delegate->GetMessageText() == GetMessageText());
}

ConfirmInfoBarDelegate* ConfirmInfoBarDelegate::AsConfirmInfoBarDelegate() {
  return this;
}
