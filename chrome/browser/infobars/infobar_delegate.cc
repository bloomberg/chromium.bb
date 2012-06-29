// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

// InfoBarDelegate ------------------------------------------------------------

InfoBarDelegate::~InfoBarDelegate() {
}

InfoBarDelegate::InfoBarAutomationType
    InfoBarDelegate::GetInfoBarAutomationType() const {
  return UNKNOWN_INFOBAR;
}

bool InfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  return false;
}

bool InfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  if (!details.is_navigation_to_different_page())
    return false;

  return ShouldExpireInternal(details);
}

void InfoBarDelegate::InfoBarDismissed() {
}

void InfoBarDelegate::InfoBarClosed() {
  delete this;
}

gfx::Image* InfoBarDelegate::GetIcon() const {
  return NULL;
}

InfoBarDelegate::Type InfoBarDelegate::GetInfoBarType() const {
  return WARNING_TYPE;
}

AutoLoginInfoBarDelegate* InfoBarDelegate::AsAutoLoginInfoBarDelegate() {
  return NULL;
}

ConfirmInfoBarDelegate* InfoBarDelegate::AsConfirmInfoBarDelegate() {
  return NULL;
}

ExtensionInfoBarDelegate* InfoBarDelegate::AsExtensionInfoBarDelegate() {
  return NULL;
}

InsecureContentInfoBarDelegate*
    InfoBarDelegate::AsInsecureContentInfoBarDelegate() {
  return NULL;
}

LinkInfoBarDelegate* InfoBarDelegate::AsLinkInfoBarDelegate() {
  return NULL;
}

MediaStreamInfoBarDelegate* InfoBarDelegate::AsMediaStreamInfoBarDelegate() {
  return NULL;
}

RegisterProtocolHandlerInfoBarDelegate*
    InfoBarDelegate::AsRegisterProtocolHandlerInfoBarDelegate() {
  return NULL;
}

ThemeInstalledInfoBarDelegate*
    InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return NULL;
}

TranslateInfoBarDelegate* InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return NULL;
}

InfoBarDelegate::InfoBarDelegate(InfoBarTabHelper* infobar_helper)
    : contents_unique_id_(0),
      owner_(infobar_helper) {
  if (infobar_helper)
    StoreActiveEntryUniqueID(infobar_helper);
}

void InfoBarDelegate::StoreActiveEntryUniqueID(
    InfoBarTabHelper* infobar_helper) {
  NavigationEntry* active_entry =
      infobar_helper->web_contents()->GetController().GetActiveEntry();
  contents_unique_id_ = active_entry ? active_entry->GetUniqueID() : 0;
}

bool InfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  return (contents_unique_id_ != details.entry->GetUniqueID()) ||
      (content::PageTransitionStripQualifier(
          details.entry->GetTransitionType()) ==
              content::PAGE_TRANSITION_RELOAD);
}

void InfoBarDelegate::RemoveSelf() {
  if (owner_)
    owner_->RemoveInfoBar(this);  // Clears |owner_|.
}
