// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_pref_store.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/supervised_user/supervised_user_bookmarks_handler.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"

namespace {

struct SupervisedUserSettingsPrefMappingEntry {
  const char* settings_name;
  const char* pref_name;
};

SupervisedUserSettingsPrefMappingEntry kSupervisedUserSettingsPrefMapping[] = {
  {
    supervised_users::kContentPackDefaultFilteringBehavior,
    prefs::kDefaultSupervisedUserFilteringBehavior,
  },
  {
    supervised_users::kContentPackManualBehaviorHosts,
    prefs::kSupervisedUserManualHosts,
  },
  {
    supervised_users::kContentPackManualBehaviorURLs,
    prefs::kSupervisedUserManualURLs,
  },
  {
    supervised_users::kForceSafeSearch, prefs::kForceGoogleSafeSearch,
  },
  {
    supervised_users::kForceSafeSearch, prefs::kForceYouTubeSafetyMode,
  },
  {
    supervised_users::kRecordHistory, prefs::kRecordHistory,
  },
  {
    supervised_users::kSigninAllowed, prefs::kSigninAllowed,
  },
  {
    supervised_users::kUserName, prefs::kProfileName,
  },
};

}  // namespace

SupervisedUserPrefStore::SupervisedUserPrefStore(
    SupervisedUserSettingsService* supervised_user_settings_service)
    : weak_ptr_factory_(this) {
  supervised_user_settings_service->Subscribe(
      base::Bind(&SupervisedUserPrefStore::OnNewSettingsAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool SupervisedUserPrefStore::GetValue(const std::string& key,
                                       const base::Value** value) const {
  // TODO(bauerb): Temporary CHECK to force a clean crash while investigating
  // https://crbug.com/425785. Remove (or change back to DCHECK) once the bug
  // is fixed.
  CHECK(prefs_);
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
    // Set hardcoded prefs and defaults.
    prefs_->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
    prefs_->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                       SupervisedUserURLFilter::ALLOW);
    prefs_->SetBoolean(prefs::kForceGoogleSafeSearch, true);
    prefs_->SetBoolean(prefs::kForceYouTubeSafetyMode, true);
    prefs_->SetBoolean(prefs::kHideWebStoreIcon, true);
    prefs_->SetInteger(prefs::kIncognitoModeAvailability,
                       IncognitoModePrefs::DISABLED);
    prefs_->SetBoolean(prefs::kRecordHistory, true);
    prefs_->SetBoolean(prefs::kSigninAllowed, false);

    // Copy supervised user settings to prefs.
    for (const auto& entry : kSupervisedUserSettingsPrefMapping) {
      const base::Value* value = NULL;
      if (settings->GetWithoutPathExpansion(entry.settings_name, &value))
        prefs_->SetValue(entry.pref_name, value->DeepCopy());
    }

    // Manually set preferences that aren't direct copies of the settings value.
    bool record_history;
    if (settings->GetBoolean(supervised_users::kRecordHistory,
                             &record_history)) {
      prefs_->SetBoolean(prefs::kAllowDeletingBrowserHistory, !record_history);
      prefs_->SetInteger(prefs::kIncognitoModeAvailability,
                         record_history ? IncognitoModePrefs::DISABLED
                                        : IncognitoModePrefs::ENABLED);
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableSupervisedUserManagedBookmarksFolder)) {
      // Reconstruct bookmarks from split settings.
      prefs_->SetValue(
          bookmarks::prefs::kSupervisedBookmarks,
          SupervisedUserBookmarksHandler::BuildBookmarksTree(*settings)
              .release());
    }
  }

  if (!old_prefs) {
    FOR_EACH_OBSERVER(Observer, observers_, OnInitializationCompleted(true));
    return;
  }

  std::vector<std::string> changed_prefs;
  prefs_->GetDifferingKeys(old_prefs.get(), &changed_prefs);

  // Send out change notifications.
  for (const std::string& pref : changed_prefs) {
    FOR_EACH_OBSERVER(Observer, observers_, OnPrefValueChanged(pref));
  }
}
