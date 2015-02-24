// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/simple_alert_infobar_delegate.h"

#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"

// static
void SimpleAlertInfoBarDelegate::Create(
    infobars::InfoBarManager* infobar_manager,
    int icon_id,
    const base::string16& message,
    bool auto_expire) {
  infobar_manager->AddInfoBar(
      infobar_manager->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new SimpleAlertInfoBarDelegate(icon_id, message, auto_expire))));
}

SimpleAlertInfoBarDelegate::SimpleAlertInfoBarDelegate(
    int icon_id,
    const base::string16& message,
    bool auto_expire)
    : ConfirmInfoBarDelegate(),
      icon_id_(icon_id),
      message_(message),
      auto_expire_(auto_expire) {
}

SimpleAlertInfoBarDelegate::~SimpleAlertInfoBarDelegate() {
}

int SimpleAlertInfoBarDelegate::GetIconID() const {
  return icon_id_;
}

bool SimpleAlertInfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  return auto_expire_ && ConfirmInfoBarDelegate::ShouldExpireInternal(details);
}

base::string16 SimpleAlertInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SimpleAlertInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
