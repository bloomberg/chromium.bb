// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pref_provider.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

typedef std::pair<std::string, std::string> StringPair;
typedef std::map<std::string, std::string> StringMap;

// The preference keys where resource identifiers are stored for
// ContentSettingsType values that support resource identifiers.
const char* kResourceTypeNames[] = {
  NULL,
  NULL,
  NULL,
  "per_plugin",
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};
COMPILE_ASSERT(arraysize(kResourceTypeNames) == CONTENT_SETTINGS_NUM_TYPES,
               resource_type_names_incorrect_size);

// The default setting for each content type.
const ContentSetting kDefaultSettings[] = {
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_COOKIES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_IMAGES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_PLUGINS
  CONTENT_SETTING_BLOCK,  // CONTENT_SETTINGS_TYPE_POPUPS
  CONTENT_SETTING_ASK,    // CONTENT_SETTINGS_TYPE_GEOLOCATION
  CONTENT_SETTING_ASK,    // CONTENT_SETTINGS_TYPE_NOTIFICATIONS
  CONTENT_SETTING_ASK,    // CONTENT_SETTINGS_TYPE_INTENTS
  CONTENT_SETTING_ASK,    // CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE
};
COMPILE_ASSERT(arraysize(kDefaultSettings) == CONTENT_SETTINGS_NUM_TYPES,
               default_settings_incorrect_size);

// The names of the ContentSettingsType values, for use with dictionary prefs.
const char* kTypeNames[] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  "geolocation",
  "notifications",
  "intents",
  "auto-select-certificate"
};
COMPILE_ASSERT(arraysize(kTypeNames) == CONTENT_SETTINGS_NUM_TYPES,
               type_names_incorrect_size);

void SetDefaultContentSettings(DictionaryValue* default_settings) {
  default_settings->Clear();

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (kTypeNames[i] != NULL) {
      default_settings->SetInteger(kTypeNames[i],
                                   kDefaultSettings[i]);
    }
  }
}

ContentSetting ValueToContentSetting(Value* value) {
  int int_value;
  value->GetAsInteger(&int_value);
  return IntToContentSetting(int_value);
}

ContentSettingsType StringToContentSettingsType(
    const std::string& content_type_str) {
  for (size_t type = 0; type < arraysize(kTypeNames); ++type) {
    if ((kTypeNames[type] != NULL) && (kTypeNames[type] == content_type_str))
      return ContentSettingsType(type);
  }
  for (size_t type = 0; type < arraysize(kResourceTypeNames); ++type) {
    if ((kResourceTypeNames[type] != NULL) &&
        (kResourceTypeNames[type] == content_type_str)) {
      return ContentSettingsType(type);
    }
  }
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

ContentSetting FixObsoleteCookiePromptMode(ContentSettingsType content_type,
                                           ContentSetting setting) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES &&
      setting == CONTENT_SETTING_ASK) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

// Clears all settings for the given |type| in the given |pattern_pairs|
// dictionary.
void ClearSettings(ContentSettingsType type,
                   DictionaryValue* pattern_pairs) {
  std::string type_name(kTypeNames[type]);
  for (DictionaryValue::key_iterator i = pattern_pairs->begin_keys();
       i != pattern_pairs->end_keys();
       ++i) {
    const std::string& pattern_pair(*i);

    DictionaryValue* settings = NULL;
    pattern_pairs->GetDictionaryWithoutPathExpansion(
        pattern_pair, &settings);

    settings->RemoveWithoutPathExpansion(type_name, NULL);
  }
}

}  // namespace

namespace content_settings {

PrefDefaultProvider::PrefDefaultProvider(PrefService* prefs,
                                         bool incognito)
    : prefs_(prefs),
      is_incognito_(incognito),
      updating_preferences_(false) {
  DCHECK(prefs_);
  MigrateObsoleteNotificationPref();
  MigrateObsoleteGeolocationPref();

  // Read global defaults.
  DCHECK_EQ(arraysize(kTypeNames),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  ReadDefaultSettings(true);
  if (default_content_settings_.settings[CONTENT_SETTINGS_TYPE_COOKIES] ==
      CONTENT_SETTING_BLOCK) {
    UserMetrics::RecordAction(
        UserMetricsAction("CookieBlockingEnabledPerDefault"));
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("CookieBlockingDisabledPerDefault"));
  }

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(prefs::kDefaultContentSettings, this);
  pref_change_registrar_.Add(prefs::kGeolocationDefaultContentSetting, this);
}

PrefDefaultProvider::~PrefDefaultProvider() {
  DCHECK(!prefs_);
}

ContentSetting PrefDefaultProvider::ProvideDefaultSetting(
    ContentSettingsType content_type) const {
  base::AutoLock lock(lock_);
  return default_content_settings_.settings[content_type];
}

void PrefDefaultProvider::UpdateDefaultSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  DCHECK(kTypeNames[content_type] != NULL);

