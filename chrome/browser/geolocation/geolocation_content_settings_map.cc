// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the geolocation content settings map. Styled on
// HostContentSettingsMap however unlike that class, this one does not hold
// an additional in-memory copy of the settings as it does not need to support
// thread safe synchronous access to the settings; all geolocation permissions
// are read and written in the UI thread. (If in future this is no longer the
// case, refer to http://codereview.chromium.org/1525018 for a previous version
// with caching. Note that as we must observe the prefs store for settings
// changes, e.g. coming from the sync engine, the simplest design would be to
// always write-through changes straight to the prefs store, and rely on the
// notification observer to subsequently update any cached copy).

#include "chrome/browser/geolocation/geolocation_content_settings_map.h"

#include <string>

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "net/base/dns_util.h"
#include "net/base/static_cookie_policy.h"

// static
const ContentSetting
    GeolocationContentSettingsMap::kDefaultSetting = CONTENT_SETTING_ASK;

GeolocationContentSettingsMap::GeolocationContentSettingsMap(Profile* profile)
    : profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs_registrar_.Init(profile_->GetPrefs());
  prefs_registrar_.Add(prefs::kGeolocationDefaultContentSetting, this);
  prefs_registrar_.Add(prefs::kGeolocationContentSettings, this);
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

// static
void GeolocationContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kGeolocationDefaultContentSetting,
                             CONTENT_SETTING_ASK);
  prefs->RegisterDictionaryPref(prefs::kGeolocationContentSettings);
}

ContentSetting GeolocationContentSettingsMap::GetDefaultContentSetting() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If the profile is destroyed (and set to NULL) return CONTENT_SETTING_BLOCK.
  if (!profile_)
    return CONTENT_SETTING_BLOCK;
  const PrefService* prefs = profile_->GetPrefs();
  const ContentSetting default_content_setting = IntToContentSetting(
      prefs->GetInteger(prefs::kGeolocationDefaultContentSetting));
  return default_content_setting == CONTENT_SETTING_DEFAULT ?
         kDefaultSetting : default_content_setting;
}

bool GeolocationContentSettingsMap::IsDefaultContentSettingManaged() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If the profile is destroyed (and set to NULL) return true.
  if (!profile_)
    return true;
  return profile_->GetPrefs()->IsManagedPreference(
      prefs::kGeolocationDefaultContentSetting);
}

ContentSetting GeolocationContentSettingsMap::GetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(requesting_url.is_valid() && embedding_url.is_valid());
  GURL requesting_origin(requesting_url.GetOrigin());
  GURL embedding_origin(embedding_url.GetOrigin());
  DCHECK(requesting_origin.is_valid() && embedding_origin.is_valid());
  // If the profile is destroyed (and set to NULL) return CONTENT_SETTING_BLOCK.
  if (!profile_)
    return CONTENT_SETTING_BLOCK;
  const DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kGeolocationContentSettings);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    DictionaryValue* requesting_origin_settings;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
        requesting_origin.spec(), &requesting_origin_settings)) {
      int setting;
      if (requesting_origin_settings->GetIntegerWithoutPathExpansion(
          embedding_origin.spec(), &setting))
        return IntToContentSetting(setting);
      // Check for any-embedder setting
      if (requesting_origin != embedding_origin &&
          requesting_origin_settings->GetIntegerWithoutPathExpansion(
          "", &setting))
        return IntToContentSetting(setting);
    }
  }
  return GetDefaultContentSetting();
}

GeolocationContentSettingsMap::AllOriginsSettings
    GeolocationContentSettingsMap::GetAllOriginsSettings() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AllOriginsSettings content_settings;
  const DictionaryValue* all_settings_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kGeolocationContentSettings);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (all_settings_dictionary != NULL) {
    for (DictionaryValue::key_iterator i(all_settings_dictionary->begin_keys());
         i != all_settings_dictionary->end_keys(); ++i) {
      const std::string& origin(*i);
      GURL origin_as_url(origin);
      if (!origin_as_url.is_valid())
        continue;
      DictionaryValue* requesting_origin_settings_dictionary = NULL;
      bool found = all_settings_dictionary->GetDictionaryWithoutPathExpansion(
          origin, &requesting_origin_settings_dictionary);
      DCHECK(found);
      if (!requesting_origin_settings_dictionary)
        continue;
      GetOneOriginSettingsFromDictionary(
          requesting_origin_settings_dictionary,
          &content_settings[origin_as_url]);
    }
  }
  return content_settings;
}

