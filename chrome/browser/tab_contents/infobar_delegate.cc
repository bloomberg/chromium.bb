// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/infobar_delegate.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

// InfoBarDelegate: ------------------------------------------------------------

bool InfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  return false;
}

bool InfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  bool is_reload =
      PageTransition::StripQualifier(details.entry->transition_type()) ==
          PageTransition::RELOAD;
  return is_reload || (contents_unique_id_ != details.entry->unique_id());
}

SkBitmap* InfoBarDelegate::GetIcon() const {
  return NULL;
}

AlertInfoBarDelegate* InfoBarDelegate::AsAlertInfoBarDelegate() {
  return NULL;
}

LinkInfoBarDelegate* InfoBarDelegate::AsLinkInfoBarDelegate() {
  return NULL;
}

ConfirmInfoBarDelegate* InfoBarDelegate::AsConfirmInfoBarDelegate() {
  return NULL;
}

ThemeInstalledInfoBarDelegate*
InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return NULL;
}

TranslateInfoBarDelegate* InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return NULL;
}

ExtensionInfoBarDelegate* InfoBarDelegate::AsExtensionInfoBarDelegate() {
  return NULL;
}

CrashedExtensionInfoBarDelegate*
InfoBarDelegate::AsCrashedExtensionInfoBarDelegate() {
  return NULL;
}

InfoBarDelegate::Type InfoBarDelegate::GetInfoBarType() {
  return WARNING_TYPE;
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

// AlertInfoBarDelegate: -------------------------------------------------------

SkBitmap* AlertInfoBarDelegate::GetIcon() const {
  return NULL;
}

bool AlertInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  AlertInfoBarDelegate* alert_delegate = delegate->AsAlertInfoBarDelegate();
  if (!alert_delegate)
    return false;

  return alert_delegate->GetMessageText() == GetMessageText();
}

AlertInfoBarDelegate* AlertInfoBarDelegate::AsAlertInfoBarDelegate() {
  return this;
}

AlertInfoBarDelegate::AlertInfoBarDelegate(TabContents* contents)
    : InfoBarDelegate(contents) {
}

// LinkInfoBarDelegate: --------------------------------------------------------

string16 LinkInfoBarDelegate::GetMessageTextWithOffset(
    size_t* link_offset) const {
  *link_offset = string16::npos;
  return string16();
}

SkBitmap* LinkInfoBarDelegate::GetIcon() const {
  return NULL;
}

bool LinkInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  return true;
}

LinkInfoBarDelegate* LinkInfoBarDelegate::AsLinkInfoBarDelegate() {
  return this;
}

LinkInfoBarDelegate::LinkInfoBarDelegate(TabContents* contents)
    : InfoBarDelegate(contents) {
}

// ConfirmInfoBarDelegate: -----------------------------------------------------

int ConfirmInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

string16 ConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_OK);
  if (button == BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_CANCEL);
  NOTREACHED();
  return string16();
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

ConfirmInfoBarDelegate* ConfirmInfoBarDelegate::AsConfirmInfoBarDelegate() {
  return this;
}

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate(TabContents* contents)
    : AlertInfoBarDelegate(contents) {
}

// SimpleAlertInfoBarDelegate: -------------------------------------------------

SimpleAlertInfoBarDelegate::SimpleAlertInfoBarDelegate(
    TabContents* contents,
    const string16& message,
    SkBitmap* icon,
    bool auto_expire)
    : AlertInfoBarDelegate(contents),
      message_(message),
      icon_(icon),
      auto_expire_(auto_expire) {
}

bool SimpleAlertInfoBarDelegate::ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const {
  if (auto_expire_)
    return AlertInfoBarDelegate::ShouldExpire(details);

  return false;
}

string16 SimpleAlertInfoBarDelegate::GetMessageText() const {
  return message_;
}

SkBitmap* SimpleAlertInfoBarDelegate::GetIcon() const {
  return icon_;
}

void SimpleAlertInfoBarDelegate::InfoBarClosed() {
  delete this;
}
