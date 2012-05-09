// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pref_provider.h"

#include <map>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;
using content::UserMetricsAction;

namespace {

typedef std::pair<std::string, std::string> StringPair;
typedef std::map<std::string, std::string> StringMap;

const char kPerPluginPrefName[] = "per_plugin";

ContentSetting FixObsoleteCookiePromptMode(ContentSettingsType content_type,
                                           ContentSetting setting) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES &&
      setting == CONTENT_SETTING_ASK) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

// If the given content type supports resource identifiers in user preferences,
// returns true and sets |pref_key| to the key in the content settings
// dictionary under which per-resource content settings are stored.
// Otherwise, returns false.
bool GetResourceTypeName(ContentSettingsType content_type,
                         std::string* pref_key) {
  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    *pref_key = kPerPluginPrefName;
    return true;
  }
  return false;
}

}  // namespace

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// PrefProvider:
//

// static
void PrefProvider::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(
      prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion,
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kContentSettingsPatternPairs,
                                PrefService::SYNCABLE_PREF);

  // Obsolete prefs, for migration:
  prefs->RegisterDictionaryPref(prefs::kGeolocationContentSettings,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kContentSettingsPatterns,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kDesktopNotificationAllowedOrigins,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kDesktopNotificationDeniedOrigins,
                          PrefService::UNSYNCABLE_PREF);
}

PrefProvider::PrefProvider(PrefService* prefs,
                           bool incognito)
  : prefs_(prefs),
    is_incognito_(incognito),
    updating_preferences_(false) {
  DCHECK(prefs_);
  if (!is_incognito_) {
    // Migrate obsolete preferences.
    MigrateObsoleteContentSettingsPatternPref();
    MigrateObsoleteGeolocationPref();
    MigrateObsoleteNotificationsPrefs();
  }

  // Verify preferences version.
  if (!prefs_->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs_->SetInteger(prefs::kContentSettingsVersion,
                      ContentSettingsPattern::kContentSettingsPatternVersion);
  }
  if (prefs_->GetInteger(prefs::kContentSettingsVersion) >
      ContentSettingsPattern::kContentSettingsPatternVersion) {
    return;
  }

  // Read content settings exceptions.
  ReadContentSettingsFromPref(false);

  if (!is_incognito_) {
    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfExceptions",
                         value_map_.size());
  }

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(prefs::kContentSettingsPatternPairs, this);
}

bool PrefProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Value* in_value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

  // At this point take the ownership of the |in_value|.
  scoped_ptr<base::Value> value(in_value);
  // Update in memory value map.
  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(lock_);
    if (value.get()) {
      map_to_modify->SetValue(
          primary_pattern,
          secondary_pattern,
          content_type,
          resource_identifier,
          value->DeepCopy());
    } else {
      map_to_modify->DeleteValue(
          primary_pattern,
          secondary_pattern,
          content_type,
          resource_identifier);
    }
  }
  // Update the content settings preference.
  if (!is_incognito_) {
    UpdatePref(primary_pattern,
               secondary_pattern,
               content_type,
               resource_identifier,
               value.get());
  }

  NotifyObservers(
      primary_pattern, secondary_pattern, content_type, resource_identifier);

  return true;
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);

  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  std::vector<Rule> rules_to_delete;
  {
    base::AutoLock auto_lock(lock_);
    scoped_ptr<RuleIterator> rule_iterator(
        map_to_modify->GetRuleIterator(content_type, "", NULL));
    // Copy the rules; we cannot call |UpdatePref| while holding |lock_|.
    while (rule_iterator->HasNext())
      rules_to_delete.push_back(rule_iterator->Next());

    map_to_modify->DeleteValues(content_type, "");
  }

  for (std::vector<Rule>::const_iterator it = rules_to_delete.begin();
       it != rules_to_delete.end(); ++it) {
    UpdatePref(
        it->primary_pattern,
        it->secondary_pattern,
        content_type,
        "",
        NULL);
  }
  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  std::string());
}

void PrefProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    DCHECK_EQ(prefs_, content::Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = content::Details<std::string>(details).ptr();
    if (*name != prefs::kContentSettingsPatternPairs) {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }
    ReadContentSettingsFromPref(true);

    NotifyObservers(ContentSettingsPattern(),
                    ContentSettingsPattern(),
                    CONTENT_SETTINGS_TYPE_DEFAULT,
                    std::string());
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

PrefProvider::~PrefProvider() {
  DCHECK(!prefs_);
}

RuleIterator* PrefProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  if (incognito)
    return incognito_value_map_.GetRuleIterator(content_type,
                                                resource_identifier,
                                                &lock_);
  return value_map_.GetRuleIterator(content_type, resource_identifier, &lock_);
}

