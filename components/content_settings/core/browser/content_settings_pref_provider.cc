// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <map>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_registry.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
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

const char kPerPluginPrefName[] = "per_plugin";

// Returns true and sets |pref_key| to the key in the content settings
// dictionary under which per-resource content settings are stored,
// if the given content type supports resource identifiers in user preferences.
bool GetResourceTypeName(ContentSettingsType content_type,
                         std::string* pref_key) {
  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    *pref_key = kPerPluginPrefName;
    return true;
  }
  return false;
}

// TODO(msramek): Check if we still need this migration code. If not, remove it.
ContentSetting FixObsoleteCookiePromptMode(ContentSettingsType content_type,
                                           ContentSetting setting) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES &&
      setting == CONTENT_SETTING_ASK) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

// A helper function to duplicate |ContentSettingsPattern|, so that
// |ReadContentSettingsFromOldPref| can export them in a vector. We cannot pass
// them by pointer, because the original values will go out of scope when
// the vector is used in |WriteSettingsToNewPreferences|.
ContentSettingsPattern CopyPattern(const ContentSettingsPattern& pattern) {
  return ContentSettingsPattern::FromString(pattern.ToString());
}

const char kAudioKey[] = "audio";
const char kVideoKey[] = "video";

// A list of exception preferences corresponding to individual content settings
// types. Must be kept in sync with the enum |ContentSettingsType|.
const char* kContentSettingsExceptionsPrefs[] = {
    prefs::kContentSettingsCookiesPatternPairs,
    prefs::kContentSettingsImagesPatternPairs,
    prefs::kContentSettingsJavaScriptPatternPairs,
    prefs::kContentSettingsPluginsPatternPairs,
    prefs::kContentSettingsPopupsPatternPairs,
    prefs::kContentSettingsGeolocationPatternPairs,
    prefs::kContentSettingsNotificationsPatternPairs,
    prefs::kContentSettingsAutoSelectCertificatePatternPairs,
    prefs::kContentSettingsFullScreenPatternPairs,
    prefs::kContentSettingsMouseLockPatternPairs,
    prefs::kContentSettingsMixedScriptPatternPairs,
    prefs::kContentSettingsMediaStreamPatternPairs,
    prefs::kContentSettingsMediaStreamMicPatternPairs,
    prefs::kContentSettingsMediaStreamCameraPatternPairs,
    prefs::kContentSettingsProtocolHandlersPatternPairs,
    prefs::kContentSettingsPpapiBrokerPatternPairs,
    prefs::kContentSettingsAutomaticDownloadsPatternPairs,
    prefs::kContentSettingsMidiSysexPatternPairs,
    prefs::kContentSettingsPushMessagingPatternPairs,
    prefs::kContentSettingsSSLCertDecisionsPatternPairs,
#if defined(OS_WIN)
    prefs::kContentSettingsMetroSwitchToDesktopPatternPairs,
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
    prefs::kContentSettingsProtectedMediaIdentifierPatternPairs,
#endif
    prefs::kContentSettingsAppBannerPatternPairs
};
static_assert(arraysize(kContentSettingsExceptionsPrefs)
              == CONTENT_SETTINGS_NUM_TYPES,
              "kContentSettingsExceptionsPrefs should have "
              "CONTENT_SETTINGS_NUM_TYPES elements");

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
      ContentSettingsPattern::kContentSettingsPatternVersion);
  registry->RegisterDictionaryPref(
      prefs::kContentSettingsPatternPairs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMigratedContentSettingsPatternPairs,
                                false);

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    registry->RegisterDictionaryPref(
        kContentSettingsExceptionsPrefs[i],
        PrefRegistrationFlagsForType(ContentSettingsType(i)));
  }
}

PrefProvider::PrefProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs),
      clock_(new base::DefaultClock()),
      is_incognito_(incognito),
      updating_old_preferences_(false) {
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

  pref_change_registrar_.Add(prefs::kContentSettingsPatternPairs, base::Bind(
      &PrefProvider::OnOldContentSettingsPatternPairsChanged,
      base::Unretained(this)));

  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    content_settings_prefs_.push_back(new ContentSettingsPref(
        ContentSettingsType(i), prefs_, &pref_change_registrar_,
        kContentSettingsExceptionsPrefs[i], is_incognito_,
        &updating_old_preferences_, base::Bind(&PrefProvider::Notify,
                                               base::Unretained(this))));
  }

  // Migrate all the exceptions from the aggregate dictionary preference
  // to the separate dictionaries, if this hasn't been done before.
  if (!prefs_->GetBoolean(prefs::kMigratedContentSettingsPatternPairs)) {
    WriteSettingsToNewPreferences(false);
    prefs_->SetBoolean(prefs::kMigratedContentSettingsPatternPairs, true);
  } else {
    // Trigger the update of old preference, and as a result,
    // the new preferences as well.
    OnOldContentSettingsPatternPairsChanged();
  }

  if (!is_incognito_) {
    size_t num_exceptions = 0;
    for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      num_exceptions += content_settings_prefs_[i]->GetNumExceptions();

    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfExceptions",
                         num_exceptions);

    // Migrate the obsolete media content setting exceptions to the new
    // settings.
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
  return content_settings_prefs_[content_type]->GetRuleIterator(
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

  return content_settings_prefs_[content_type]->SetWebsiteSetting(
      primary_pattern,
      secondary_pattern,
      resource_identifier,
      in_value);
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  content_settings_prefs_[content_type]->ClearAllContentSettingsRules();
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
  content_settings_prefs_[content_type]->UpdateLastUsage(primary_pattern,
                                                         secondary_pattern,
                                                         clock_.get());
}

