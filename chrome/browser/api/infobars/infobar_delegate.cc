// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/infobars/infobar_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/api/infobars/infobar_service.h"
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

ThreeDAPIInfoBarDelegate* InfoBarDelegate::AsThreeDAPIInfoBarDelegate() {
  return NULL;
}

TranslateInfoBarDelegate* InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return NULL;
}

InfoBarDelegate::InfoBarDelegate(InfoBarService* infobar_service)
    : contents_unique_id_(0),
      owner_(infobar_service) {
  if (infobar_service)
    StoreActiveEntryUniqueID();
}

void InfoBarDelegate::StoreActiveEntryUniqueID() {
  content::WebContents* web_contents = owner_->GetWebContents();
  DCHECK(web_contents);
  NavigationEntry* active_entry =
      web_contents->GetController().GetActiveEntry();
  contents_unique_id_ = active_entry ? active_entry->GetUniqueID() : 0;
}

bool InfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  // NOTE: If you change this, be sure to check and adjust the behavior of
  // anyone who overrides this as necessary!
  return (contents_unique_id_ != details.entry->GetUniqueID()) ||
      (content::PageTransitionStripQualifier(
          details.entry->GetTransitionType()) ==
              content::PAGE_TRANSITION_RELOAD);
}

void InfoBarDelegate::RemoveSelf() {
  if (owner_)
    owner_->RemoveInfoBar(this);  // Clears |owner_|.
}