// ////////////////////////////////////////////////////////////////////////////
// Private

void PrefProvider::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    const base::Value* value) {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~DictionaryPrefUpdate|).
  AssertLockNotHeld();

  AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    DictionaryPrefUpdate update(prefs_,
                                prefs::kContentSettingsPatternPairs);
    DictionaryValue* pattern_pairs_settings = update.Get();
    UpdatePatternPairsSettings(primary_pattern,
                               secondary_pattern,
                               content_type,
                               resource_identifier,
                               value,
                               pattern_pairs_settings);
  }
}

void PrefProvider::ReadContentSettingsFromPref(bool overwrite) {
  // |DictionaryPrefUpdate| sends out notifications when destructed. This
  // construction order ensures |AutoLock| gets destroyed first and |lock_| is
  // not held when the notifications are sent. Also, |auto_reset| must be still
  // valid when the notifications are sent, so that |Observe| skips the
  // notification.
  AutoReset<bool> auto_reset(&updating_preferences_, true);
  DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);
  base::AutoLock auto_lock(lock_);

  const DictionaryValue* all_settings_dictionary =
      prefs_->GetDictionary(prefs::kContentSettingsPatternPairs);

  if (overwrite)
    value_map_.clear();

  // Careful: The returned value could be NULL if the pref has never been set.
  if (!all_settings_dictionary)
    return;

  DictionaryValue* mutable_settings;
  scoped_ptr<DictionaryValue> mutable_settings_scope;

  if (!is_incognito_) {
    mutable_settings = update.Get();
  } else {
    // Create copy as we do not want to persist anything in OTR prefs.
    mutable_settings = all_settings_dictionary->DeepCopy();
    mutable_settings_scope.reset(mutable_settings);
  }
  // Convert all Unicode patterns into punycode form, then read.
  CanonicalizeContentSettingsExceptions(mutable_settings);

  for (DictionaryValue::key_iterator i(mutable_settings->begin_keys());
       i != mutable_settings->end_keys(); ++i) {
    const std::string& pattern_str(*i);
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
        ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      LOG(DFATAL) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    // Get settings dictionary for the current pattern string, and read
    // settings from the dictionary.
    DictionaryValue* settings_dictionary = NULL;
    bool found = mutable_settings->GetDictionaryWithoutPathExpansion(
        pattern_str, &settings_dictionary);
    DCHECK(found);

    for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
      ContentSettingsType content_type = static_cast<ContentSettingsType>(i);

      std::string res_dictionary_path;
      if (GetResourceTypeName(content_type, &res_dictionary_path)) {
        DictionaryValue* resource_dictionary = NULL;
        if (settings_dictionary->GetDictionary(
                res_dictionary_path, &resource_dictionary)) {
          for (DictionaryValue::key_iterator j(
                   resource_dictionary->begin_keys());
               j != resource_dictionary->end_keys();
               ++j) {
            const std::string& resource_identifier(*j);
            int setting = CONTENT_SETTING_DEFAULT;
            found = resource_dictionary->GetIntegerWithoutPathExpansion(
                resource_identifier, &setting);
            DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
            value_map_.SetValue(pattern_pair.first,
                                pattern_pair.second,
                                content_type,
                                resource_identifier,
                                Value::CreateIntegerValue(setting));
          }
        }
      }
      int setting = CONTENT_SETTING_DEFAULT;
      if (settings_dictionary->GetIntegerWithoutPathExpansion(
              GetTypeName(ContentSettingsType(i)), &setting)) {
        DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
        setting = FixObsoleteCookiePromptMode(content_type,
                                              ContentSetting(setting));
        value_map_.SetValue(pattern_pair.first,
                            pattern_pair.second,
                            content_type,
                            ResourceIdentifier(""),
                            Value::CreateIntegerValue(setting));
      }
    }
  }
}

