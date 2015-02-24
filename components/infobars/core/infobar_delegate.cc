// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/resource/resource_bundle.h"

namespace infobars {

const int InfoBarDelegate::kNoIconID = 0;

InfoBarDelegate::~InfoBarDelegate() {
}

InfoBarDelegate::InfoBarAutomationType
    InfoBarDelegate::GetInfoBarAutomationType() const {
  return UNKNOWN_INFOBAR;
}

InfoBarDelegate::Type InfoBarDelegate::GetInfoBarType() const {
  return WARNING_TYPE;
}

int InfoBarDelegate::GetIconID() const {
  return kNoIconID;
}

gfx::Image InfoBarDelegate::GetIcon() const {
  int icon_id = GetIconID();
  return (icon_id == kNoIconID) ? gfx::Image() :
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(icon_id);
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

AutoLoginInfoBarDelegate* InfoBarDelegate::AsAutoLoginInfoBarDelegate() {
  return nullptr;
}

ConfirmInfoBarDelegate* InfoBarDelegate::AsConfirmInfoBarDelegate() {
  return nullptr;
}

InsecureContentInfoBarDelegate*
    InfoBarDelegate::AsInsecureContentInfoBarDelegate() {
  return nullptr;
}

MediaStreamInfoBarDelegate* InfoBarDelegate::AsMediaStreamInfoBarDelegate() {
  return nullptr;
}

NativeAppInfoBarDelegate* InfoBarDelegate::AsNativeAppInfoBarDelegate() {
  return nullptr;
}

PopupBlockedInfoBarDelegate* InfoBarDelegate::AsPopupBlockedInfoBarDelegate() {
  return nullptr;
}

RegisterProtocolHandlerInfoBarDelegate*
    InfoBarDelegate::AsRegisterProtocolHandlerInfoBarDelegate() {
  return nullptr;
}

ScreenCaptureInfoBarDelegate*
    InfoBarDelegate::AsScreenCaptureInfoBarDelegate() {
  return nullptr;
}

ThemeInstalledInfoBarDelegate*
    InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return nullptr;
}

ThreeDAPIInfoBarDelegate* InfoBarDelegate::AsThreeDAPIInfoBarDelegate() {
  return nullptr;
}

translate::TranslateInfoBarDelegate*
InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return nullptr;
}

void InfoBarDelegate::StoreActiveEntryUniqueID() {
  contents_unique_id_ = infobar()->owner()->GetActiveEntryID();
}

InfoBarDelegate::InfoBarDelegate() : contents_unique_id_(0) {
}

bool InfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  // NOTE: If you change this, be sure to check and adjust the behavior of
  // anyone who overrides this as necessary!
  return (contents_unique_id_ != details.entry_id) || details.is_reload;
}

}  // namespace infobars
