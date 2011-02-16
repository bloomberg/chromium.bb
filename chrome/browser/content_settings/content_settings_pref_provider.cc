// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pref_provider.h"

#include <string>
#include <vector>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

// The preference keys where resource identifiers are stored for
// ContentSettingsType values that support resource identifiers.
const char* kResourceTypeNames[CONTENT_SETTINGS_NUM_TYPES] = {
  NULL,
  NULL,
  NULL,
  "per_plugin",
  NULL,
  NULL,  // Not used for Geolocation
  NULL,  // Not used for Notifications
};

// The default setting for each content type.
const ContentSetting kDefaultSettings[CONTENT_SETTINGS_NUM_TYPES] = {
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_COOKIES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_IMAGES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_PLUGINS
  CONTENT_SETTING_BLOCK,  // CONTENT_SETTINGS_TYPE_POPUPS
  CONTENT_SETTING_ASK,    // Not used for Geolocation
  CONTENT_SETTING_ASK,    // Not used for Notifications
};

// The names of the ContentSettingsType values, for use with dictionary prefs.
const char* kTypeNames[CONTENT_SETTINGS_NUM_TYPES] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  NULL,  // Not used for Geolocation
  NULL,  // Not used for Notifications
};

}  // namespace

namespace content_settings {

PrefDefaultProvider::PrefDefaultProvider(Profile* profile)
    : profile_(profile),
      is_off_the_record_(profile_->IsOffTheRecord()),
      updating_preferences_(false) {
  PrefService* prefs = profile->GetPrefs();

  // Read global defaults.
  DCHECK_EQ(arraysize(kTypeNames),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  ReadDefaultSettings(true);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kDefaultContentSettings, this);
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

PrefDefaultProvider::~PrefDefaultProvider() {
  UnregisterObservers();
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
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_PLUGINS ||
         setting != CONTENT_SETTING_ASK ||
         CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableClickToPlay));

  // The default settings may not be directly modified for OTR sessions.
  // Instead, they are synced to the main profile's setting.
  if (is_off_the_record_)
    return;

  PrefService* prefs = profile_->GetPrefs();

  DictionaryValue* default_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kDefaultContentSettings);
  std::string dictionary_path(kTypeNames[content_type]);
  updating_preferences_ = true;
  {
    base::AutoLock lock(lock_);
    ScopedPrefUpdate update(prefs, prefs::kDefaultContentSettings);
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
  }
  updating_preferences_ = false;

  NotifyObservers(
      ContentSettingsDetails(ContentSettingsPattern(), content_type, ""));
}

bool PrefDefaultProvider::DefaultSettingIsManaged(
    ContentSettingsType content_type) const {
  return false;
}

void PrefDefaultProvider::ResetToDefaults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  default_content_settings_ = ContentSettings();
  ForceDefaultsToBeExplicit();

  if (!is_off_the_record_) {
    PrefService* prefs = profile_->GetPrefs();
    updating_preferences_ = true;
    prefs->ClearPref(prefs::kDefaultContentSettings);
    updating_preferences_ = false;
  }
}

void PrefDefaultProvider::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == NotificationType::PREF_CHANGED) {
    DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kDefaultContentSettings) {
      ReadDefaultSettings(true);
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    if (!is_off_the_record_) {
      NotifyObservers(ContentSettingsDetails(
            ContentSettingsPattern(), CONTENT_SETTINGS_TYPE_DEFAULT, ""));
    }
  } else if (type == NotificationType::PROFILE_DESTROYED) {
    DCHECK_EQ(profile_, Source<Profile>(source).ptr());
    UnregisterObservers();
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

void PrefDefaultProvider::UnregisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  pref_change_registrar_.RemoveAll();
  notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                 Source<Profile>(profile_));
  profile_ = NULL;
}

void PrefDefaultProvider::ReadDefaultSettings(bool overwrite) {
  PrefService* prefs = profile_->GetPrefs();
  const DictionaryValue* default_settings_dictionary =
      prefs->GetDictionary(prefs::kDefaultContentSettings);

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
      BaseProvider::ClickToPlayFixup(
          CONTENT_SETTINGS_TYPE_PLUGINS,
          settings->settings[CONTENT_SETTINGS_TYPE_PLUGINS]);
}

void PrefDefaultProvider::NotifyObservers(
    const ContentSettingsDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_ == NULL)
    return;
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(profile_->GetHostContentSettingsMap()),
      Details<const ContentSettingsDetails>(&details));
}

// static
void PrefDefaultProvider::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kDefaultContentSettings);
}

// ////////////////////////////////////////////////////////////////////////////
// PrefProvider:
//

