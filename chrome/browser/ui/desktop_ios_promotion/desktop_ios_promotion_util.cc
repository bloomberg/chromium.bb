// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"
#include "chrome/grit/generated_resources.h"

namespace desktop_ios_promotion {

bool IsEligibleForIOSPromotion() {
  return false;
}

base::string16 GetPromoBubbleText(
    desktop_ios_promotion::PromotionEntryPoint entry_point) {
  if (entry_point ==
      desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_DESKTOP_TO_IOS_PROMO_TEXT);
  }
  return base::string16();
}

}  // namespace desktop_ios_promotion
