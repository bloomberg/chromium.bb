// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/infobars/one_click_signin_infobar_delegate.h"

OneClickSigninInfoBarDelegate::OneClickSigninInfoBarDelegate(
    InfoBarService* infobar_service)
    : ConfirmInfoBarDelegate(infobar_service) {
}

OneClickSigninInfoBarDelegate::~OneClickSigninInfoBarDelegate() {
}

void OneClickSigninInfoBarDelegate::GetAlternateColors(
    AlternateColors* alt_colors) {
  alt_colors->enabled = false;
}
