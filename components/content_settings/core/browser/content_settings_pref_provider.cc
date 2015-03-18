// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

const char kAudioKey[] = "audio";
const char kVideoKey[] = "video";

}  // namespace

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// PrefProvider:
//

// static
void PrefProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kContentSettingsPatternPairs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

PrefProvider::PrefProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs),
      clock_(new base::DefaultClock()) {
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

  pref_change_registrar_.Init(prefs_);
  content_settings_pref_.reset(new ContentSettingsPref(
      prefs_, &pref_change_registrar_, clock_.get(), incognito,
      base::Bind(&PrefProvider::Notify,
                 base::Unretained(this))));

  if (!incognito) {
    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfExceptions",
                         content_settings_pref_->GetNumExceptions());
    // Migrate the obsolete media content setting exceptions to the new
    // settings. This needs to be done after ReadContentSettingsFromPref().
    MigrateObsoleteMediaContentSetting();
  }
}

PrefProvider::~PrefProvider() {
  DCHECK(!prefs_);
}

RuleIterator* PrefProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return content_settings_pref_->GetRuleIterator(content_type,
                                                 resource_identifier,
                                                 incognito);
}

bool PrefProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* in_value) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  return content_settings_pref_->SetWebsiteSetting(primary_pattern,
                                                   secondary_pattern,
                                                   content_type,
                                                   resource_identifier,
                                                   in_value);
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  content_settings_pref_->ClearAllContentSettingsRules(content_type);
}

void PrefProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void PrefProvider::UpdateLastUsage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  content_settings_pref_->UpdateLastUsage(primary_pattern,
                                          secondary_pattern,
                                          content_type);
}

base::Time PrefProvider::GetLastUsage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  return content_settings_pref_->GetLastUsage(primary_pattern,
                                              secondary_pattern,
                                              content_type);
}

// ////////////////////////////////////////////////////////////////////////////
// Private

void PrefProvider::MigrateObsoleteMediaContentSetting() {
  std::vector<Rule> rules_to_delete;
  {
    scoped_ptr<RuleIterator> rule_iterator(GetRuleIterator(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM, std::string(), false));
    while (rule_iterator->HasNext()) {
      // Skip default setting and rules without a value.
      const content_settings::Rule& rule = rule_iterator->Next();
      DCHECK(rule.primary_pattern != ContentSettingsPattern::Wildcard());
      if (!rule.value.get())
        continue;
      rules_to_delete.push_back(rule);
    }
  }

  for (std::vector<Rule>::const_iterator it = rules_to_delete.begin();
       it != rules_to_delete.end(); ++it) {
    const base::DictionaryValue* value_dict = NULL;
    if (!it->value->GetAsDictionary(&value_dict) || value_dict->empty())
      return;

    std::string audio_device, video_device;
    value_dict->GetString(kAudioKey, &audio_device);
    value_dict->GetString(kVideoKey, &video_device);
    // Add the exception to the new microphone content setting.
    if (!audio_device.empty()) {
      SetWebsiteSetting(it->primary_pattern,
                        it->secondary_pattern,
                        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                        std::string(),
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
    }
    // Add the exception to the new camera content setting.
    if (!video_device.empty()) {
      SetWebsiteSetting(it->primary_pattern,
                        it->secondary_pattern,
                        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                        std::string(),
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
    }

    // Remove the old exception in CONTENT_SETTINGS_TYPE_MEDIASTREAM.
    SetWebsiteSetting(it->primary_pattern,
                      it->secondary_pattern,
                      CONTENT_SETTINGS_TYPE_MEDIASTREAM,
                      std::string(),
                      NULL);
  }
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

void PrefProvider::SetClockForTesting(scoped_ptr<base::Clock> clock) {
  clock_ = clock.Pass();
  content_settings_pref_->SetClockForTesting(clock_.get());
}

}  // namespace content_settings
