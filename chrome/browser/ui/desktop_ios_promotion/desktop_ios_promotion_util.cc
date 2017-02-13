// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"

#include "base/i18n/rtl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace desktop_ios_promotion {

// Default Impression cap. for each entry point.
const int kEntryPointImpressionCap[] = {2, 2, 5, 10};

bool IsEligibleForIOSPromotion(
    PrefService* prefs,
    const syncer::SyncService* sync_service,
    desktop_ios_promotion::PromotionEntryPoint entry_point) {
  // Promotion should only show for english locale.
  std::string locale = base::i18n::GetConfiguredLocale();
  if (locale != "en-US" && locale != "en-CA")
    return false;
  if (!base::FeatureList::IsEnabled(features::kDesktopIOSPromotion) ||
      !sync_service || !sync_service->IsSyncAllowed())
    return false;
  // TODO(crbug.com/676655): Check if the specific entrypoint is enabled by
  // Finch.
  bool is_dismissed =
      prefs->GetBoolean(kEntryPointLocalPrefs[(int)entry_point][1]);
  int show_count =
      prefs->GetInteger(kEntryPointLocalPrefs[(int)entry_point][0]);
  // TODO(crbug.com/676655): Get the impression cap. from Finch and replace the
  // value from the entryPointImpressionCap array.
  if (is_dismissed || show_count >= kEntryPointImpressionCap[(int)entry_point])
    return false;
  bool is_user_eligible = prefs->GetBoolean(prefs::kIOSPromotionEligible);
  bool did_promo_done_before = prefs->GetBoolean(prefs::kIOSPromotionDone);
  return is_user_eligible && !did_promo_done_before;
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

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kIOSPromotionEligible, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kIOSPromotionDone, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

void RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kNumberSavePasswordsBubbleIOSPromoShown,
                                0);
  registry->RegisterBooleanPref(prefs::kSavePasswordsBubbleIOSPromoDismissed,
                                false);
  registry->RegisterIntegerPref(prefs::kNumberBookmarksBubbleIOSPromoShown, 0);
  registry->RegisterBooleanPref(prefs::kBookmarksBubbleIOSPromoDismissed,
                                false);
  registry->RegisterIntegerPref(prefs::kNumberBookmarksFootNoteIOSPromoShown,
                                0);
  registry->RegisterBooleanPref(prefs::kBookmarksFootNoteIOSPromoDismissed,
                                false);
  registry->RegisterIntegerPref(prefs::kNumberHistoryPageIOSPromoShown, 0);
  registry->RegisterBooleanPref(prefs::kHistoryPageIOSPromoDismissed, false);
}

}  // namespace desktop_ios_promotion
