// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/static_cookie_policy.h"

// static
const wchar_t*
    HostContentSettingsMap::kTypeNames[CONTENT_SETTINGS_NUM_TYPES] = {
  L"cookies",
  L"images",
  L"javascript",
  L"plugins",
  L"popups",
};

// static
const ContentSetting
    HostContentSettingsMap::kDefaultSettings[CONTENT_SETTINGS_NUM_TYPES] = {
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_COOKIES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_IMAGES
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  CONTENT_SETTING_ALLOW,  // CONTENT_SETTINGS_TYPE_PLUGINS
  CONTENT_SETTING_BLOCK,  // CONTENT_SETTINGS_TYPE_POPUPS
};

HostContentSettingsMap::HostContentSettingsMap(Profile* profile)
    : profile_(profile),
      block_third_party_cookies_(false) {
  PrefService* prefs = profile_->GetPrefs();

  // Migrate obsolete cookie pref.
  if (prefs->HasPrefPath(prefs::kCookieBehavior)) {
    int cookie_behavior = prefs->GetInteger(prefs::kCookieBehavior);
    prefs->ClearPref(prefs::kCookieBehavior);
    if (!prefs->HasPrefPath(prefs::kDefaultContentSettings)) {
        SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
            (cookie_behavior == net::StaticCookiePolicy::BLOCK_ALL_COOKIES) ?
                CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
    }
    if (!prefs->HasPrefPath(prefs::kBlockThirdPartyCookies)) {
      SetBlockThirdPartyCookies(cookie_behavior ==
          net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
    }
  }

  // Migrate obsolete popups pref.
  if (prefs->HasPrefPath(prefs::kPopupWhitelistedHosts)) {
    const ListValue* whitelist_pref =
        prefs->GetList(prefs::kPopupWhitelistedHosts);
    for (ListValue::const_iterator i(whitelist_pref->begin());
         i != whitelist_pref->end(); ++i) {
      std::string host;
      (*i)->GetAsString(&host);
      SetContentSetting(host, CONTENT_SETTINGS_TYPE_POPUPS,
                        CONTENT_SETTING_ALLOW);
    }
    prefs->ClearPref(prefs::kPopupWhitelistedHosts);
  }

  // Read global defaults.
  DCHECK_EQ(arraysize(kTypeNames),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  const DictionaryValue* default_settings_dictionary =
      prefs->GetDictionary(prefs::kDefaultContentSettings);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (default_settings_dictionary != NULL) {
    GetSettingsFromDictionary(default_settings_dictionary,
                              &default_content_settings_);
  }
  ForceDefaultsToBeExplicit();

  // Read host-specific exceptions.
  const DictionaryValue* all_settings_dictionary =
      prefs->GetDictionary(prefs::kPerHostContentSettings);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      std::wstring wide_host(*i);
      DictionaryValue* host_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          wide_host, &host_settings_dictionary);
      DCHECK(found);
      ContentSettings settings;
      GetSettingsFromDictionary(host_settings_dictionary, &settings);
      host_content_settings_[WideToUTF8(wide_host)] = settings;
    }
  }

  // Read misc. global settings.
  block_third_party_cookies_ =
      prefs->GetBoolean(prefs::kBlockThirdPartyCookies);
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kDefaultContentSettings);
  prefs->RegisterDictionaryPref(prefs::kPerHostContentSettings);
  prefs->RegisterBooleanPref(prefs::kBlockThirdPartyCookies, false);

  // Obsolete prefs, for migration:
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
                             net::StaticCookiePolicy::ALLOW_ALL_COOKIES);
  prefs->RegisterListPref(prefs::kPopupWhitelistedHosts);
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type) const {
  AutoLock auto_lock(lock_);
  return default_content_settings_.settings[content_type];
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const std::string& host,
    ContentSettingsType content_type) const {
  AutoLock auto_lock(lock_);
  HostContentSettings::const_iterator i(host_content_settings_.find(host));
  return (i == host_content_settings_.end()) ?
      default_content_settings_.settings[content_type] :
      i->second.settings[content_type];
}