base::Time PrefProvider::GetLastUsage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  return content_settings_prefs_[content_type]->GetLastUsage(primary_pattern,
                                                             secondary_pattern);
}

// ////////////////////////////////////////////////////////////////////////////
// Private

PrefProvider::ContentSettingsPrefEntry::ContentSettingsPrefEntry(
    const ContentSettingsPattern primary_pattern,
    const ContentSettingsPattern secondary_pattern,
    const ResourceIdentifier resource_identifier,
    base::Value* value)
    : primary_pattern(CopyPattern(primary_pattern)),
      secondary_pattern(CopyPattern(secondary_pattern)),
      resource_identifier(resource_identifier),
      value(value) {
}

PrefProvider::ContentSettingsPrefEntry::ContentSettingsPrefEntry(
    const ContentSettingsPrefEntry& entry)
    : primary_pattern(CopyPattern(entry.primary_pattern)),
      secondary_pattern(CopyPattern(entry.secondary_pattern)),
      resource_identifier(entry.resource_identifier),
      value(entry.value->DeepCopy()) {
}

PrefProvider::ContentSettingsPrefEntry&
    PrefProvider::ContentSettingsPrefEntry::operator=(
        const ContentSettingsPrefEntry& entry) {
  this->primary_pattern = CopyPattern(entry.primary_pattern);
  this->secondary_pattern = CopyPattern(entry.secondary_pattern);
  this->resource_identifier = entry.resource_identifier;
  this->value.reset(entry.value->DeepCopy());

  return *this;
}

PrefProvider::ContentSettingsPrefEntry::~ContentSettingsPrefEntry() {}

