// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/background_budget_service.h"

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

std::string GetBudgetStringForOrigin(Profile* profile, const GURL& origin) {
  DCHECK_EQ(origin.GetOrigin(), origin);
  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kBackgroundBudgetMap);

  std::string map_string;
  map->GetStringWithoutPathExpansion(origin.spec(), &map_string);
  return map_string;
}

}  // namespace

BackgroundBudgetService::BackgroundBudgetService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
}

// static
void BackgroundBudgetService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kBackgroundBudgetMap);
}

std::string BackgroundBudgetService::GetBudget(const GURL& origin) {
  return GetBudgetStringForOrigin(profile_, origin);
}

void BackgroundBudgetService::StoreBudget(
    const GURL& origin,
    const std::string& notifications_shown) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kBackgroundBudgetMap);
  base::DictionaryValue* map = update.Get();
  map->SetStringWithoutPathExpansion(origin.spec(), notifications_shown);
}
