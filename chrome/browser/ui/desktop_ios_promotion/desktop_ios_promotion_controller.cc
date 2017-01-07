// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_controller.h"

DesktopIOSPromotionController::DesktopIOSPromotionController() {}

DesktopIOSPromotionController::~DesktopIOSPromotionController() {}

void DesktopIOSPromotionController::OnSendSMSClicked() {
  // TODO(crbug.com/676655): Call the growth api to send sms.
}

void DesktopIOSPromotionController::OnNoThanksClicked() {
  // TODO(crbug.com/676655): Handle logging & update sync.
}
