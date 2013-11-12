// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/supervised_user_pref_store.h"

#include "base/bind.h"
#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/common/pref_names.h"

using base::FundamentalValue;

namespace {

struct ManagedUserSettingsPrefMappingEntry {
  const char* settings_name;
  const char* pref_name;
};

ManagedUserSettingsPrefMappingEntry kManagedUserSettingsPrefMapping[] = {
  {
    managed_users::kContentPackDefaultFilteringBehavior,
    prefs::kDefaultManagedModeFilteringBehavior,
  },
  {
    managed_users::kContentPackManualBehaviorHosts,
    prefs::kManagedModeManualHosts,
  },
  {
    managed_users::kContentPackManualBehaviorURLs,
    prefs::kManagedModeManualURLs,
  },
  {
    managed_users::kForceSafeSearch,
    prefs::kForceSafeSearch,
  },
  {
    managed_users::kUserName,
    prefs::kProfileName,
  },
};

}  // namespace

SupervisedUserPrefStore::SupervisedUserPrefStore(
    ManagedUserSettingsService* managed_user_settings_service)
    : weak_ptr_factory_(this) {
  managed_user_settings_service->Subscribe(
      base::Bind(&SupervisedUserPrefStore::OnNewSettingsAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool SupervisedUserPrefStore::GetValue(const std::string& key,
                                       const base::Value** value) const {
  DCHECK(prefs_);
  return prefs_->GetValue(key, value);
}

void SupervisedUserPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SupervisedUserPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool SupervisedUserPrefStore::IsInitializationComplete() const {
  return !!prefs_;
}

SupervisedUserPrefStore::~SupervisedUserPrefStore() {
}

void SupervisedUserPrefStore::OnNewSettingsAvailable(
    const base::DictionaryValue* settings) {
  scoped_ptr<PrefValueMap> old_prefs = prefs_.Pass();
  prefs_.reset(new PrefValueMap);
  if (settings) {
    // Set hardcoded prefs.
    prefs_->SetValue(prefs::kAllowDeletingBrowserHistory,
                     new FundamentalValue(false));
    prefs_->SetValue(prefs::kDefaultManagedModeFilteringBehavior,
                     new FundamentalValue(ManagedModeURLFilter::ALLOW));
    prefs_->SetValue(prefs::kForceSafeSearch, new FundamentalValue(true));
    prefs_->SetValue(prefs::kHideWebStoreIcon, new FundamentalValue(true));
    prefs_->SetValue(prefs::kIncognitoModeAvailability,
                     new FundamentalValue(IncognitoModePrefs::DISABLED));
    prefs_->SetValue(prefs::kSigninAllowed, new FundamentalValue(false));

    // Copy managed user settings to prefs.
    for (size_t i = 0; i < arraysize(kManagedUserSettingsPrefMapping); ++i) {
      const ManagedUserSettingsPrefMappingEntry& entry =
          kManagedUserSettingsPrefMapping[i];
      const base::Value* value = NULL;
      if (settings->GetWithoutPathExpansion(entry.settings_name, &value))
        prefs_->SetValue(entry.pref_name, value->DeepCopy());
    }
  }

  if (!old_prefs) {
    FOR_EACH_OBSERVER(Observer, observers_, OnInitializationCompleted(true));
    return;
  }

  std::vector<std::string> changed_prefs;
  prefs_->GetDifferingKeys(old_prefs.get(), &changed_prefs);

  // Send out change notifications.
  for (std::vector<std::string>::const_iterator pref(changed_prefs.begin());
       pref != changed_prefs.end();
       ++pref) {
    FOR_EACH_OBSERVER(Observer, observers_, OnPrefValueChanged(*pref));
  }
}
