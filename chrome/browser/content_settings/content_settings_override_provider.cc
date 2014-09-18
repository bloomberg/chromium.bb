// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_override_provider.h"

#include <string>

#include "base/auto_reset.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace content_settings {

namespace {

class OverrideRuleIterator : public RuleIterator {
 public:
  explicit OverrideRuleIterator(bool is_allowed) : is_done_(is_allowed) {}

  virtual bool HasNext() const OVERRIDE { return !is_done_; }

  virtual Rule Next() OVERRIDE {
    DCHECK(!is_done_);
    is_done_ = true;
    return Rule(ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::Wildcard(),
                new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  }

 private:
  bool is_done_;
};

}  // namespace

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
  base::AutoLock lock(lock_);
  if (resource_identifier.empty()) {
    return new OverrideRuleIterator(allowed_settings_[content_type]);
  }
  return new EmptyRuleIterator();
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prefs_);

  // Disallow incognito to change the state.
  DCHECK(!is_incognito_);

  base::AutoLock lock(lock_);
  DictionaryPrefUpdate update(prefs_, prefs::kOverrideContentSettings);
  base::DictionaryValue* default_settings_dictionary = update.Get();
  if (enabled) {
    allowed_settings_[content_type] = true;
    default_settings_dictionary->RemoveWithoutPathExpansion(
        GetTypeName(content_type), NULL);
  } else {
    allowed_settings_[content_type] = false;
    default_settings_dictionary->SetWithoutPathExpansion(
        GetTypeName(content_type), new base::FundamentalValue(true));
  }
}

bool OverrideProvider::IsEnabled(ContentSettingsType content_type) const {
  base::AutoLock lock(lock_);
  return allowed_settings_[content_type];
}

void OverrideProvider::ReadOverrideSettings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const base::DictionaryValue* blocked_settings_dictionary =
      prefs_->GetDictionary(prefs::kOverrideContentSettings);

  for (int type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    ContentSettingsType content_setting = ContentSettingsType(type);
    allowed_settings_[content_setting] =
        !blocked_settings_dictionary->HasKey(GetTypeName(content_setting));
  }
}

}  // namespace content_settings