  // The default settings may not be directly modified for OTR sessions.
  // Instead, they are synced to the main profile's setting.
  if (is_incognito_)
    return;

  std::string dictionary_path(kTypeNames[content_type]);
  {
    AutoReset<bool> auto_reset(&updating_preferences_, true);
    base::AutoLock lock(lock_);
    DictionaryPrefUpdate update(prefs_, prefs::kDefaultContentSettings);
    DictionaryValue* default_settings_dictionary = update.Get();
    if ((setting == CONTENT_SETTING_DEFAULT) ||
        (setting == kDefaultSettings[content_type])) {
      default_content_settings_.settings[content_type] =
          kDefaultSettings[content_type];
      default_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path,
                                                              NULL);
    } else {
      default_content_settings_.settings[content_type] = setting;
      default_settings_dictionary->SetWithoutPathExpansion(
          dictionary_path, Value::CreateIntegerValue(setting));
    }

    // Keep the obsolete pref in sync as long as backwards compatibility is
    // required. This is required to keep sync working correctly.
    if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
       prefs_->SetInteger(prefs::kGeolocationDefaultContentSetting,
                          setting == CONTENT_SETTING_DEFAULT ?
                              kDefaultSettings[content_type] : setting);
    }
  }

  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  std::string());
}

bool PrefDefaultProvider::DefaultSettingIsManaged(
    ContentSettingsType content_type) const {
  return false;
}

void PrefDefaultProvider::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    DCHECK_EQ(prefs_, Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kDefaultContentSettings) {
      ReadDefaultSettings(true);
    } else if (*name == prefs::kGeolocationDefaultContentSetting) {
      MigrateObsoleteGeolocationPref();
      // Return and don't send a notifications. Migrating the obsolete
      // geolocation pref will change the prefs::kDefaultContentSettings and
      // cause the notification to be fired.
      return;
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    NotifyObservers(ContentSettingsPattern(),
                    ContentSettingsPattern(),
                    CONTENT_SETTINGS_TYPE_DEFAULT,
                    std::string());
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

void PrefDefaultProvider::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void PrefDefaultProvider::ReadDefaultSettings(bool overwrite) {
  const DictionaryValue* default_settings_dictionary =
      prefs_->GetDictionary(prefs::kDefaultContentSettings);

  base::AutoLock lock(lock_);

  if (overwrite)
    default_content_settings_ = ContentSettings();

  // Careful: The returned value could be NULL if the pref has never been set.
  if (default_settings_dictionary != NULL) {
    GetSettingsFromDictionary(default_settings_dictionary,
                              &default_content_settings_);
  }
  ForceDefaultsToBeExplicit();
}

void PrefDefaultProvider::ForceDefaultsToBeExplicit() {
  DCHECK_EQ(arraysize(kDefaultSettings),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (default_content_settings_.settings[i] == CONTENT_SETTING_DEFAULT)
      default_content_settings_.settings[i] = kDefaultSettings[i];
  }
}

void PrefDefaultProvider::GetSettingsFromDictionary(
    const DictionaryValue* dictionary,
    ContentSettings* settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::string& content_type(*i);
    for (size_t type = 0; type < arraysize(kTypeNames); ++type) {
      if ((kTypeNames[type] != NULL) && (kTypeNames[type] == content_type)) {
        int setting = CONTENT_SETTING_DEFAULT;
        bool found = dictionary->GetIntegerWithoutPathExpansion(content_type,
                                                                &setting);
        DCHECK(found);
        settings->settings[type] = IntToContentSetting(setting);
        break;
      }
    }
  }
  // Migrate obsolete cookie prompt mode.
  if (settings->settings[CONTENT_SETTINGS_TYPE_COOKIES] ==
      CONTENT_SETTING_ASK)
    settings->settings[CONTENT_SETTINGS_TYPE_COOKIES] = CONTENT_SETTING_BLOCK;

  settings->settings[CONTENT_SETTINGS_TYPE_PLUGINS] =
      ClickToPlayFixup(CONTENT_SETTINGS_TYPE_PLUGINS,
                       settings->settings[CONTENT_SETTINGS_TYPE_PLUGINS]);
}

void PrefDefaultProvider::MigrateObsoleteNotificationPref() {
  if (prefs_->HasPrefPath(prefs::kDesktopNotificationDefaultContentSetting)) {
    ContentSetting setting = IntToContentSetting(
        prefs_->GetInteger(prefs::kDesktopNotificationDefaultContentSetting));
    UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, setting);
    prefs_->ClearPref(prefs::kDesktopNotificationDefaultContentSetting);
  }
}

void PrefDefaultProvider::MigrateObsoleteGeolocationPref() {
  if (prefs_->HasPrefPath(prefs::kGeolocationDefaultContentSetting)) {
    ContentSetting setting = IntToContentSetting(
        prefs_->GetInteger(prefs::kGeolocationDefaultContentSetting));
    UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION, setting);
    // Do not clear the old preference yet as long as we need to maintain
    // backward compatibility.
  }
}

