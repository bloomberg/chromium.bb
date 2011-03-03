// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"

#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "third_party/skia/include/core/SkBitmap.h"

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
