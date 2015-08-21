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
  return details.is_navigation_to_different_page &&
      !details.did_replace_entry &&
      // This next condition ensures a navigation that passes the above
      // conditions doesn't dismiss infobars added while that navigation was
      // already in process.  We carve out an exception for reloads since we
      // want reloads to dismiss infobars, but they will have unchanged entry
      // IDs.
      ((nav_entry_id_ != details.entry_id) || details.is_reload);
}

void InfoBarDelegate::InfoBarDismissed() {
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

PermissionInfobarDelegate* InfoBarDelegate::AsPermissionInfobarDelegate() {
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

InfoBarDelegate::InfoBarDelegate() : nav_entry_id_(0) {
}

}  // namespace infobars
