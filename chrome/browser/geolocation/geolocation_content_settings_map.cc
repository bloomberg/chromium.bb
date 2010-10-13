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

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

// static
void GeolocationContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kGeolocationDefaultContentSetting,
                             CONTENT_SETTING_ASK);
  prefs->RegisterDictionaryPref(prefs::kGeolocationContentSettings);
}

ContentSetting GeolocationContentSettingsMap::GetDefaultContentSetting() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const PrefService* prefs = profile_->GetPrefs();
  const ContentSetting default_content_setting = IntToContentSetting(
      prefs->GetInteger(prefs::kGeolocationDefaultContentSetting));
  return default_content_setting == CONTENT_SETTING_DEFAULT ?
         kDefaultSetting : default_content_setting;
}

ContentSetting GeolocationContentSettingsMap::GetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(requesting_url.is_valid() && embedding_url.is_valid());
  GURL requesting_origin(requesting_url.GetOrigin());
  GURL embedding_origin(embedding_url.GetOrigin());
  DCHECK(requesting_origin.is_valid() && embedding_origin.is_valid());

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
  profile_->GetPrefs()->SetInteger(prefs::kGeolocationDefaultContentSetting,
                                   setting == CONTENT_SETTING_DEFAULT ?
                                       kDefaultSetting : setting);

  NotificationService::current()->Notify(
      NotificationType::GEOLOCATION_DEFAULT_CHANGED,
      Source<GeolocationContentSettingsMap>(this),
      NotificationService::NoDetails());
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
  PrefService* prefs = profile_->GetPrefs();
  DictionaryValue* all_settings_dictionary = prefs->GetMutableDictionary(
      prefs::kGeolocationContentSettings);
  DCHECK(all_settings_dictionary);

  ScopedPrefUpdate update(prefs, prefs::kGeolocationContentSettings);
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

  NotificationService::current()->Notify(
      NotificationType::GEOLOCATION_SETTINGS_CHANGED,
      Source<GeolocationContentSettingsMap>(this),
      NotificationService::NoDetails());
}

void GeolocationContentSettingsMap::ResetToDefault() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