void PrefProvider::UpdatePatternPairsSettings(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      const base::Value* value,
      DictionaryValue* pattern_pairs_settings) {
  // Get settings dictionary for the given patterns.
  std::string pattern_str(CreatePatternString(primary_pattern,
                                              secondary_pattern));
  DictionaryValue* settings_dictionary = NULL;
  bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
      pattern_str, &settings_dictionary);

  if (!found && value) {
    settings_dictionary = new DictionaryValue;
    pattern_pairs_settings->SetWithoutPathExpansion(
        pattern_str, settings_dictionary);
  }

  if (settings_dictionary) {
    std::string res_dictionary_path;
    if (GetResourceTypeName(content_type, &res_dictionary_path) &&
        !resource_identifier.empty()) {
      DictionaryValue* resource_dictionary = NULL;
      found = settings_dictionary->GetDictionary(
          res_dictionary_path, &resource_dictionary);
      if (!found) {
        if (value == NULL)
          return;  // Nothing to remove. Exit early.
        resource_dictionary = new DictionaryValue;
        settings_dictionary->Set(res_dictionary_path, resource_dictionary);
      }
      // Update resource dictionary.
      if (value == NULL) {
        resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                        NULL);
        if (resource_dictionary->empty()) {
          settings_dictionary->RemoveWithoutPathExpansion(
              res_dictionary_path, NULL);
        }
      } else {
        resource_dictionary->SetWithoutPathExpansion(
            resource_identifier, value->DeepCopy());
      }
    } else {
      // Update settings dictionary.
      std::string setting_path = GetTypeName(content_type);
      if (value == NULL) {
        settings_dictionary->RemoveWithoutPathExpansion(setting_path,
                                                        NULL);
      } else {
        settings_dictionary->SetWithoutPathExpansion(
            setting_path, value->DeepCopy());
      }
    }
    // Remove the settings dictionary if it is empty.
    if (settings_dictionary->empty()) {
      pattern_pairs_settings->RemoveWithoutPathExpansion(
          pattern_str, NULL);
    }
  }
}

// static
void PrefProvider::CanonicalizeContentSettingsExceptions(
    DictionaryValue* all_settings_dictionary) {
  DCHECK(all_settings_dictionary);

  std::vector<std::string> remove_items;
  std::vector<std::pair<std::string, std::string> > move_items;
  for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
       i != all_settings_dictionary->end_keys(); ++i) {
    const std::string& pattern_str(*i);
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
         ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    const std::string canonicalized_pattern_str = CreatePatternString(
        pattern_pair.first, pattern_pair.second);

    if (canonicalized_pattern_str.empty() ||
        canonicalized_pattern_str == pattern_str)
      continue;

    // Clear old pattern if prefs already have canonicalized pattern.
    DictionaryValue* new_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            canonicalized_pattern_str, &new_pattern_settings_dictionary)) {
      remove_items.push_back(pattern_str);
      continue;
    }

    // Move old pattern to canonicalized pattern.
    DictionaryValue* old_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            pattern_str, &old_pattern_settings_dictionary)) {
      move_items.push_back(
          std::make_pair(pattern_str, canonicalized_pattern_str));
    }
  }

  for (size_t i = 0; i < remove_items.size(); ++i) {
    all_settings_dictionary->RemoveWithoutPathExpansion(remove_items[i], NULL);
  }

  for (size_t i = 0; i < move_items.size(); ++i) {
    Value* pattern_settings_dictionary = NULL;
    all_settings_dictionary->RemoveWithoutPathExpansion(
        move_items[i].first, &pattern_settings_dictionary);
    all_settings_dictionary->SetWithoutPathExpansion(
        move_items[i].second, pattern_settings_dictionary);
  }
}

void PrefProvider::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void PrefProvider::MigrateObsoleteContentSettingsPatternPref() {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~DictionaryPrefUpdate|).
  AssertLockNotHeld();

  if (!prefs_->HasPrefPath(prefs::kContentSettingsPatterns))
    return;

  const DictionaryValue* patterns_dictionary =
      prefs_->GetDictionary(prefs::kContentSettingsPatterns);
  {
    DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);
    DictionaryValue* pattern_pairs_dictionary = update.Get();
    for (DictionaryValue::key_iterator i(
             patterns_dictionary->begin_keys());
         i != patterns_dictionary->end_keys();
         ++i) {
      const std::string& key(*i);
      // In the past a bug once corrupted dictionary keys. Test if the |key| is
      // corrupted and skip a corrupted key. A dictionary |key| can contain two
      // content settings patterns, a primary pattern and a secondary pattern.
      // If the |key| contains two patterns than they are concataneted with a
      // ','.
      size_t sep_pos = key.find(",");
      ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(key.substr(0, sep_pos));
      // Skip the current |key| if the primary |pattern| is invalid.
      if (!pattern.IsValid())
        continue;
      // If the |key| contains a secondary pattern, and the obsolete pref
      // dictionary also contains a key that equals the primary |pattern| then
      // skip the current |key|.
      if (sep_pos != std::string::npos &&
          patterns_dictionary->HasKey(pattern.ToString())) {
        continue;
      }

      // Copy the legacy content settings for the current |key| from the
      // obsolete pref prefs::kContentSettingsPatterns to the pref
      // prefs::kContentSettingsPatternPairs.
      DictionaryValue* dictionary = NULL;
      bool found = patterns_dictionary->GetDictionaryWithoutPathExpansion(
          key, &dictionary);
      DCHECK(found);
      std::string new_key = CreatePatternString(
          pattern, ContentSettingsPattern::Wildcard());
      // Existing values are overwritten.
      pattern_pairs_dictionary->SetWithoutPathExpansion(
          new_key, dictionary->DeepCopy());
    }
  }
  prefs_->ClearPref(prefs::kContentSettingsPatterns);
}