// static
void PrefProvider::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion);
  prefs->RegisterDictionaryPref(prefs::kContentSettingsPatterns);

  // Obsolete prefs, for migration:
  prefs->RegisterListPref(prefs::kPopupWhitelistedHosts);
  prefs->RegisterDictionaryPref(prefs::kPerHostContentSettings);
}

PrefProvider::PrefProvider(Profile* profile)
  : BaseProvider(profile->IsOffTheRecord()),
    profile_(profile),
    updating_preferences_(false) {
  Init();
}

void PrefProvider::Init() {
  initializing_ = true;
  PrefService* prefs = profile_->GetPrefs();

  // Migrate obsolete preferences.
  MigrateObsoletePerhostPref(prefs);
  MigrateObsoletePopupsPref(prefs);

  // Verify preferences version.
  if (!prefs->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs->SetInteger(prefs::kContentSettingsVersion,
                      ContentSettingsPattern::kContentSettingsPatternVersion);
  }
  if (prefs->GetInteger(prefs::kContentSettingsVersion) >
      ContentSettingsPattern::kContentSettingsPatternVersion) {
    LOG(ERROR) << "Unknown content settings version in preferences.";
    return;
  }

  // Read exceptions.
  ReadExceptions(false);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kContentSettingsPatterns, this);

  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
  initializing_ = false;
}

bool PrefProvider::ContentSettingsTypeIsManaged(
    ContentSettingsType content_type) {
  return false;
}

void PrefProvider::SetContentSetting(
    const ContentSettingsPattern& requesting_pattern,
    const ContentSettingsPattern& embedding_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    ContentSetting setting) {
  // Support for embedding_patterns is not implemented yet.
  DCHECK(requesting_pattern == embedding_pattern);

  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(RequiresResourceIdentifier(content_type),
            resource_identifier.empty());
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_PLUGINS ||
         setting != CONTENT_SETTING_ASK ||
         CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableClickToPlay));

  const ContentSettingsPattern pattern(
      requesting_pattern.CanonicalizePattern());

  bool early_exit = false;
  std::string pattern_str(pattern.AsString());
  PrefService* prefs = NULL;
  DictionaryValue* all_settings_dictionary = NULL;

  // Select content-settings-map to write to.
  HostContentSettings* map_to_modify = off_the_record_settings();
  if (!is_off_the_record()) {
    prefs = profile_->GetPrefs();
    all_settings_dictionary =
        prefs->GetMutableDictionary(prefs::kContentSettingsPatterns);

    map_to_modify = host_content_settings();
  }

  // Update content-settings-map.
  {
    base::AutoLock auto_lock(lock());
    if (!map_to_modify->count(pattern_str))
      (*map_to_modify)[pattern_str].content_settings = ContentSettings();
    HostContentSettings::iterator i(
        map_to_modify->find(pattern_str));
    ContentSettings& settings = i->second.content_settings;
    if (RequiresResourceIdentifier(content_type)) {
      settings.settings[content_type] = CONTENT_SETTING_DEFAULT;
      if (setting != CONTENT_SETTING_DEFAULT) {
        i->second.content_settings_for_resources[
            ContentSettingsTypeResourceIdentifierPair(content_type,
            resource_identifier)] = setting;
      } else {
        i->second.content_settings_for_resources.erase(
            ContentSettingsTypeResourceIdentifierPair(content_type,
            resource_identifier));
      }
    } else {
      settings.settings[content_type] = setting;
    }
    if (AllDefault(i->second)) {
      map_to_modify->erase(i);
      if (all_settings_dictionary)
        all_settings_dictionary->RemoveWithoutPathExpansion(pattern_str, NULL);

      // We can't just return because |NotifyObservers()| needs to be called,
      // without lock() being held.
      early_exit = true;
    }
  }

  // Update the content_settings preference.
  if (!early_exit && all_settings_dictionary) {
    DictionaryValue* host_settings_dictionary = NULL;
    bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
        pattern_str, &host_settings_dictionary);
    if (!found) {
      host_settings_dictionary = new DictionaryValue;
      all_settings_dictionary->SetWithoutPathExpansion(
          pattern_str, host_settings_dictionary);
      DCHECK_NE(setting, CONTENT_SETTING_DEFAULT);
    }
    if (RequiresResourceIdentifier(content_type)) {
      std::string dictionary_path(kResourceTypeNames[content_type]);
      DictionaryValue* resource_dictionary = NULL;
      found = host_settings_dictionary->GetDictionary(
          dictionary_path, &resource_dictionary);
      if (!found) {
        resource_dictionary = new DictionaryValue;
        host_settings_dictionary->Set(dictionary_path, resource_dictionary);
      }
      if (setting == CONTENT_SETTING_DEFAULT) {
        resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                        NULL);
      } else {
        resource_dictionary->SetWithoutPathExpansion(
            resource_identifier, Value::CreateIntegerValue(setting));
      }
    } else {
      std::string dictionary_path(kTypeNames[content_type]);
      if (setting == CONTENT_SETTING_DEFAULT) {
        host_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path,
                                                             NULL);
      } else {
        host_settings_dictionary->SetWithoutPathExpansion(
            dictionary_path, Value::CreateIntegerValue(setting));
      }
    }
  }

  updating_preferences_ = true;
  if (!is_off_the_record())
    ScopedPrefUpdate update(prefs, prefs::kContentSettingsPatterns);
  updating_preferences_ = false;

  NotifyObservers(ContentSettingsDetails(pattern, content_type, ""));
}

