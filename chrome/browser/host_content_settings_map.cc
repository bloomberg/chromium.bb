// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

// static
const wchar_t* HostContentSettingsMap::kTypeNames[] = {
  L"cookies",
  L"images",
  L"javascript",
  L"plugins",
  L"popups",
};

HostContentSettingsMap::HostContentSettingsMap(Profile* profile)
    : profile_(profile),
      block_third_party_cookies_(false) {
  DCHECK_EQ(arraysize(kTypeNames),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));

  const DictionaryValue* default_settings_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kDefaultContentSettings);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (default_settings_dictionary != NULL) {
    GetSettingsFromDictionary(default_settings_dictionary,
                              &default_content_settings_);
  }

  const DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kPerHostContentSettings);
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

  // TODO(darin): init third-party cookie pref
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kDefaultContentSettings);
  prefs->RegisterDictionaryPref(prefs::kPerHostContentSettings);

  // TODO(darin): register third-party cookie pref
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

  ContentSettings output;
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (i->second.settings[i] == CONTENT_SETTING_DEFAULT) {
      output.settings[i] = default_content_settings_.settings[i];
    } else {
      output.settings[i] = i->second.settings[i];
    }
  }
  return output;
}

void HostContentSettingsMap::GetHostContentSettingsForOneType(
    ContentSettingsType content_type,
    HostContentSettingsForOneType* settings) const {
  DCHECK(settings);
  settings->clear();

  AutoLock auto_lock(lock_);
  for (HostContentSettings::const_iterator i(host_content_settings_.begin());
       i != host_content_settings_.end(); ++i) {
    ContentSetting setting = i->second.settings[content_type];
    if (setting != CONTENT_SETTING_DEFAULT)
      (*settings)[i->first] = setting;
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    default_content_settings_.settings[content_type] = setting;
  }

  profile_->GetPrefs()->GetMutableDictionary(prefs::kDefaultContentSettings)->
      SetWithoutPathExpansion(std::wstring(kTypeNames[content_type]),
                              Value::CreateIntegerValue(setting));
}

void HostContentSettingsMap::SetContentSetting(const std::string& host,
                                               ContentSettingsType content_type,
                                               ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  bool all_default = true;
  {
    AutoLock auto_lock(lock_);
    if (!host_content_settings_.count(host))
      host_content_settings_[host] = ContentSettings();
    HostContentSettings::iterator i(host_content_settings_.find(host));
    ContentSettings& settings = i->second;
    settings.settings[content_type] = setting;
    for (size_t i = 0; i < arraysize(settings.settings); ++i) {
      if (settings.settings[i] != CONTENT_SETTING_DEFAULT) {
        all_default = false;
        break;
      }
    }
    if (all_default)
      host_content_settings_.erase(i);
  }

  std::wstring wide_host(UTF8ToWide(host));
  DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetMutableDictionary(
          prefs::kPerHostContentSettings);
  if (all_default) {
    all_settings_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
    return;
  }
  DictionaryValue* host_settings_dictionary;
  bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      wide_host, &host_settings_dictionary);
  if (!found) {
    host_settings_dictionary = new DictionaryValue;
    all_settings_dictionary->SetWithoutPathExpansion(
        wide_host, host_settings_dictionary);
  }
  host_settings_dictionary->SetWithoutPathExpansion(
      std::wstring(kTypeNames[content_type]),
      Value::CreateIntegerValue(setting));
}

void HostContentSettingsMap::ResetToDefaults() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    default_content_settings_ = ContentSettings();
    host_content_settings_.clear();
  }

  profile_->GetPrefs()->ClearPref(prefs::kDefaultContentSettings);
  profile_->GetPrefs()->ClearPref(prefs::kPerHostContentSettings);

  // TODO(darin): clear third-party cookie pref
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
