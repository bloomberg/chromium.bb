// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_UTIL_H_
#define CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_UTIL_H_

#include "base/macros.h"
#include "chrome/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace syncer {
class SyncService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefRegistrySimple;
class PrefService;

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
  BOOKMARKS_FOOTNOTE,
  HISTORY_PAGE
};

// Entry points local prefs, each entry point has a preference for impressions
// count and a preference for whether user dismissed it or not.
// Do not change the order of this array, as it's indexed using
// desktop_ios_promotion::PromotionEntryPoint.
const char* const kEntryPointLocalPrefs[4][2] = {
    {prefs::kNumberSavePasswordsBubbleIOSPromoShown,
     prefs::kSavePasswordsBubbleIOSPromoDismissed},
    {prefs::kNumberBookmarksBubbleIOSPromoShown,
     prefs::kBookmarksBubbleIOSPromoDismissed},
    {prefs::kNumberBookmarksFootNoteIOSPromoShown,
     prefs::kBookmarksFootNoteIOSPromoDismissed},
    {prefs::kNumberHistoryPageIOSPromoShown,
     prefs::kHistoryPageIOSPromoDismissed}};

bool IsEligibleForIOSPromotion(PrefService* prefs,
                               const syncer::SyncService* sync_service,
                               desktop_ios_promotion::PromotionEntryPoint);

// Returns the Bubble text based on the promotion entry point.
base::string16 GetPromoBubbleText(
    desktop_ios_promotion::PromotionEntryPoint entry_point);

// Register all Priority Sync preferences.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Register all local only Preferences.
void RegisterLocalPrefs(PrefRegistrySimple* registry);

}  // namespace desktop_ios_promotion

#endif  // CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_UTIL_H_
