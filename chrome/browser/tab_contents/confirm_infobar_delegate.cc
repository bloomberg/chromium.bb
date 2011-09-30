// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate(InfoBarTabHelper* infobar_helper)
    : InfoBarDelegate(infobar_helper) {
}

ConfirmInfoBarDelegate::~ConfirmInfoBarDelegate() {
}

bool ConfirmInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  ConfirmInfoBarDelegate* confirm_delegate =
      delegate->AsConfirmInfoBarDelegate();
  return confirm_delegate &&
      (confirm_delegate->GetMessageText() == GetMessageText());
}

bool ConfirmInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  if (details.did_replace_entry)
    return false;
  return InfoBarDelegate::ShouldExpire(details);
}

ConfirmInfoBarDelegate* ConfirmInfoBarDelegate::AsConfirmInfoBarDelegate() {
  return this;
}
