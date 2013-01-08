// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"

#include "content/public/browser/navigation_details.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ConfirmInfoBarDelegate::~ConfirmInfoBarDelegate() {
}

InfoBarDelegate::InfoBarAutomationType
    ConfirmInfoBarDelegate::GetInfoBarAutomationType() const {
  return CONFIRM_INFOBAR;
}

int ConfirmInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 ConfirmInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_OK : IDS_CANCEL);
}

bool ConfirmInfoBarDelegate::NeedElevation(InfoBarButton button) const {
  return false;
}

bool ConfirmInfoBarDelegate::Accept() {
  return true;
}

bool ConfirmInfoBarDelegate::Cancel() {
  return true;
}

string16 ConfirmInfoBarDelegate::GetLinkText() const {
  return string16();
}

bool ConfirmInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  return true;
}

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate(
    InfoBarService* infobar_service)
    : InfoBarDelegate(infobar_service) {
}

bool ConfirmInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  ConfirmInfoBarDelegate* confirm_delegate =
      delegate->AsConfirmInfoBarDelegate();
  return confirm_delegate &&
      (confirm_delegate->GetMessageText() == GetMessageText());
}

bool ConfirmInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  return !details.did_replace_entry &&
      InfoBarDelegate::ShouldExpireInternal(details);
}

ConfirmInfoBarDelegate* ConfirmInfoBarDelegate::AsConfirmInfoBarDelegate() {
  return this;
}
