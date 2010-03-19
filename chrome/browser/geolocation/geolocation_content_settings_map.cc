// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_content_settings_map.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/base/dns_util.h"
#include "net/base/static_cookie_policy.h"

// static
const ContentSetting
    GeolocationContentSettingsMap::kDefaultSetting = CONTENT_SETTING_ASK;

GeolocationContentSettingsMap::GeolocationContentSettingsMap(Profile* profile)
    : profile_(profile) {
  PrefService* prefs = profile_->GetPrefs();

  // Read global defaults.
  default_content_setting_ = IntToContentSetting(
      prefs->GetInteger(prefs::kGeolocationDefaultContentSetting));
  if (default_content_setting_ == CONTENT_SETTING_DEFAULT)
    default_content_setting_ = kDefaultSetting;

  // Read exceptions.
  const DictionaryValue* all_settings_dictionary =
      prefs->GetDictionary(prefs::kGeolocationContentSettings);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::wstring& wide_origin(*i);
      DictionaryValue* requesting_origin_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          wide_origin, &requesting_origin_settings_dictionary);
      DCHECK(found);
      OneOriginSettings* requesting_origin_settings =
          &content_settings_[GURL(WideToUTF8(wide_origin))];
      GetOneOriginSettingsFromDictionary(requesting_origin_settings_dictionary,
                                         requesting_origin_settings);
    }
  }
}

// static
void GeolocationContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kGeolocationDefaultContentSetting,
                             CONTENT_SETTING_ASK);
  prefs->RegisterDictionaryPref(prefs::kGeolocationContentSettings);
}

ContentSetting GeolocationContentSettingsMap::GetDefaultContentSetting() const {
  AutoLock auto_lock(lock_);
  return default_content_setting_;
}

ContentSetting GeolocationContentSettingsMap::GetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url) const {
  DCHECK(requesting_url.is_valid() && embedding_url.is_valid());
  GURL requesting_origin(requesting_url.GetOrigin());
  GURL embedding_origin(embedding_url.GetOrigin());
  DCHECK(requesting_origin.is_valid() && embedding_origin.is_valid());
  AutoLock auto_lock(lock_);
  AllOriginsSettings::const_iterator i(content_settings_.find(
      requesting_origin));
  if (i != content_settings_.end()) {
    OneOriginSettings::const_iterator j(i->second.find(embedding_origin));
    if (j != i->second.end())
      return j->second;
    if (requesting_origin != embedding_origin) {
      OneOriginSettings::const_iterator any_embedder(i->second.find(GURL()));
      if (any_embedder != i->second.end())
        return any_embedder->second;
    }
  }
  return default_content_setting_;
}

GeolocationContentSettingsMap::AllOriginsSettings
    GeolocationContentSettingsMap::GetAllOriginsSettings() const {
  AutoLock auto_lock(lock_);
  return content_settings_;
}

void GeolocationContentSettingsMap::SetDefaultContentSetting(
    ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  {
    AutoLock auto_lock(lock_);
    default_content_setting_ =
        (setting == CONTENT_SETTING_DEFAULT) ? kDefaultSetting : setting;
  }
  profile_->GetPrefs()->SetInteger(prefs::kGeolocationDefaultContentSetting,
                                   default_content_setting_);
}

void GeolocationContentSettingsMap::SetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url,
    ContentSetting setting) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(requesting_url.is_valid() &&
         (embedding_url.is_valid() || embedding_url.is_empty()));
  GURL requesting_origin(requesting_url.GetOrigin());
  GURL embedding_origin(embedding_url.GetOrigin());
  DCHECK(requesting_origin.is_valid() &&
         (embedding_origin.is_valid() || embedding_url.is_empty()));
  std::wstring wide_requesting_origin(UTF8ToWide(requesting_origin.spec()));
  std::wstring wide_embedding_origin(UTF8ToWide(embedding_origin.spec()));
  DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetMutableDictionary(
          prefs::kGeolocationContentSettings);

  AutoLock auto_lock(lock_);
  DictionaryValue* requesting_origin_settings_dictionary;
  all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      wide_requesting_origin, &requesting_origin_settings_dictionary);
  if (setting == CONTENT_SETTING_DEFAULT) {
    if (!content_settings_.count(requesting_origin) ||
        !content_settings_[requesting_origin].count(embedding_origin))
      return;
    if (content_settings_[requesting_origin].size() == 1) {
      all_settings_dictionary->RemoveWithoutPathExpansion(
          wide_requesting_origin, NULL);
      content_settings_.erase(requesting_origin);
      return;
    }
    requesting_origin_settings_dictionary->RemoveWithoutPathExpansion(
        wide_embedding_origin, NULL);
    content_settings_[requesting_origin].erase(embedding_origin);
    return;
  }

  if (!content_settings_.count(requesting_origin)) {
    requesting_origin_settings_dictionary = new DictionaryValue;
    all_settings_dictionary->SetWithoutPathExpansion(
        wide_requesting_origin, requesting_origin_settings_dictionary);
  }
  content_settings_[requesting_origin][embedding_origin] = setting;
  DCHECK(requesting_origin_settings_dictionary);
  requesting_origin_settings_dictionary->SetWithoutPathExpansion(
      wide_embedding_origin, Value::CreateIntegerValue(setting));
}

void GeolocationContentSettingsMap::ClearOneRequestingOrigin(
    const GURL& requesting_origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(requesting_origin.is_valid());

  {
    AutoLock auto_lock(lock_);
    AllOriginsSettings::iterator i(content_settings_.find(requesting_origin));
    if (i == content_settings_.end())
      return;
    content_settings_.erase(i);
  }

  DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetMutableDictionary(
          prefs::kGeolocationContentSettings);
  all_settings_dictionary->RemoveWithoutPathExpansion(
      UTF8ToWide(requesting_origin.spec()), NULL);
}

void GeolocationContentSettingsMap::ResetToDefault() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    default_content_setting_ = kDefaultSetting;
    content_settings_.clear();
  }

  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kGeolocationDefaultContentSetting);
  prefs->ClearPref(prefs::kGeolocationContentSettings);
}

GeolocationContentSettingsMap::~GeolocationContentSettingsMap() {
}

// static
void GeolocationContentSettingsMap::GetOneOriginSettingsFromDictionary(
    const DictionaryValue* dictionary,
    OneOriginSettings* one_origin_settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::wstring& target(*i);
    int setting = kDefaultSetting;
    bool found = dictionary->GetIntegerWithoutPathExpansion(target, &setting);
    DCHECK(found);
    (*one_origin_settings)[GURL(WideToUTF8(target))] =
        IntToContentSetting(setting);
  }
}
