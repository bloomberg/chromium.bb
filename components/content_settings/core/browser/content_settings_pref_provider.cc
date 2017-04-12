// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace content_settings {

namespace {

// These settings are no longer used, and should be deleted on profile startup.
#if !defined(OS_IOS)
const char kObsoleteFullscreenExceptionsPref[] =
    "profile.content_settings.exceptions.fullscreen";
#if !defined(OS_ANDROID)
const char kObsoleteMouseLockExceptionsPref[] =
    "profile.content_settings.exceptions.mouselock";
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_IOS)
const char kObsoleteLastUsed[] = "last_used";

}  // namespace

// ////////////////////////////////////////////////////////////////////////////
// PrefProvider:
//

// static
void PrefProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion);

  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings) {
    registry->RegisterDictionaryPref(info->pref_name(),
                                     info->GetPrefRegistrationFlags());
  }

  // Obsolete prefs ----------------------------------------------------------

  // These prefs have been removed, but need to be registered so they can
  // be deleted on startup.
#if !defined(OS_IOS)
  registry->RegisterDictionaryPref(
      kObsoleteFullscreenExceptionsPref,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if !defined(OS_ANDROID)
  registry->RegisterDictionaryPref(
      kObsoleteMouseLockExceptionsPref,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_IOS)
}

PrefProvider::PrefProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs),
      is_incognito_(incognito) {
  DCHECK(prefs_);
  // Verify preferences version.
  if (!prefs_->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs_->SetInteger(prefs::kContentSettingsVersion,
                       ContentSettingsPattern::kContentSettingsPatternVersion);
  }
  if (prefs_->GetInteger(prefs::kContentSettingsVersion) >
      ContentSettingsPattern::kContentSettingsPatternVersion) {
    return;
  }

  DiscardObsoletePreferences();

  pref_change_registrar_.Init(prefs_);

  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings) {
    content_settings_prefs_.insert(std::make_pair(
        info->type(),
        base::MakeUnique<ContentSettingsPref>(
            info->type(), prefs_, &pref_change_registrar_, info->pref_name(),
            is_incognito_,
            base::Bind(&PrefProvider::Notify, base::Unretained(this)))));
  }

  if (!is_incognito_) {
    size_t num_exceptions = 0;
    for (const auto& pref : content_settings_prefs_)
      num_exceptions += pref.second->GetNumExceptions();

    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfExceptions",
                         num_exceptions);
  }
}

PrefProvider::~PrefProvider() {
  DCHECK(!prefs_);
}

std::unique_ptr<RuleIterator> PrefProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return GetPref(content_type)->GetRuleIterator(resource_identifier, incognito);
}

bool PrefProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* in_value) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  // Default settings are set using a wildcard pattern for both
  // |primary_pattern| and |secondary_pattern|. Don't store default settings in
  // the |PrefProvider|. The |PrefProvider| handles settings for specific
  // sites/origins defined by the |primary_pattern| and the |secondary_pattern|.
  // Default settings are handled by the |DefaultProvider|.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      resource_identifier.empty()) {
    return false;
  }

  return GetPref(content_type)
      ->SetWebsiteSetting(primary_pattern, secondary_pattern,
                          resource_identifier, in_value);
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  GetPref(content_type)->ClearAllContentSettingsRules();
}

void PrefProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void PrefProvider::ClearPrefs() {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  for (const auto& pref : content_settings_prefs_)
    pref.second->ClearPref();
}

ContentSettingsPref* PrefProvider::GetPref(ContentSettingsType type) const {
  auto it = content_settings_prefs_.find(type);
  DCHECK(it != content_settings_prefs_.end());
  return it->second.get();
}

void PrefProvider::Notify(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  NotifyObservers(primary_pattern,
                  secondary_pattern,
                  content_type,
                  resource_identifier);
}

void PrefProvider::DiscardObsoletePreferences() {
  // These prefs were never stored on iOS/Android so they don't need to be
  // deleted.
#if !defined(OS_IOS)
  prefs_->ClearPref(kObsoleteFullscreenExceptionsPref);
#if !defined(OS_ANDROID)
  prefs_->ClearPref(kObsoleteMouseLockExceptionsPref);
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_IOS)

#if !defined(OS_IOS)
  // Migrate CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT to
  // CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA.
  // TODO(raymes): See crbug.com/681709. Remove after M60.
  const std::string prompt_no_decision_count_pref =
      WebsiteSettingsRegistry::GetInstance()
          ->Get(CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT)
          ->pref_name();
  const base::DictionaryValue* old_dict =
      prefs_->GetDictionary(prompt_no_decision_count_pref);

  const std::string permission_autoblocker_data_pref =
      WebsiteSettingsRegistry::GetInstance()
          ->Get(CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA)
          ->pref_name();
  const base::DictionaryValue* new_dict =
      prefs_->GetDictionary(permission_autoblocker_data_pref);

  if (!old_dict->empty() && new_dict->empty())
    prefs_->Set(permission_autoblocker_data_pref, *old_dict);
  prefs_->ClearPref(prompt_no_decision_count_pref);
#endif  // !defined(OS_IOS)

  // TODO(timloh): See crbug.com/691893. This removal code was added in M58,
  // so is probably fine to remove in M60 or later.
  for (const WebsiteSettingsInfo* info :
       *WebsiteSettingsRegistry::GetInstance()) {
    if (!prefs_->GetDictionary(info->pref_name()))
      continue;

    DictionaryPrefUpdate update(prefs_, info->pref_name());
    base::DictionaryValue* all_settings = update.Get();
    std::vector<std::string> values_to_clean;
    for (base::DictionaryValue::Iterator i(*all_settings); !i.IsAtEnd();
         i.Advance()) {
      const base::DictionaryValue* pattern_settings = nullptr;
      bool is_dictionary = i.value().GetAsDictionary(&pattern_settings);
      DCHECK(is_dictionary);
      if (pattern_settings->GetWithoutPathExpansion(kObsoleteLastUsed, nullptr))
        values_to_clean.push_back(i.key());
    }

    for (const std::string& key : values_to_clean) {
      base::DictionaryValue* pattern_settings = nullptr;
      all_settings->GetDictionaryWithoutPathExpansion(key, &pattern_settings);
      pattern_settings->RemoveWithoutPathExpansion(kObsoleteLastUsed, nullptr);
      if (pattern_settings->empty())
        all_settings->RemoveWithoutPathExpansion(key, nullptr);
    }
  }
}

}  // namespace content_settings
