// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/infobar_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// LinkInfoBarDelegate --------------------------------------------------------

bool LinkInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  return true;
}

LinkInfoBarDelegate::LinkInfoBarDelegate(TabContents* contents)
    : InfoBarDelegate(contents) {
}

LinkInfoBarDelegate::~LinkInfoBarDelegate() {
}

LinkInfoBarDelegate* LinkInfoBarDelegate::AsLinkInfoBarDelegate() {
  return this;
}


// ConfirmInfoBarDelegate -----------------------------------------------------

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

string16 ConfirmInfoBarDelegate::GetLinkText() {
  return string16();
}

bool ConfirmInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  return true;
}

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate(TabContents* contents)
    : InfoBarDelegate(contents) {
}

ConfirmInfoBarDelegate::~ConfirmInfoBarDelegate() {
}

bool ConfirmInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  ConfirmInfoBarDelegate* confirm_delegate =
      delegate->AsConfirmInfoBarDelegate();
  return confirm_delegate &&
      (confirm_delegate->GetMessageText() == GetMessageText());
}

ConfirmInfoBarDelegate* ConfirmInfoBarDelegate::AsConfirmInfoBarDelegate() {
  return this;
}


// SimpleAlertInfoBarDelegate -------------------------------------------------

SimpleAlertInfoBarDelegate::SimpleAlertInfoBarDelegate(
    TabContents* contents,
    SkBitmap* icon,
    const string16& message,
    bool auto_expire)
    : ConfirmInfoBarDelegate(contents),
      icon_(icon),
      message_(message),
      auto_expire_(auto_expire) {
}

SimpleAlertInfoBarDelegate::~SimpleAlertInfoBarDelegate() {
}

bool SimpleAlertInfoBarDelegate::ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const {
  return auto_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

void SimpleAlertInfoBarDelegate::InfoBarClosed() {
  delete this;
}

SkBitmap* SimpleAlertInfoBarDelegate::GetIcon() const {
  return icon_;
}

string16 SimpleAlertInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SimpleAlertInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
