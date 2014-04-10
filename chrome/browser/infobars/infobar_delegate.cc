// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"

using content::NavigationEntry;

// InfoBarDelegate ------------------------------------------------------------

const int InfoBarDelegate::kNoIconID = 0;

InfoBarDelegate::~InfoBarDelegate() {
}

InfoBarDelegate::InfoBarAutomationType
    InfoBarDelegate::GetInfoBarAutomationType() const {
  return UNKNOWN_INFOBAR;
}

bool InfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  return false;
}

bool InfoBarDelegate::ShouldExpire(const NavigationDetails& details) const {
  if (!details.is_navigation_to_different_page)
    return false;

  return ShouldExpireInternal(details);
}

void InfoBarDelegate::InfoBarDismissed() {
}

int InfoBarDelegate::GetIconID() const {
  return kNoIconID;
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

PopupBlockedInfoBarDelegate* InfoBarDelegate::AsPopupBlockedInfoBarDelegate() {
  return NULL;
}

RegisterProtocolHandlerInfoBarDelegate*
    InfoBarDelegate::AsRegisterProtocolHandlerInfoBarDelegate() {
  return NULL;
}

ScreenCaptureInfoBarDelegate*
    InfoBarDelegate::AsScreenCaptureInfoBarDelegate() {
  return NULL;
}

ThemeInstalledInfoBarDelegate*
    InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return NULL;
}

TranslateInfoBarDelegate* InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return NULL;
}

void InfoBarDelegate::StoreActiveEntryUniqueID() {
  DCHECK(web_contents());
  NavigationEntry* active_entry =
      web_contents()->GetController().GetActiveEntry();
  contents_unique_id_ = active_entry ? active_entry->GetUniqueID() : 0;
}

gfx::Image InfoBarDelegate::GetIcon() const {
  int icon_id = GetIconID();
  return (icon_id == kNoIconID) ? gfx::Image() :
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(icon_id);
}

content::WebContents* InfoBarDelegate::web_contents() {
  return InfoBarService::WebContentsFromInfoBar(infobar_);
}

InfoBarDelegate::InfoBarDelegate() : contents_unique_id_(0) {
}

bool InfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  // NOTE: If you change this, be sure to check and adjust the behavior of
  // anyone who overrides this as necessary!
  return (contents_unique_id_ != details.entry_id) || details.is_reload;
}