void PrefProvider::ResetToDefaults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock());
    host_content_settings()->clear();
    off_the_record_settings()->clear();
  }

  if (!is_off_the_record()) {
    PrefService* prefs = profile_->GetPrefs();
    updating_preferences_ = true;
    prefs->ClearPref(prefs::kContentSettingsPatterns);
    updating_preferences_ = false;
  }
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(kTypeNames[content_type] != NULL);  // Don't call this for Geolocation.

  PrefService* prefs = NULL;
  DictionaryValue* all_settings_dictionary = NULL;
  HostContentSettings* map_to_modify = off_the_record_settings();

  if (!is_off_the_record()) {
    prefs = profile_->GetPrefs();
    all_settings_dictionary =
        prefs->GetMutableDictionary(prefs::kContentSettingsPatterns);
    map_to_modify = host_content_settings();
  }

  {
    base::AutoLock auto_lock(lock());
    for (HostContentSettings::iterator i(map_to_modify->begin());
         i != map_to_modify->end(); ) {
      if (RequiresResourceIdentifier(content_type) ||
          i->second.content_settings.settings[content_type] !=
              CONTENT_SETTING_DEFAULT) {
        if (RequiresResourceIdentifier(content_type))
          i->second.content_settings_for_resources.clear();
        i->second.content_settings.settings[content_type] =
            CONTENT_SETTING_DEFAULT;
        std::string host(i->first);
        if (AllDefault(i->second)) {
          if (all_settings_dictionary)
            all_settings_dictionary->RemoveWithoutPathExpansion(host, NULL);
          map_to_modify->erase(i++);
        } else if (all_settings_dictionary) {
          DictionaryValue* host_settings_dictionary;
          bool found =
              all_settings_dictionary->GetDictionaryWithoutPathExpansion(
                  host, &host_settings_dictionary);
          DCHECK(found);
          host_settings_dictionary->RemoveWithoutPathExpansion(
              kTypeNames[content_type], NULL);
          ++i;
        }
      } else {
        ++i;
      }
    }
  }

  updating_preferences_ = true;
  if (!is_off_the_record())
    ScopedPrefUpdate update(prefs, prefs::kContentSettingsPatterns);
  updating_preferences_ = false;

  NotifyObservers(
      ContentSettingsDetails(ContentSettingsPattern(), content_type, ""));
}

void PrefProvider::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == NotificationType::PREF_CHANGED) {
    DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kContentSettingsPatterns) {
      ReadExceptions(true);
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    if (!is_off_the_record()) {
      NotifyObservers(ContentSettingsDetails(ContentSettingsPattern(),
                                             CONTENT_SETTINGS_TYPE_DEFAULT,
                                             ""));
    }
  } else if (type == NotificationType::PROFILE_DESTROYED) {
    DCHECK_EQ(profile_, Source<Profile>(source).ptr());
    UnregisterObservers();
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

PrefProvider::~PrefProvider() {
  UnregisterObservers();
}

// ////////////////////////////////////////////////////////////////////////////
// Private

void PrefProvider::ReadExceptions(bool overwrite) {
  base::AutoLock auto_lock(lock());

  PrefService* prefs = profile_->GetPrefs();
  DictionaryValue* all_settings_dictionary =
      prefs->GetMutableDictionary(prefs::kContentSettingsPatterns);

  if (overwrite)
    host_content_settings()->clear();

  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    // Convert all Unicode patterns into punycode form, then read.
    CanonicalizeContentSettingsExceptions(all_settings_dictionary);

    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::string& pattern(*i);
      if (!ContentSettingsPattern(pattern).IsValid())
        LOG(WARNING) << "Invalid pattern stored in content settings";
      DictionaryValue* pattern_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          pattern, &pattern_settings_dictionary);
      DCHECK(found);

      ExtendedContentSettings extended_settings;
      GetSettingsFromDictionary(pattern_settings_dictionary,
                                &extended_settings.content_settings);
      GetResourceSettingsFromDictionary(
          pattern_settings_dictionary,
          &extended_settings.content_settings_for_resources);

      (*host_content_settings())[pattern] = extended_settings;
    }
  }
}