// static
void PrefDefaultProvider::RegisterUserPrefs(PrefService* prefs) {
  // The registration of the preference prefs::kDefaultContentSettings should
  // also include the default values for default content settings. This allows
  // functional tests to get default content settings by reading the preference
  // prefs::kDefaultContentSettings via pyauto.
  // TODO(markusheintz): Write pyauto hooks for the content settings map as
  // content settings should be read from the host content settings map.
  DictionaryValue* default_content_settings = new DictionaryValue();
  SetDefaultContentSettings(default_content_settings);
  prefs->RegisterDictionaryPref(prefs::kDefaultContentSettings,
                                default_content_settings,
                                PrefService::SYNCABLE_PREF);

  // Obsolete prefs, for migrations:
  prefs->RegisterIntegerPref(
      prefs::kDesktopNotificationDefaultContentSetting,
      kDefaultSettings[CONTENT_SETTINGS_TYPE_NOTIFICATIONS],
      PrefService::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(
      prefs::kGeolocationDefaultContentSetting,
      kDefaultSettings[CONTENT_SETTINGS_TYPE_GEOLOCATION],
      PrefService::UNSYNCABLE_PREF);
}

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
                                PrefService::SYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kContentSettingsPatterns,
                                PrefService::SYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kDesktopNotificationAllowedOrigins,
                          PrefService::SYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kDesktopNotificationDeniedOrigins,
                          PrefService::SYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPopupWhitelistedHosts,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kPerHostContentSettings,
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
    MigrateObsoletePerhostPref();
    MigrateObsoletePopupsPref();
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
  pref_change_registrar_.Add(prefs::kContentSettingsPatterns, this);
  pref_change_registrar_.Add(prefs::kContentSettingsPatternPairs, this);
  pref_change_registrar_.Add(prefs::kGeolocationContentSettings, this);
  pref_change_registrar_.Add(prefs::kDesktopNotificationAllowedOrigins, this);
  pref_change_registrar_.Add(prefs::kDesktopNotificationDeniedOrigins, this);
}

ContentSetting PrefProvider::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  scoped_ptr<Value> value(GetContentSettingValue(primary_url,
                                                 secondary_url,
                                                 content_type,
                                                 resource_identifier));
  return value.get() ? ValueToContentSetting(value.get())
                     : CONTENT_SETTING_DEFAULT;
}

Value* PrefProvider::GetContentSettingValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  // For a |PrefProvider| used in a |HostContentSettingsMap| of a non incognito
  // profile, this will always return NULL.
  // TODO(markusheintz): I don't like this. I'd like to have an
  // IncognitoProviderWrapper that wrapps the pref provider for a host content
  // settings map of an incognito profile.
  base::AutoLock auto_lock(lock_);
  Value* incognito_value = incognito_value_map_.GetValue(
      primary_url,
      secondary_url,
      content_type,
      resource_identifier);
  if (incognito_value)
    return incognito_value->DeepCopy();

  Value* value = value_map_.GetValue(
      primary_url,
      secondary_url,
      content_type,
      resource_identifier);
  return value ? value->DeepCopy() : NULL;
}

void PrefProvider::GetAllContentSettingsRules(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Rules* content_setting_rules) const {
  DCHECK(content_setting_rules);
  content_setting_rules->clear();

  const OriginIdentifierValueMap* map_to_return =
      is_incognito_ ? &incognito_value_map_ : &value_map_;

  base::AutoLock auto_lock(lock_);
  for (OriginIdentifierValueMap::const_iterator entry = map_to_return->begin();
       entry != map_to_return->end();
       ++entry) {
    if (entry->content_type == content_type &&
        entry->identifier == resource_identifier) {
      ContentSetting setting = ValueToContentSetting(entry->value.get());
      DCHECK(setting != CONTENT_SETTING_DEFAULT);
      Rule new_rule(entry->primary_pattern,
                    entry->secondary_pattern,
                    setting);
      content_setting_rules->push_back(new_rule);
    }
  }
}

void PrefProvider::SetContentSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    ContentSetting setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  DCHECK(kTypeNames[content_type] != NULL);

  // Update in memory value map.
  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(lock_);
    if (setting == CONTENT_SETTING_DEFAULT) {
      map_to_modify->DeleteValue(
          primary_pattern,
          secondary_pattern,
          content_type,
          resource_identifier);
    } else {
      map_to_modify->SetValue(
          primary_pattern,
          secondary_pattern,
          content_type,
          resource_identifier,
          Value::CreateIntegerValue(setting));
    }
  }

  // Update the content settings preference.
  if (!is_incognito_) {
    UpdatePref(primary_pattern,
               secondary_pattern,
               content_type,
               resource_identifier,
               setting);
    prefs_->ScheduleSavePersistentPrefs();
  }

  NotifyObservers(
      primary_pattern, secondary_pattern, content_type, resource_identifier);
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(prefs_);

  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(lock_);
    OriginIdentifierValueMap::iterator entry(map_to_modify->begin());
    while (entry != map_to_modify->end()) {
      if (entry->content_type == content_type) {
        UpdatePref(
            entry->primary_pattern,
            entry->secondary_pattern,
            entry->content_type,
            entry->identifier,
            CONTENT_SETTING_DEFAULT);
        // Delete current |entry| and set |entry| to the next value map entry.
        entry = map_to_modify->erase(entry);
      } else {
        ++entry;
      }
    }
  }
  prefs_->ScheduleSavePersistentPrefs();
  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  std::string());
}

