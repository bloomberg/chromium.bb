// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/infobar_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// InfoBarDelegate ------------------------------------------------------------

InfoBarDelegate::~InfoBarDelegate() {
}

bool InfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  return false;
}

bool InfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  return (contents_unique_id_ != details.entry->unique_id()) ||
      (PageTransition::StripQualifier(details.entry->transition_type()) ==
          PageTransition::RELOAD);
}

void InfoBarDelegate::InfoBarDismissed() {
}

void InfoBarDelegate::InfoBarClosed() {
}

SkBitmap* InfoBarDelegate::GetIcon() const {
  return NULL;
}

InfoBarDelegate::Type InfoBarDelegate::GetInfoBarType() const {
  return WARNING_TYPE;
}

ConfirmInfoBarDelegate* InfoBarDelegate::AsConfirmInfoBarDelegate() {
  return NULL;
}

CrashedExtensionInfoBarDelegate*
    InfoBarDelegate::AsCrashedExtensionInfoBarDelegate() {
  return NULL;
}

ExtensionInfoBarDelegate* InfoBarDelegate::AsExtensionInfoBarDelegate() {
  return NULL;
}

LinkInfoBarDelegate* InfoBarDelegate::AsLinkInfoBarDelegate() {
  return NULL;
}

ThemeInstalledInfoBarDelegate*
    InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return NULL;
}

TranslateInfoBarDelegate* InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return NULL;
}

InfoBarDelegate::InfoBarDelegate(TabContents* contents)
    : contents_unique_id_(0) {
  if (contents)
    StoreActiveEntryUniqueID(contents);
}

void InfoBarDelegate::StoreActiveEntryUniqueID(TabContents* contents) {
  NavigationEntry* active_entry = contents->controller().GetActiveEntry();
  contents_unique_id_ = active_entry ? active_entry->unique_id() : 0;
}


// LinkInfoBarDelegate --------------------------------------------------------

string16 LinkInfoBarDelegate::GetMessageTextWithOffset(
    size_t* link_offset) const {
  *link_offset = string16::npos;
  return string16();
}

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