void GeolocationContentSettingsMap::SetDefaultContentSetting(
    ContentSetting setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  profile_->GetPrefs()->SetInteger(prefs::kGeolocationDefaultContentSetting,
                                   setting == CONTENT_SETTING_DEFAULT ?
                                       kDefaultSetting : setting);
}

void GeolocationContentSettingsMap::SetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url,
    ContentSetting setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(requesting_url.is_valid());
  DCHECK(embedding_url.is_valid() || embedding_url.is_empty());
  GURL requesting_origin(requesting_url.GetOrigin());
  GURL embedding_origin(embedding_url.GetOrigin());
  DCHECK(requesting_origin.is_valid());
  DCHECK(embedding_origin.is_valid() || embedding_url.is_empty());
  if (!profile_)
    return;
  PrefService* prefs = profile_->GetPrefs();
  DictionaryValue* all_settings_dictionary = prefs->GetMutableDictionary(
      prefs::kGeolocationContentSettings);
  DCHECK(all_settings_dictionary);

  ScopedUserPrefUpdate update(prefs, prefs::kGeolocationContentSettings);
  DictionaryValue* requesting_origin_settings_dictionary = NULL;
  all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      requesting_origin.spec(), &requesting_origin_settings_dictionary);
  if (setting == CONTENT_SETTING_DEFAULT) {
    if (requesting_origin_settings_dictionary) {
      requesting_origin_settings_dictionary->RemoveWithoutPathExpansion(
          embedding_origin.spec(), NULL);
      if (requesting_origin_settings_dictionary->empty())
        all_settings_dictionary->RemoveWithoutPathExpansion(
            requesting_origin.spec(), NULL);
    }
  } else {
    if (!requesting_origin_settings_dictionary) {
      requesting_origin_settings_dictionary = new DictionaryValue;
      all_settings_dictionary->SetWithoutPathExpansion(
          requesting_origin.spec(), requesting_origin_settings_dictionary);
    }
    DCHECK(requesting_origin_settings_dictionary);
    requesting_origin_settings_dictionary->SetWithoutPathExpansion(
        embedding_origin.spec(), Value::CreateIntegerValue(setting));
  }
}

void GeolocationContentSettingsMap::ResetToDefault() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kGeolocationDefaultContentSetting);
  prefs->ClearPref(prefs::kGeolocationContentSettings);
}

void GeolocationContentSettingsMap::NotifyObservers(
    const ContentSettingsDetails& details) {
  NotificationService::current()->Notify(
      NotificationType::GEOLOCATION_SETTINGS_CHANGED,
      Source<GeolocationContentSettingsMap>(this),
      Details<const ContentSettingsDetails>(&details));
}

void GeolocationContentSettingsMap::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    const std::string& name = *Details<std::string>(details).ptr();
    if (name == prefs::kGeolocationDefaultContentSetting) {
      NotifyObservers(ContentSettingsDetails(
      ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_DEFAULT,
      ""));
    }
  } else if (NotificationType::PROFILE_DESTROYED == type) {
    UnregisterObservers();
  } else {
    NOTREACHED();
  }
}

void GeolocationContentSettingsMap::UnregisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  prefs_registrar_.RemoveAll();
  notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                 Source<Profile>(profile_));
  profile_ = NULL;
}

GeolocationContentSettingsMap::~GeolocationContentSettingsMap() {
  UnregisterObservers();
}

// static
void GeolocationContentSettingsMap::GetOneOriginSettingsFromDictionary(
    const DictionaryValue* dictionary,
    OneOriginSettings* one_origin_settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::string& target(*i);
    int setting = kDefaultSetting;
    bool found = dictionary->GetIntegerWithoutPathExpansion(target, &setting);
    DCHECK(found);
    GURL target_url(target);
    // An empty URL has a special meaning (wildcard), so only accept invalid
    // URLs if the original version was empty (avoids treating corrupted prefs
    // as the wildcard entry; see http://crbug.com/39685)
    if (target_url.is_valid() || target.empty())
      (*one_origin_settings)[target_url] = IntToContentSetting(setting);
  }
}