void PrefProvider::MigrateObsoleteGeolocationPref() {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~DictionaryPrefUpdate|).
  AssertLockNotHeld();

  if (!prefs_->HasPrefPath(prefs::kGeolocationContentSettings))
    return;

  DictionaryPrefUpdate update(prefs_,
                              prefs::kContentSettingsPatternPairs);
  DictionaryValue* pattern_pairs_settings = update.Get();

  const DictionaryValue* geolocation_settings =
      prefs_->GetDictionary(prefs::kGeolocationContentSettings);

  std::vector<std::pair<std::string, std::string> > corrupted_keys;
  for (DictionaryValue::key_iterator i =
           geolocation_settings->begin_keys();
       i != geolocation_settings->end_keys();
       ++i) {
    const std::string& primary_key(*i);
    GURL primary_url(primary_key);
    DCHECK(primary_url.is_valid());

    DictionaryValue* requesting_origin_settings = NULL;
    // The method GetDictionaryWithoutPathExpansion() returns false if the
    // value for the given key is not a |DictionaryValue|. If the value for the
    // |primary_key| is not a |DictionaryValue| then the location settings for
    // this key are corrupted. Therefore they are ignored.
    if (!geolocation_settings->GetDictionaryWithoutPathExpansion(
        primary_key, &requesting_origin_settings))
      continue;

    for (DictionaryValue::key_iterator j =
             requesting_origin_settings->begin_keys();
         j != requesting_origin_settings->end_keys();
         ++j) {
      const std::string& secondary_key(*j);
      GURL secondary_url(secondary_key);
      // Save corrupted keys to remove them later.
      if (!secondary_url.is_valid()) {
        corrupted_keys.push_back(std::make_pair(primary_key, secondary_key));
        continue;
      }

      base::Value* value = NULL;
      bool found = requesting_origin_settings->GetWithoutPathExpansion(
          secondary_key, &value);
      DCHECK(found);

      ContentSettingsPattern primary_pattern =
          ContentSettingsPattern::FromURLNoWildcard(primary_url);
      ContentSettingsPattern secondary_pattern =
          ContentSettingsPattern::FromURLNoWildcard(secondary_url);
      DCHECK(primary_pattern.IsValid() && secondary_pattern.IsValid());

      UpdatePatternPairsSettings(primary_pattern,
                                 secondary_pattern,
                                 CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                 std::string(),
                                 value,
                                 pattern_pairs_settings);
    }
  }

  prefs_->ClearPref(prefs::kGeolocationContentSettings);
}

void PrefProvider::MigrateObsoleteNotificationsPrefs() {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~DictionaryPrefUpdate|).
  AssertLockNotHeld();

  if (!prefs_->HasPrefPath(prefs::kDesktopNotificationAllowedOrigins) &&
      !prefs_->HasPrefPath(prefs::kDesktopNotificationDeniedOrigins)) {
    return;
  }

  DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);
  DictionaryValue* pattern_pairs_settings = update.Get();

  const ListValue* allowed_origins =
      prefs_->GetList(prefs::kDesktopNotificationAllowedOrigins);
  for (size_t i = 0; i < allowed_origins->GetSize(); ++i) {
    std::string url_string;
    bool status = allowed_origins->GetString(i, &url_string);
    DCHECK(status);
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url_string));
    DCHECK(primary_pattern.IsValid());
    scoped_ptr<base::Value> value(
        Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
    UpdatePatternPairsSettings(primary_pattern,
                               ContentSettingsPattern::Wildcard(),
                               CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                               std::string(),
                               value.get(),
                               pattern_pairs_settings);
  }

  const ListValue* denied_origins =
      prefs_->GetList(prefs::kDesktopNotificationDeniedOrigins);
  for (size_t i = 0; i < denied_origins->GetSize(); ++i) {
    std::string url_string;
    bool status = denied_origins->GetString(i, &url_string);
    DCHECK(status);
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url_string));
    DCHECK(primary_pattern.IsValid());
    scoped_ptr<base::Value> value(
        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
    UpdatePatternPairsSettings(primary_pattern,
                               ContentSettingsPattern::Wildcard(),
                               CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                               std::string(),
                               value.get(),
                               pattern_pairs_settings);
  }

  prefs_->ClearPref(prefs::kDesktopNotificationAllowedOrigins);
  prefs_->ClearPref(prefs::kDesktopNotificationDeniedOrigins);
}

void PrefProvider::AssertLockNotHeld() const {
#if !defined(NDEBUG)
  // |Lock::Acquire()| will assert if the lock is held by this thread.
  lock_.Acquire();
  lock_.Release();
#endif
}

}  // namespace content_settings
