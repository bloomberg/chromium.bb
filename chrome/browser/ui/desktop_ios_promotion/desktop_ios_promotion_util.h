// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_UTIL_H_
#define CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_UTIL_H_

#include "base/macros.h"
#include "ui/base/l10n/l10n_util.h"

namespace desktop_ios_promotion {

// Represents the reason that the Desktop to iOS promotion was dismissed for.
enum class PromotionDismissalReason {
  FOCUS_LOST = 0,
  TAB_SWITCHED,
  NO_THANKS,
  CLOSE_BUTTON,
  SEND_SMS,
  LEARN_MORE_CLICKED,
};

// The place where the promotion appeared.
enum class PromotionEntryPoint {
  SAVE_PASSWORD_BUBBLE = 0,
  BOOKMARKS_BUBBLE,
  BOOKMARKS_LINK,
  HISTORY_PAGE
};

bool IsEligibleForIOSPromotion();

// Returns the Bubble text based on the promotion entry point.
base::string16 GetPromoBubbleText(
    desktop_ios_promotion::PromotionEntryPoint entry_point);

}  // namespace desktop_ios_promotion

#endif  // CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_UTIL_H_