void PrefProvider::CanonicalizeContentSettingsExceptions(
    DictionaryValue* all_settings_dictionary) {
  DCHECK(all_settings_dictionary);

  std::vector<std::string> remove_items;
  std::vector<std::pair<std::string, std::string> > move_items;
  for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
       i != all_settings_dictionary->end_keys(); ++i) {
    const std::string& pattern(*i);
    const std::string canonicalized_pattern =
        ContentSettingsPattern(pattern).CanonicalizePattern();

    if (canonicalized_pattern.empty() || canonicalized_pattern == pattern)
      continue;

    // Clear old pattern if prefs already have canonicalized pattern.
    DictionaryValue* new_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            canonicalized_pattern, &new_pattern_settings_dictionary)) {
      remove_items.push_back(pattern);
      continue;
    }

    // Move old pattern to canonicalized pattern.
    DictionaryValue* old_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            pattern, &old_pattern_settings_dictionary)) {
      move_items.push_back(std::make_pair(pattern, canonicalized_pattern));
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

void PrefProvider::GetSettingsFromDictionary(
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

void PrefProvider::GetResourceSettingsFromDictionary(
    const DictionaryValue* dictionary,
    ResourceContentSettings* settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::string& content_type(*i);
    for (size_t type = 0; type < arraysize(kResourceTypeNames); ++type) {
      if ((kResourceTypeNames[type] != NULL) &&
          (kResourceTypeNames[type] == content_type)) {
        DictionaryValue* resource_dictionary = NULL;
        bool found = dictionary->GetDictionary(content_type,
                                               &resource_dictionary);
        DCHECK(found);
        for (DictionaryValue::key_iterator j(resource_dictionary->begin_keys());
             j != resource_dictionary->end_keys(); ++j) {
          const std::string& resource_identifier(*j);
          int setting = CONTENT_SETTING_DEFAULT;
          bool found = resource_dictionary->GetIntegerWithoutPathExpansion(
              resource_identifier, &setting);
          DCHECK(found);
          (*settings)[ContentSettingsTypeResourceIdentifierPair(
              ContentSettingsType(type), resource_identifier)] =
                  ClickToPlayFixup(ContentSettingsType(type),
                                   ContentSetting(setting));
        }

        break;
      }
    }
  }
}

void PrefProvider::NotifyObservers(
    const ContentSettingsDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (initializing_ || profile_ == NULL)
    return;
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(
          profile_->GetHostContentSettingsMap()),
      Details<const ContentSettingsDetails>(&details));
}

void PrefProvider::UnregisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  pref_change_registrar_.RemoveAll();
  notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                 Source<Profile>(profile_));
  profile_ = NULL;
}

void PrefProvider::MigrateObsoletePerhostPref(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kPerHostContentSettings)) {
    const DictionaryValue* all_settings_dictionary =
        prefs->GetDictionary(prefs::kPerHostContentSettings);
    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::string& host(*i);
      ContentSettingsPattern pattern(
          std::string(ContentSettingsPattern::kDomainWildcard) + host);
      DictionaryValue* host_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          host, &host_settings_dictionary);
      DCHECK(found);
      ContentSettings settings;
      GetSettingsFromDictionary(host_settings_dictionary, &settings);
      for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
        if (settings.settings[j] != CONTENT_SETTING_DEFAULT &&
            !RequiresResourceIdentifier(ContentSettingsType(j))) {
          SetContentSetting(
              pattern,
              pattern,
              ContentSettingsType(j),
              "",
              settings.settings[j]);
        }
      }
    }
    prefs->ClearPref(prefs::kPerHostContentSettings);
  }
}

void PrefProvider::MigrateObsoletePopupsPref(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kPopupWhitelistedHosts)) {
    const ListValue* whitelist_pref =
        prefs->GetList(prefs::kPopupWhitelistedHosts);
    for (ListValue::const_iterator i(whitelist_pref->begin());
         i != whitelist_pref->end(); ++i) {
      std::string host;
      (*i)->GetAsString(&host);
      SetContentSetting(ContentSettingsPattern(host),
                        ContentSettingsPattern(host),
                        CONTENT_SETTINGS_TYPE_POPUPS,
                        "",
                        CONTENT_SETTING_ALLOW);
    }
    prefs->ClearPref(prefs::kPopupWhitelistedHosts);
  }
}

}  // namespace content_settings