void PrefProvider::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    DCHECK_EQ(prefs_, Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    if (!is_incognito_) {
      AutoReset<bool> auto_reset(&updating_preferences_, true);
      std::string* name = Details<std::string>(details).ptr();
      if (*name == prefs::kContentSettingsPatternPairs) {
        SyncObsoletePatternPref();
        SyncObsoletePrefs();
      } else if (*name == prefs::kContentSettingsPatterns) {
        MigrateObsoleteContentSettingsPatternPref();
      } else if (*name == prefs::kGeolocationContentSettings) {
        MigrateObsoleteGeolocationPref();
      } else if (*name == prefs::kDesktopNotificationAllowedOrigins ||
                 *name == prefs::kDesktopNotificationDeniedOrigins) {
        MigrateObsoleteNotificationsPrefs();
      } else {
        NOTREACHED() << "Unexpected preference observed";
        return;
      }
    }
    prefs_->ScheduleSavePersistentPrefs();
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

// ////////////////////////////////////////////////////////////////////////////
// Private

void PrefProvider::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    ContentSetting setting) {
  AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    DictionaryPrefUpdate update(prefs_,
                                prefs::kContentSettingsPatternPairs);
    DictionaryValue* pattern_pairs_settings = update.Get();
    UpdatePatternPairsSettings(primary_pattern,
                               secondary_pattern,
                               content_type,
                               resource_identifier,
                               setting,
                               pattern_pairs_settings);
  }
  if (content_type != CONTENT_SETTINGS_TYPE_GEOLOCATION &&
      content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    UpdateObsoletePatternsPref(primary_pattern,
                               secondary_pattern,
                               content_type,
                               resource_identifier,
                               setting);
  } else if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    UpdateObsoleteGeolocationPref(primary_pattern, secondary_pattern, setting);
  } else if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    ListPrefUpdate update_allowed_sites(
        prefs_, prefs::kDesktopNotificationAllowedOrigins);
    ListPrefUpdate update_denied_sites(
        prefs_, prefs::kDesktopNotificationDeniedOrigins);
    UpdateObsoleteNotificationsSettings(primary_pattern,
                                        secondary_pattern,
                                        setting,
                                        update_allowed_sites.Get(),
                                        update_denied_sites.Get());
  }
}

