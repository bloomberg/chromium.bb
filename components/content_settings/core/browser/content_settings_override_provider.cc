// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_override_provider.h"

#include <string>

#include "base/auto_reset.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_binary_value_map.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace content_settings {

// static
void OverrideProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kOverrideContentSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

OverrideProvider::OverrideProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs), is_incognito_(incognito) {
  DCHECK(prefs_);

  // Read global overrides.
  ReadOverrideSettings();
}

OverrideProvider::~OverrideProvider() {
}

RuleIterator* OverrideProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  scoped_ptr<base::AutoLock> auto_lock(new base::AutoLock(lock_));
  return allowed_settings_.GetRuleIterator(content_type, resource_identifier,
                                           auto_lock.Pass());
}

void OverrideProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
}

bool OverrideProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* in_value) {
  return false;
}

void OverrideProvider::ShutdownOnUIThread() {
  DCHECK(prefs_);
  prefs_ = NULL;
}

void OverrideProvider::SetOverrideSetting(ContentSettingsType content_type,
                                          bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  // Disallow incognito to change the state.
  DCHECK(!is_incognito_);

  base::AutoLock auto_lock(lock_);
  DictionaryPrefUpdate update(prefs_, prefs::kOverrideContentSettings);
  base::DictionaryValue* default_settings_dictionary = update.Get();
  allowed_settings_.SetContentSettingDisabled(content_type, !enabled);
  if (enabled) {
    default_settings_dictionary->RemoveWithoutPathExpansion(
        GetTypeName(content_type), NULL);
  } else {
    default_settings_dictionary->SetWithoutPathExpansion(
        GetTypeName(content_type), new base::FundamentalValue(true));
  }
}

bool OverrideProvider::IsEnabled(ContentSettingsType content_type) const {
  base::AutoLock auto_lock(lock_);
  return allowed_settings_.IsContentSettingEnabled(content_type);
}

void OverrideProvider::ReadOverrideSettings() {
  const base::DictionaryValue* blocked_settings_dictionary =
      prefs_->GetDictionary(prefs::kOverrideContentSettings);

  for (int type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    ContentSettingsType content_type = ContentSettingsType(type);
    if (blocked_settings_dictionary->HasKey(GetTypeName(content_type))) {
      allowed_settings_.SetContentSettingDisabled(content_type, true);
    }
  }
}

}  // namespace content_settings