ContentSettings HostContentSettingsMap::GetContentSettings(
    const std::string& host) const {
  AutoLock auto_lock(lock_);
  HostContentSettings::const_iterator i(host_content_settings_.find(host));
  if (i == host_content_settings_.end())
    return default_content_settings_;

  ContentSettings output = i->second;
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    if (output.settings[j] == CONTENT_SETTING_DEFAULT)
      output.settings[j] = default_content_settings_.settings[j];
  }
  return output;
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    SettingsForOneType* settings) const {
  DCHECK(settings);
  settings->clear();

  AutoLock auto_lock(lock_);
  for (HostContentSettings::const_iterator i(host_content_settings_.begin());
       i != host_content_settings_.end(); ++i) {
    ContentSetting setting = i->second.settings[content_type];
    if (setting != CONTENT_SETTING_DEFAULT) {
      // Use of push_back() relies on the map iterator traversing in order of
      // ascending keys.
      settings->push_back(std::make_pair(i->first, setting));
    }
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  DictionaryValue* default_settings_dictionary =
      profile_->GetPrefs()->GetMutableDictionary(
          prefs::kDefaultContentSettings);
  std::wstring dictionary_path(kTypeNames[content_type]);
  {
    AutoLock auto_lock(lock_);
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

  NotifyObservers(std::string());
}

void HostContentSettingsMap::SetContentSetting(const std::string& host,
                                               ContentSettingsType content_type,
                                               ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  std::wstring wide_host(UTF8ToWide(host));
  DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetMutableDictionary(
          prefs::kPerHostContentSettings);
  {
    AutoLock auto_lock(lock_);
    if (!host_content_settings_.count(host))
      host_content_settings_[host] = ContentSettings();
    HostContentSettings::iterator i(host_content_settings_.find(host));
    ContentSettings& settings = i->second;
    settings.settings[content_type] = setting;
    if (AllDefault(settings)) {
      host_content_settings_.erase(i);
      all_settings_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
      return;
    }
  }

  DictionaryValue* host_settings_dictionary;
  bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      wide_host, &host_settings_dictionary);
  if (!found) {
    host_settings_dictionary = new DictionaryValue;
    all_settings_dictionary->SetWithoutPathExpansion(
        wide_host, host_settings_dictionary);
    DCHECK_NE(setting, CONTENT_SETTING_DEFAULT);
  }
  std::wstring dictionary_path(kTypeNames[content_type]);
  if (setting == CONTENT_SETTING_DEFAULT) {
    host_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path, NULL);
  } else {
    host_settings_dictionary->SetWithoutPathExpansion(
        dictionary_path, Value::CreateIntegerValue(setting));
  }

  NotifyObservers(host);
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  {
    AutoLock auto_lock(lock_);
    for (HostContentSettings::iterator i(host_content_settings_.begin());
         i != host_content_settings_.end(); ) {
      if (i->second.settings[content_type] != CONTENT_SETTING_DEFAULT) {
        i->second.settings[content_type] = CONTENT_SETTING_DEFAULT;
        std::wstring wide_host(UTF8ToWide(i->first));
        DictionaryValue* all_settings_dictionary =
            profile_->GetPrefs()->GetMutableDictionary(
                prefs::kPerHostContentSettings);
        if (AllDefault(i->second)) {
          all_settings_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
          host_content_settings_.erase(i++);
        } else {
          DictionaryValue* host_settings_dictionary;
          bool found =
              all_settings_dictionary->GetDictionaryWithoutPathExpansion(
                  wide_host, &host_settings_dictionary);
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

  NotifyObservers(std::string());
}

void HostContentSettingsMap::SetBlockThirdPartyCookies(bool block) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    block_third_party_cookies_ = block;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (block)
    prefs->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  else
    prefs->ClearPref(prefs::kBlockThirdPartyCookies);
}

void HostContentSettingsMap::ResetToDefaults() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    default_content_settings_ = ContentSettings();
    ForceDefaultsToBeExplicit();
    host_content_settings_.clear();
    block_third_party_cookies_ = false;
  }

  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kDefaultContentSettings);
  prefs->ClearPref(prefs::kPerHostContentSettings);
  prefs->ClearPref(prefs::kBlockThirdPartyCookies);

  NotifyObservers(std::string());
}

HostContentSettingsMap::~HostContentSettingsMap() {
}

void HostContentSettingsMap::GetSettingsFromDictionary(
    const DictionaryValue* dictionary,
    ContentSettings* settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    std::wstring content_type(*i);
    int setting = CONTENT_SETTING_DEFAULT;
    bool found = dictionary->GetIntegerWithoutPathExpansion(content_type,
                                                            &setting);
    DCHECK(found);
    for (size_t type = 0; type < arraysize(kTypeNames); ++type) {
      if (std::wstring(kTypeNames[type]) == content_type) {
        settings->settings[type] = static_cast<ContentSetting>(setting);
        break;
      }
    }
  }
}

void HostContentSettingsMap::ForceDefaultsToBeExplicit() {
  DCHECK_EQ(arraysize(kDefaultSettings),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (default_content_settings_.settings[i] == CONTENT_SETTING_DEFAULT)
      default_content_settings_.settings[i] = kDefaultSettings[i];
  }
}

bool HostContentSettingsMap::AllDefault(const ContentSettings& settings) const {
  for (size_t i = 0; i < arraysize(settings.settings); ++i) {
    if (settings.settings[i] != CONTENT_SETTING_DEFAULT)
      return false;
  }
  return true;
}

void HostContentSettingsMap::NotifyObservers(const std::string& host) {
  ContentSettingsDetails details(host);
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(this),
      Details<ContentSettingsDetails>(&details));
}