void PrefProvider::ReadContentSettingsFromPref(bool overwrite) {
  base::AutoLock auto_lock(lock_);

  const DictionaryValue* all_settings_dictionary =
      prefs_->GetDictionary(prefs::kContentSettingsPatternPairs);

  if (overwrite)
    value_map_.clear();

  AutoReset<bool> auto_reset(&updating_preferences_, true);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);
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

        if (RequiresResourceIdentifier(content_type)) {
          const std::string content_type_str = kResourceTypeNames[i];
          DictionaryValue* resource_dictionary = NULL;
          if (settings_dictionary->GetDictionary(
                  content_type_str, &resource_dictionary)) {
            for (DictionaryValue::key_iterator j(
                     resource_dictionary->begin_keys());
                 j != resource_dictionary->end_keys();
                 ++j) {
              const std::string& resource_identifier(*j);
              int setting = CONTENT_SETTING_DEFAULT;
              found = resource_dictionary->GetIntegerWithoutPathExpansion(
                  resource_identifier, &setting);
              DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
              setting = ClickToPlayFixup(content_type,
                                         ContentSetting(setting));
              value_map_.SetValue(pattern_pair.first,
                                  pattern_pair.second,
                                  content_type,
                                  resource_identifier,
                                  Value::CreateIntegerValue(setting));
            }
          }
        } else if (kTypeNames[i]) {
          const std::string content_type_str(kTypeNames[i]);
          int setting = CONTENT_SETTING_DEFAULT;
          if (settings_dictionary->GetIntegerWithoutPathExpansion(
                  content_type_str, &setting)) {
            DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
            setting = FixObsoleteCookiePromptMode(content_type,
                                                  ContentSetting(setting));
            setting = ClickToPlayFixup(content_type,
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
  }
}

void PrefProvider::UpdateObsoletePatternsPref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting setting) {
  DictionaryPrefUpdate update(prefs_,
                              prefs::kContentSettingsPatterns);
  DictionaryValue* all_settings_dictionary = update.Get();

  // Get settings dictionary for |primary_pattern|.
  std::string pattern_str(primary_pattern.ToString());
  DictionaryValue* settings_dictionary = NULL;
  bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      pattern_str, &settings_dictionary);

  if (!found && (setting != CONTENT_SETTING_DEFAULT)) {
    settings_dictionary = new DictionaryValue;
    all_settings_dictionary->SetWithoutPathExpansion(
        pattern_str, settings_dictionary);
  }

  if (settings_dictionary) {
    if (RequiresResourceIdentifier(content_type)) {
      // Get resource dictionary.
      std::string res_dictionary_path(kResourceTypeNames[content_type]);
      DictionaryValue* resource_dictionary = NULL;
      found = settings_dictionary->GetDictionary(
          res_dictionary_path, &resource_dictionary);
      if (!found) {
        if (setting == CONTENT_SETTING_DEFAULT)
          return;  // Nothing to remove. Exit early.
        resource_dictionary = new DictionaryValue;
        settings_dictionary->Set(res_dictionary_path, resource_dictionary);
      }
      // Update resource dictionary.
      if (setting == CONTENT_SETTING_DEFAULT) {
        resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                        NULL);
        if (resource_dictionary->empty()) {
          settings_dictionary->RemoveWithoutPathExpansion(
              res_dictionary_path, NULL);
        }
      } else {
        resource_dictionary->SetWithoutPathExpansion(
            resource_identifier, Value::CreateIntegerValue(setting));
      }
    } else {
      // Update settings dictionary.
      std::string setting_path(kTypeNames[content_type]);
      if (setting == CONTENT_SETTING_DEFAULT) {
        settings_dictionary->RemoveWithoutPathExpansion(setting_path,
                                                        NULL);
      } else {
        settings_dictionary->SetWithoutPathExpansion(
            setting_path, Value::CreateIntegerValue(setting));
      }
    }
    // Remove the settings dictionary if it is empty.
    if (settings_dictionary->empty()) {
      all_settings_dictionary->RemoveWithoutPathExpansion(
          pattern_str, NULL);
    }
  }
}

void PrefProvider::UpdatePatternPairsSettings(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting setting,
      DictionaryValue* pattern_pairs_settings) {
  // Get settings dictionary for the given patterns.
  std::string pattern_str(CreatePatternString(primary_pattern,
                                              secondary_pattern));
  DictionaryValue* settings_dictionary = NULL;
  bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
      pattern_str, &settings_dictionary);

  if (!found && (setting != CONTENT_SETTING_DEFAULT)) {
    settings_dictionary = new DictionaryValue;
    pattern_pairs_settings->SetWithoutPathExpansion(
        pattern_str, settings_dictionary);
  }

  if (settings_dictionary) {
    if (RequiresResourceIdentifier(content_type)) {
      // Get resource dictionary.
      std::string res_dictionary_path(kResourceTypeNames[content_type]);
      DictionaryValue* resource_dictionary = NULL;
      found = settings_dictionary->GetDictionary(
          res_dictionary_path, &resource_dictionary);
      if (!found) {
        if (setting == CONTENT_SETTING_DEFAULT)
          return;  // Nothing to remove. Exit early.
        resource_dictionary = new DictionaryValue;
        settings_dictionary->Set(res_dictionary_path, resource_dictionary);
      }
      // Update resource dictionary.
      if (setting == CONTENT_SETTING_DEFAULT) {
        resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                        NULL);
        if (resource_dictionary->empty()) {
          settings_dictionary->RemoveWithoutPathExpansion(
              res_dictionary_path, NULL);
        }
      } else {
        resource_dictionary->SetWithoutPathExpansion(
            resource_identifier, Value::CreateIntegerValue(setting));
      }
    } else {
      // Update settings dictionary.
      std::string setting_path(kTypeNames[content_type]);
      if (setting == CONTENT_SETTING_DEFAULT) {
        settings_dictionary->RemoveWithoutPathExpansion(setting_path,
                                                        NULL);
      } else {
        settings_dictionary->SetWithoutPathExpansion(
            setting_path, Value::CreateIntegerValue(setting));
      }
    }
    // Remove the settings dictionary if it is empty.
    if (settings_dictionary->empty()) {
      pattern_pairs_settings->RemoveWithoutPathExpansion(
          pattern_str, NULL);
    }
  }
}