void PrefProvider::MigrateObsoleteMediaContentSetting() {
  std::vector<Rule> rules_to_delete;
  {
    scoped_ptr<RuleIterator> rule_iterator(GetRuleIterator(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM, ResourceIdentifier(), false));
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
                        ResourceIdentifier(),
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
    }
    // Add the exception to the new camera content setting.
    if (!video_device.empty()) {
      SetWebsiteSetting(it->primary_pattern,
                        it->secondary_pattern,
                        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                        ResourceIdentifier(),
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
    }

    // Remove the old exception in CONTENT_SETTINGS_TYPE_MEDIASTREAM.
    SetWebsiteSetting(it->primary_pattern,
                      it->secondary_pattern,
                      CONTENT_SETTINGS_TYPE_MEDIASTREAM,
                      ResourceIdentifier(),
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

void PrefProvider::ReadContentSettingsFromOldPref() {
  // |DictionaryPrefUpdate| sends out notifications when destructed. This
  // construction order ensures |AutoLock| gets destroyed first and |old_lock_|
  // is not held when the notifications are sent. Also, |auto_reset| must be
  // still valid when the notifications are sent, so that |Observe| skips the
  // notification.
  base::AutoReset<bool> auto_reset(&updating_old_preferences_, true);
  DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);

  ClearPrefEntryMap();

  const base::DictionaryValue* all_settings_dictionary =
      prefs_->GetDictionary(prefs::kContentSettingsPatternPairs);

  // Careful: The returned value could be NULL if the pref has never been set.
  if (!all_settings_dictionary)
    return;

  base::DictionaryValue* mutable_settings;
  scoped_ptr<base::DictionaryValue> mutable_settings_scope;

  if (!is_incognito_) {
    mutable_settings = update.Get();
  } else {
    // Create copy as we do not want to persist anything in OTR prefs.
    mutable_settings = all_settings_dictionary->DeepCopy();
    mutable_settings_scope.reset(mutable_settings);
  }
  // Convert all Unicode patterns into punycode form, then read.
  ContentSettingsPref::CanonicalizeContentSettingsExceptions(mutable_settings);

  for (base::DictionaryValue::Iterator i(*mutable_settings); !i.IsAtEnd();
       i.Advance()) {
    const std::string& pattern_str(i.key());
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
        ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      // TODO: Change this to DFATAL when crbug.com/132659 is fixed.
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    // Get settings dictionary for the current pattern string, and read
    // settings from the dictionary.
    const base::DictionaryValue* settings_dictionary = NULL;
    bool is_dictionary = i.value().GetAsDictionary(&settings_dictionary);
    DCHECK(is_dictionary);

    for (size_t k = 0; k < CONTENT_SETTINGS_NUM_TYPES; ++k) {
      ContentSettingsType content_type = static_cast<ContentSettingsType>(k);

      std::string res_dictionary_path;
      if (GetResourceTypeName(content_type, &res_dictionary_path)) {
        const base::DictionaryValue* resource_dictionary = NULL;
        if (settings_dictionary->GetDictionary(
                res_dictionary_path, &resource_dictionary)) {
          for (base::DictionaryValue::Iterator j(*resource_dictionary);
               !j.IsAtEnd();
               j.Advance()) {
            const std::string& resource_identifier(j.key());
            int setting = CONTENT_SETTING_DEFAULT;
            bool is_integer = j.value().GetAsInteger(&setting);
            DCHECK(is_integer);
            DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);

            pref_entry_map_[content_type].push_back(
                new ContentSettingsPrefEntry(
                    pattern_pair.first,
                    pattern_pair.second,
                    resource_identifier,
                    new base::FundamentalValue(setting)));
          }
        }
      }
      base::Value* value = NULL;
      if (HostContentSettingsMap::ContentTypeHasCompoundValue(content_type)) {
        const base::DictionaryValue* setting = NULL;
        // TODO(xians): Handle the non-dictionary types.
        if (settings_dictionary->GetDictionaryWithoutPathExpansion(
            GetTypeName(content_type), &setting)) {
          DCHECK(!setting->empty());
          value = setting->DeepCopy();
        }
      } else {
        int setting = CONTENT_SETTING_DEFAULT;
        if (settings_dictionary->GetIntegerWithoutPathExpansion(
                GetTypeName(content_type), &setting)) {
          DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
          setting = FixObsoleteCookiePromptMode(content_type,
                                                ContentSetting(setting));
          value = new base::FundamentalValue(setting);
        }
      }

      // |pref_entry_map_| will take the ownership of |value|.
      if (value != NULL) {
        pref_entry_map_[content_type].push_back(
            new ContentSettingsPrefEntry(
                pattern_pair.first,
                pattern_pair.second,
                ResourceIdentifier(),
                value));
      }
    }
  }
}

void PrefProvider::WriteSettingsToNewPreferences(bool syncable_only) {
  // The incognito provider cannot write the settings to avoid echo effect:
  // New preference -> PrefProvider -> Old preference ->
  // -> Incognito PrefProvider -> New preference -> etc.
  if (is_incognito_)
    return;

  if (updating_old_preferences_)
    return;

  base::AutoReset<bool> auto_reset(&updating_old_preferences_, true);
  base::AutoLock auto_lock(old_lock_);

  ReadContentSettingsFromOldPref();

  for (int k = 0; k < CONTENT_SETTINGS_NUM_TYPES; ++k) {
    ContentSettingsType content_type = ContentSettingsType(k);

    if (syncable_only && !IsContentSettingsTypeSyncable(content_type))
      continue;

    content_settings_prefs_[content_type]->ClearAllContentSettingsRules();

    for (size_t i = 0; i < pref_entry_map_[content_type].size(); ++i) {
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
      // Protected Media Identifier "Allow" exceptions can not be migrated.
      const base::FundamentalValue allow_value(CONTENT_SETTING_ALLOW);
      if (content_type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER &&
          pref_entry_map_[content_type][i]->value->Equals(&allow_value)) {
        continue;
      }
#endif

      content_settings_prefs_[content_type]->SetWebsiteSetting(
          pref_entry_map_[content_type][i]->primary_pattern,
          pref_entry_map_[content_type][i]->secondary_pattern,
          pref_entry_map_[content_type][i]->resource_identifier,
          pref_entry_map_[content_type][i]->value.release());
    }
  }

  ClearPrefEntryMap();
}

void PrefProvider::ClearPrefEntryMap() {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    pref_entry_map_[i].clear();
}

void PrefProvider::OnOldContentSettingsPatternPairsChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  WriteSettingsToNewPreferences(true);
}

void PrefProvider::SetClockForTesting(scoped_ptr<base::Clock> clock) {
  clock_ = clock.Pass();
}

bool PrefProvider::TestAllLocks() const {
  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (!content_settings_prefs_[i]->lock_.Try())
      return false;
    content_settings_prefs_[i]->lock_.Release();
  }
  return true;
}

}  // namespace content_settings