void PrefProvider::UpdateObsoleteGeolocationPref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting) {
  if (!prefs_)
    return;

  const GURL requesting_origin(primary_pattern.ToString());
  const GURL embedding_origin(secondary_pattern.ToString());
  DCHECK(requesting_origin.is_valid() && embedding_origin.is_valid());

  DictionaryPrefUpdate update(prefs_, prefs::kGeolocationContentSettings);
  DictionaryValue* obsolete_geolocation_settings = update.Get();
  DictionaryValue* requesting_origin_settings_dictionary = NULL;
  obsolete_geolocation_settings->GetDictionaryWithoutPathExpansion(
      requesting_origin.spec(), &requesting_origin_settings_dictionary);
  if (setting == CONTENT_SETTING_DEFAULT) {
    if (requesting_origin_settings_dictionary) {
      requesting_origin_settings_dictionary->RemoveWithoutPathExpansion(
          embedding_origin.spec(), NULL);
      if (requesting_origin_settings_dictionary->empty()) {
        obsolete_geolocation_settings->RemoveWithoutPathExpansion(
            requesting_origin.spec(), NULL);
      }
    }
  } else {
    if (!requesting_origin_settings_dictionary) {
      requesting_origin_settings_dictionary = new DictionaryValue;
      obsolete_geolocation_settings->SetWithoutPathExpansion(
          requesting_origin.spec(), requesting_origin_settings_dictionary);
    }
    DCHECK(requesting_origin_settings_dictionary);
    requesting_origin_settings_dictionary->SetWithoutPathExpansion(
        embedding_origin.spec(), Value::CreateIntegerValue(setting));
  }
}

void PrefProvider::UpdateObsoleteNotificationsSettings(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting,
    ListValue* allowed_sites,
    ListValue* denied_sites) {
  DCHECK_EQ(secondary_pattern, ContentSettingsPattern::Wildcard());
  GURL origin(primary_pattern.ToString());
  DCHECK(origin.is_valid());
  scoped_ptr<StringValue> value(new StringValue(origin.spec()));
  if (setting == CONTENT_SETTING_ALLOW) {
    denied_sites->Remove(*value, NULL);
    allowed_sites->AppendIfNotPresent(value.release());
  } else if (setting == CONTENT_SETTING_BLOCK) {
    allowed_sites->Remove(*value, NULL);
    denied_sites->AppendIfNotPresent(value.release());
  } else if (setting == CONTENT_SETTING_DEFAULT) {
    denied_sites->Remove(*value, NULL);
    allowed_sites->Remove(*value, NULL);
  } else {
    NOTREACHED() << "Setting value: " << setting
                 << " is not supported for notifications";
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
      LOG(DFATAL) << "Invalid pattern strings: " << pattern_str;
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

void PrefProvider::MigrateObsoletePerhostPref() {
  if (prefs_->HasPrefPath(prefs::kPerHostContentSettings)) {
    const DictionaryValue* all_settings_dictionary =
        prefs_->GetDictionary(prefs::kPerHostContentSettings);
    DCHECK(all_settings_dictionary);
    for (DictionaryValue::key_iterator
         i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::string& host(*i);
      ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(
              std::string(ContentSettingsPattern::kDomainWildcard) + host);
      DictionaryValue* host_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          host, &host_settings_dictionary);
      DCHECK(found);

      for (DictionaryValue::key_iterator i(
               host_settings_dictionary->begin_keys());
           i != host_settings_dictionary->end_keys();
           ++i) {
        const std::string& content_type_str(*i);
        ContentSettingsType content_type =
            StringToContentSettingsType(content_type_str);

        if (content_type == CONTENT_SETTINGS_TYPE_DEFAULT) {
          NOTREACHED();
          LOG(DFATAL) << "Skip settings for invalid content settings type '"
                      << content_type_str << "'";
          continue;
        }

        if (!RequiresResourceIdentifier(content_type)) {
          int setting_int_value = CONTENT_SETTING_DEFAULT;
          bool found =
              host_settings_dictionary->GetIntegerWithoutPathExpansion(
                  content_type_str, &setting_int_value);
          DCHECK(found);
          ContentSetting setting = IntToContentSetting(setting_int_value);

          setting = FixObsoleteCookiePromptMode(content_type, setting);
          setting = ClickToPlayFixup(content_type, setting);

          // TODO(markusheintz): Maybe this check can be removed.
          if (setting != CONTENT_SETTING_DEFAULT) {
            SetContentSetting(
                pattern,
                pattern,
                content_type,
                "",
                setting);
          }
        }
      }
    }
    prefs_->ClearPref(prefs::kPerHostContentSettings);
  }
}

void PrefProvider::MigrateObsoletePopupsPref() {
  if (prefs_->HasPrefPath(prefs::kPopupWhitelistedHosts)) {
    const ListValue* whitelist_pref =
        prefs_->GetList(prefs::kPopupWhitelistedHosts);
    for (ListValue::const_iterator i(whitelist_pref->begin());
         i != whitelist_pref->end(); ++i) {
      std::string host;
      (*i)->GetAsString(&host);
      SetContentSetting(ContentSettingsPattern::FromString(host),
                        ContentSettingsPattern::FromString(host),
                        CONTENT_SETTINGS_TYPE_POPUPS,
                        "",
                        CONTENT_SETTING_ALLOW);
    }
    prefs_->ClearPref(prefs::kPopupWhitelistedHosts);
  }
}

void PrefProvider::MigrateObsoleteContentSettingsPatternPref() {
  if (prefs_->HasPrefPath(prefs::kContentSettingsPatterns) && !is_incognito_) {
    const DictionaryValue* patterns_dictionary =
        prefs_->GetDictionary(prefs::kContentSettingsPatterns);

    // A map with an old key, new key mapping. If the new key is empty then the
    // value for the old key will be removed.
    StringMap keys_to_change;
    {
      DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);
      DictionaryValue* pattern_pairs_dictionary = update.Get();
      for (DictionaryValue::key_iterator i(
               patterns_dictionary->begin_keys());
           i != patterns_dictionary->end_keys();
           ++i) {
        const std::string& key(*i);
        // Remove broken pattern keys and fix keys with pattern pairs.
        size_t sep_pos = key.find(",");
        ContentSettingsPattern pattern =
            ContentSettingsPattern::FromString(key.substr(0, sep_pos));

        // Save the key if it contains a invalid patterns to remove it later.
        // Continue and don't try to migrate the broken pattern key.
        if (!pattern.IsValid()) {
          keys_to_change[key] = "";
          continue;
        }

        // If the key contains a pattern pair, then remove the secondary
        // pattern from the key.
        if (sep_pos != std::string::npos) {
          // If the dictionary already has a key that equals the primary pattern
          // of the corrupted pattern pair key, don't fix the key but remove it.
          if (patterns_dictionary->HasKey(pattern.ToString())) {
            keys_to_change[key] = "";
            continue;
          }

          // If there is more than one key with a pattern pair that has the same
          // valid primary pattern, then the value of the last key processed
          // will win and  overwrite the value any previous key.
          keys_to_change[key] = pattern.ToString();
        }

        // Copy dictionary value.
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

    {
      DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatterns);
      DictionaryValue* mutable_patterns_dictionary = update.Get();
      // Fix broken pattern strings.
      for (StringMap::iterator i(keys_to_change.begin());
           i != keys_to_change.end();
           ++i) {
        const StringPair& pattern_str_pair(*i);
        Value* dict_ptr = NULL;
        bool found = mutable_patterns_dictionary->RemoveWithoutPathExpansion(
            pattern_str_pair.first, &dict_ptr);
        scoped_ptr<Value> dict(dict_ptr);
        DCHECK(found);
        if (!pattern_str_pair.second.empty()) {
          mutable_patterns_dictionary->SetWithoutPathExpansion(
              pattern_str_pair.second, dict.release());
        }
      }
    }
  }
}

void PrefProvider::SyncObsoletePatternPref() {
  if (prefs_->HasPrefPath(prefs::kContentSettingsPatternPairs) &&
      !is_incognito_) {
    const DictionaryValue* pattern_pairs_dictionary =
      prefs_->GetDictionary(prefs::kContentSettingsPatternPairs);

    DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatterns);
    DictionaryValue* obsolete_settings_dictionary = update.Get();

    for (DictionaryValue::key_iterator i =
             pattern_pairs_dictionary->begin_keys();
         i != pattern_pairs_dictionary->end_keys();
         ++i) {
      const std::string& key(*i);
      // Validate pattern string and skip it if it is invalid.
      std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
          ParsePatternString(key);
      if (!pattern_pair.first.IsValid() || !pattern_pair.second.IsValid()) {
        LOG(DFATAL) << "Invalid pattern strings: " << key;
        continue;
      }

      DictionaryValue* settings_dictionary = NULL;
      bool found = pattern_pairs_dictionary->GetDictionaryWithoutPathExpansion(
          key, &settings_dictionary);
      DCHECK(found);
      scoped_ptr<DictionaryValue> settings_dictionary_copy(
          new DictionaryValue());
      for (size_t i = CONTENT_SETTINGS_TYPE_COOKIES;
           i <= CONTENT_SETTINGS_TYPE_POPUPS;
           ++i) {
        DCHECK(kTypeNames[i]);
        std::string type_name(kTypeNames[i]);
        if (settings_dictionary->HasKey(type_name)) {
          Value* value = NULL;
          bool found = settings_dictionary->GetWithoutPathExpansion(
              type_name, &value);
          DCHECK(found);
          settings_dictionary_copy->SetWithoutPathExpansion(
              type_name, value->DeepCopy());
        }
      }

      // Ignore empty dictionaryies.
      if (!settings_dictionary_copy->empty()) {
        std::string new_key = pattern_pair.first.ToString();
        // Existing values are overwritten.
        obsolete_settings_dictionary->SetWithoutPathExpansion(
            new_key, settings_dictionary_copy.release());
      }
    }
  }
}

void PrefProvider::MigrateObsoleteGeolocationPref() {
  if (!prefs_->HasPrefPath(prefs::kGeolocationContentSettings))
    return;

  DictionaryPrefUpdate update(prefs_,
                              prefs::kContentSettingsPatternPairs);
  DictionaryValue* pattern_pairs_settings = update.Get();

  const DictionaryValue* geolocation_settings =
      prefs_->GetDictionary(prefs::kGeolocationContentSettings);
  for (DictionaryValue::key_iterator i =
           geolocation_settings->begin_keys();
       i != geolocation_settings->end_keys();
       ++i) {
    const std::string& primary_key(*i);
    GURL primary_url(primary_key);
    DCHECK(primary_url.is_valid());

    DictionaryValue* requesting_origin_settings = NULL;
    bool found = geolocation_settings->GetDictionaryWithoutPathExpansion(
        primary_key, &requesting_origin_settings);
    DCHECK(found);

    for (DictionaryValue::key_iterator j =
             requesting_origin_settings->begin_keys();
         j != requesting_origin_settings->end_keys();
         ++j) {
      const std::string& secondary_key(*j);
      GURL secondary_url(secondary_key);
      DCHECK(secondary_url.is_valid());

      int setting_value;
      found = requesting_origin_settings->GetIntegerWithoutPathExpansion(
          secondary_key, &setting_value);
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
                                 IntToContentSetting(setting_value),
                                 pattern_pairs_settings);
    }
  }
}

void PrefProvider::MigrateObsoleteNotificationsPrefs() {
  // The notifications settings in the preferences
  // prefs::kContentSettingsPatternPairs do not contain the latest
  // notifications settings. So all notification settings are cleared and
  // migrated from the obsolete preferences for notifications settings that
  // contain the latest settings.
  DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);
  DictionaryValue* pattern_pairs_settings = update.Get();
  ClearSettings(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, pattern_pairs_settings);

  const ListValue* allowed_origins =
      prefs_->GetList(prefs::kDesktopNotificationAllowedOrigins);
  for (size_t i = 0; i < allowed_origins->GetSize(); ++i) {
    std::string url_string;
    bool status = allowed_origins->GetString(i, &url_string);
    DCHECK(status);
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url_string));
    DCHECK(primary_pattern.IsValid());
    UpdatePatternPairsSettings(primary_pattern,
                               ContentSettingsPattern::Wildcard(),
                               CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                               std::string(),
                               CONTENT_SETTING_ALLOW,
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
    UpdatePatternPairsSettings(primary_pattern,
                               ContentSettingsPattern::Wildcard(),
                               CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                               std::string(),
                               CONTENT_SETTING_BLOCK,
                               pattern_pairs_settings);
  }
}

void PrefProvider::SyncObsoletePrefs() {
  DCHECK(prefs_);
  DCHECK(prefs_->HasPrefPath(prefs::kContentSettingsPatternPairs));

  // Clear obsolete preferences first. Then copy the settings from the new
  // preference to the obsolete ones.
  prefs_->ClearPref(prefs::kGeolocationContentSettings);
  prefs_->ClearPref(prefs::kDesktopNotificationAllowedOrigins);
  prefs_->ClearPref(prefs::kDesktopNotificationDeniedOrigins);

  ListPrefUpdate update_allowed_origins(
      prefs_, prefs::kDesktopNotificationAllowedOrigins);
  ListPrefUpdate update_denied_origins(
      prefs_, prefs::kDesktopNotificationDeniedOrigins);
  ListValue* allowed_origins = update_allowed_origins.Get();
  ListValue* denied_origins = update_denied_origins.Get();

  const DictionaryValue* pattern_pairs_dictionary =
      prefs_->GetDictionary(prefs::kContentSettingsPatternPairs);
  for (DictionaryValue::key_iterator i =
           pattern_pairs_dictionary->begin_keys();
       i != pattern_pairs_dictionary->end_keys();
       ++i) {
    const std::string& key(*i);
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
        ParsePatternString(key);
    DCHECK(pattern_pair.first.IsValid() && pattern_pair.second.IsValid());

    DictionaryValue* settings_dictionary = NULL;
    pattern_pairs_dictionary->GetDictionaryWithoutPathExpansion(
        key, &settings_dictionary);

    int setting_value = 0;
    if (settings_dictionary->GetInteger(
            kTypeNames[CONTENT_SETTINGS_TYPE_NOTIFICATIONS], &setting_value)) {
      UpdateObsoleteNotificationsSettings(pattern_pair.first,
                                          pattern_pair.second,
                                          ContentSetting(setting_value),
                                          allowed_origins,
                                          denied_origins);
    }

    setting_value = 0;
    if (settings_dictionary->GetInteger(
            kTypeNames[CONTENT_SETTINGS_TYPE_GEOLOCATION], &setting_value)) {
      UpdateObsoleteGeolocationPref(pattern_pair.first,
                                    pattern_pair.second,
                                    ContentSetting(setting_value));
    }
  }
}

}  // namespace content_settings
