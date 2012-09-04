// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_policy_provider.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace {

// The preferences used to manage ContentSettingsTypes.
const char* kPrefToManageType[] = {
  prefs::kManagedDefaultCookiesSetting,
  prefs::kManagedDefaultImagesSetting,
  prefs::kManagedDefaultJavaScriptSetting,
  prefs::kManagedDefaultPluginsSetting,
  prefs::kManagedDefaultPopupsSetting,
  prefs::kManagedDefaultGeolocationSetting,
  prefs::kManagedDefaultNotificationsSetting,
  NULL,  // No policy for default value of content type intents
  NULL,  // No policy for default value of content type auto-select-certificate
  NULL,  // No policy for default value of fullscreen requests
  NULL,  // No policy for default value of mouse lock requests
  NULL,  // No policy for default value of mixed script blocking
  prefs::kManagedDefaultMediaStreamSetting,
  NULL,  // No policy for default value of protocol handlers
  NULL,  // No policy for default value of PPAPI broker
};
COMPILE_ASSERT(arraysize(kPrefToManageType) == CONTENT_SETTINGS_NUM_TYPES,
               managed_content_settings_pref_names_array_size_incorrect);

struct PrefsForManagedContentSettingsMapEntry {
  const char* pref_name;
  ContentSettingsType content_type;
  ContentSetting setting;
};

const PrefsForManagedContentSettingsMapEntry
    kPrefsForManagedContentSettingsMap[] = {
  {
    prefs::kManagedCookiesAllowedForUrls,
    CONTENT_SETTINGS_TYPE_COOKIES,
    CONTENT_SETTING_ALLOW
  }, {
    prefs::kManagedCookiesSessionOnlyForUrls,
    CONTENT_SETTINGS_TYPE_COOKIES,
    CONTENT_SETTING_SESSION_ONLY
  }, {
    prefs::kManagedCookiesBlockedForUrls,
    CONTENT_SETTINGS_TYPE_COOKIES,
    CONTENT_SETTING_BLOCK
  }, {
    prefs::kManagedImagesAllowedForUrls,
    CONTENT_SETTINGS_TYPE_IMAGES,
    CONTENT_SETTING_ALLOW
  }, {
    prefs::kManagedImagesBlockedForUrls,
    CONTENT_SETTINGS_TYPE_IMAGES,
    CONTENT_SETTING_BLOCK
  }, {
    prefs::kManagedJavaScriptAllowedForUrls,
    CONTENT_SETTINGS_TYPE_JAVASCRIPT,
    CONTENT_SETTING_ALLOW
  }, {
    prefs::kManagedJavaScriptBlockedForUrls,
    CONTENT_SETTINGS_TYPE_JAVASCRIPT,
    CONTENT_SETTING_BLOCK
  }, {
    prefs::kManagedPluginsAllowedForUrls,
    CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTING_ALLOW
  }, {
    prefs::kManagedPluginsBlockedForUrls,
    CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTING_BLOCK
  }, {
    prefs::kManagedPopupsAllowedForUrls,
    CONTENT_SETTINGS_TYPE_POPUPS,
    CONTENT_SETTING_ALLOW
  }, {
    prefs::kManagedPopupsBlockedForUrls,
    CONTENT_SETTINGS_TYPE_POPUPS,
    CONTENT_SETTING_BLOCK
  }, {
    prefs::kManagedNotificationsAllowedForUrls,
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
    CONTENT_SETTING_ALLOW
  }, {
    prefs::kManagedNotificationsBlockedForUrls,
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
    CONTENT_SETTING_BLOCK
  }
};

}  // namespace

namespace content_settings {

// static
void PolicyProvider::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kManagedAutoSelectCertificateForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedCookiesAllowedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedCookiesBlockedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedCookiesSessionOnlyForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedImagesAllowedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedImagesBlockedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedJavaScriptAllowedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedJavaScriptBlockedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedPluginsAllowedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedPluginsBlockedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedPopupsAllowedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedPopupsBlockedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedNotificationsAllowedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedNotificationsBlockedForUrls,
                          PrefService::UNSYNCABLE_PREF);
  // Preferences for default content setting policies. If a policy is not set of
  // the corresponding preferences below is set to CONTENT_SETTING_DEFAULT.
  prefs->RegisterIntegerPref(prefs::kManagedDefaultCookiesSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultImagesSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultJavaScriptSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultPluginsSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultPopupsSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultGeolocationSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultNotificationsSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultMediaStreamSetting,
                             CONTENT_SETTING_DEFAULT,
                             PrefService::UNSYNCABLE_PREF);
}

PolicyProvider::PolicyProvider(PrefService* prefs) : prefs_(prefs) {
  ReadManagedDefaultSettings();
  ReadManagedContentSettings(false);

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(prefs::kManagedAutoSelectCertificateForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedCookiesBlockedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedCookiesAllowedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedCookiesSessionOnlyForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedImagesBlockedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedImagesAllowedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedJavaScriptBlockedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedJavaScriptAllowedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedPluginsBlockedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedPluginsAllowedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedPopupsBlockedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedPopupsAllowedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedNotificationsAllowedForUrls, this);
  pref_change_registrar_.Add(prefs::kManagedNotificationsBlockedForUrls, this);
  // The following preferences are only used to indicate if a
  // default content setting is managed and to hold the managed default setting
  // value. If the value for any of the following perferences is set then the
  // corresponding default content setting is managed. These preferences exist
  // in parallel to the preference default content settings.  If a
  // default content settings type is managed any user defined excpetions
  // (patterns) for this type are ignored.
  pref_change_registrar_.Add(prefs::kManagedDefaultCookiesSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultImagesSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultJavaScriptSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultPluginsSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultPopupsSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultGeolocationSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultNotificationsSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultMediaStreamSetting, this);
}

PolicyProvider::~PolicyProvider() {
  DCHECK(!prefs_);
}

RuleIterator* PolicyProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return value_map_.GetRuleIterator(content_type, resource_identifier, &lock_);
}

void PolicyProvider::GetContentSettingsFromPreferences(
    OriginIdentifierValueMap* value_map) {
  for (size_t i = 0; i < arraysize(kPrefsForManagedContentSettingsMap); ++i) {
    const char* pref_name = kPrefsForManagedContentSettingsMap[i].pref_name;
    // Skip unset policies.
    if (!prefs_->HasPrefPath(pref_name)) {
      VLOG(2) << "Skipping unset preference: " << pref_name;
      continue;
    }

    const PrefService::Preference* pref = prefs_->FindPreference(pref_name);
    DCHECK(pref);
    DCHECK(pref->IsManaged());

    const base::ListValue* pattern_str_list = NULL;
    if (!pref->GetValue()->GetAsList(&pattern_str_list)) {
      NOTREACHED();
      return;
    }

    for (size_t j = 0; j < pattern_str_list->GetSize(); ++j) {
      std::string original_pattern_str;
      if (!pattern_str_list->GetString(j, &original_pattern_str)) {
        NOTREACHED();
        continue;
      }

      PatternPair pattern_pair = ParsePatternString(original_pattern_str);
      // Ignore invalid patterns.
      if (!pattern_pair.first.IsValid()) {
        VLOG(1) << "Ignoring invalid content settings pattern: " <<
                   original_pattern_str;
        continue;
      }

      ContentSettingsType content_type =
          kPrefsForManagedContentSettingsMap[i].content_type;
      DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
      // If only one pattern was defined auto expand it to a pattern pair.
      ContentSettingsPattern secondary_pattern =
          !pattern_pair.second.IsValid() ? ContentSettingsPattern::Wildcard()
                                         : pattern_pair.second;
      value_map->SetValue(
          pattern_pair.first,
          secondary_pattern,
          content_type,
          ResourceIdentifier(NO_RESOURCE_IDENTIFIER),
          base::Value::CreateIntegerValue(
              kPrefsForManagedContentSettingsMap[i].setting));
    }
  }
}

void PolicyProvider::GetAutoSelectCertificateSettingsFromPreferences(
    OriginIdentifierValueMap* value_map) {
  const char* pref_name = prefs::kManagedAutoSelectCertificateForUrls;

  if (!prefs_->HasPrefPath(pref_name)) {
    VLOG(2) << "Skipping unset preference: " << pref_name;
    return;
  }

  const PrefService::Preference* pref = prefs_->FindPreference(pref_name);
  DCHECK(pref);
  DCHECK(pref->IsManaged());

  const base::ListValue* pattern_filter_str_list = NULL;
  if (!pref->GetValue()->GetAsList(&pattern_filter_str_list)) {
    NOTREACHED();
    return;
  }

  // Parse the list of pattern filter strings. A pattern filter string has
  // the following JSON format:
  //
  // {
  //   "pattern": <content settings pattern string>,
  //   "filter" : <certificate filter in JSON format>
  // }
  //
  // e.g.
  // {
  //   "pattern": "[*.]example.com",
  //   "filter": {
  //      "ISSUER": {
  //        "CN": "some name"
  //      }
  //   }
  // }
  for (size_t j = 0; j < pattern_filter_str_list->GetSize(); ++j) {
    std::string pattern_filter_json;
    if (!pattern_filter_str_list->GetString(j, &pattern_filter_json)) {
      NOTREACHED();
      continue;
    }

    scoped_ptr<base::Value> value(base::JSONReader::Read(pattern_filter_json,
        base::JSON_ALLOW_TRAILING_COMMAS));
    if (!value.get()) {
      VLOG(1) << "Ignoring invalid certificate auto select setting. Reason:"
                 " Invalid JSON format: " << pattern_filter_json;
      continue;
    }

    scoped_ptr<base::DictionaryValue> pattern_filter_pair(
        static_cast<base::DictionaryValue*>(value.release()));
    std::string pattern_str;
    bool pattern_read = pattern_filter_pair->GetString("pattern", &pattern_str);
    base::Value* cert_filter_ptr = NULL;
    bool filter_read = pattern_filter_pair->Remove("filter", &cert_filter_ptr);
    scoped_ptr<base::Value> cert_filter(cert_filter_ptr);
    if (!pattern_read || !filter_read) {
      VLOG(1) << "Ignoring invalid certificate auto select setting. Reason:"
                 " Missing pattern or filter.";
      continue;
    }

    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(pattern_str);
    // Ignore invalid patterns.
    if (!pattern.IsValid()) {
      VLOG(1) << "Ignoring invalid certificate auto select setting:"
                 " Invalid content settings pattern: " << pattern;
      continue;
    }

    DCHECK(cert_filter->IsType(base::Value::TYPE_DICTIONARY));
    value_map->SetValue(pattern,
                        ContentSettingsPattern::Wildcard(),
                        CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                        std::string(),
                        cert_filter.release());
  }
}

void PolicyProvider::ReadManagedDefaultSettings() {
  for (size_t type = 0; type < arraysize(kPrefToManageType); ++type) {
    if (kPrefToManageType[type] == NULL) {
      continue;
    }
    UpdateManagedDefaultSetting(ContentSettingsType(type));
  }
}

void PolicyProvider::UpdateManagedDefaultSetting(
    ContentSettingsType content_type) {
  // If a pref to manage a default-content-setting was not set (NOTICE:
  // "HasPrefPath" returns false if no value was set for a registered pref) then
  // the default value of the preference is used. The default value of a
  // preference to manage a default-content-settings is CONTENT_SETTING_DEFAULT.
  // This indicates that no managed value is set. If a pref was set, than it
  // MUST be managed.
  DCHECK(!prefs_->HasPrefPath(kPrefToManageType[content_type]) ||
          prefs_->IsManagedPreference(kPrefToManageType[content_type]));
  base::AutoLock auto_lock(lock_);

  int setting = prefs_->GetInteger(kPrefToManageType[content_type]);
  if (setting == CONTENT_SETTING_DEFAULT) {
    value_map_.DeleteValue(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::Wildcard(),
        content_type,
        std::string());
  } else {
    value_map_.SetValue(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::Wildcard(),
        content_type,
        std::string(),
        Value::CreateIntegerValue(setting));
  }
}


void PolicyProvider::ReadManagedContentSettings(bool overwrite) {
  base::AutoLock auto_lock(lock_);
  if (overwrite)
    value_map_.clear();
  GetContentSettingsFromPreferences(&value_map_);
  GetAutoSelectCertificateSettingsFromPreferences(&value_map_);
}

// Since the PolicyProvider is a read only content settings provider, all
// methodes of the ProviderInterface that set or delete any settings do nothing.
bool PolicyProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Value* value) {
  return false;
}

void PolicyProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
}

void PolicyProvider::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RemoveAllObservers();
  if (!prefs_)
    return;
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void PolicyProvider::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    DCHECK_EQ(prefs_, content::Source<PrefService>(source).ptr());
    std::string* name = content::Details<std::string>(details).ptr();
    if (*name == prefs::kManagedDefaultCookiesSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES);
    } else if (*name == prefs::kManagedDefaultImagesSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_IMAGES);
    } else if (*name == prefs::kManagedDefaultJavaScriptSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
    } else if (*name == prefs::kManagedDefaultPluginsSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_PLUGINS);
    } else if (*name == prefs::kManagedDefaultPopupsSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_POPUPS);
    } else if (*name == prefs::kManagedDefaultGeolocationSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION);
    } else if (*name == prefs::kManagedDefaultNotificationsSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    } else if (*name == prefs::kManagedDefaultMediaStreamSetting) {
      UpdateManagedDefaultSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM);
    } else if (*name == prefs::kManagedAutoSelectCertificateForUrls ||
        *name == prefs::kManagedCookiesAllowedForUrls ||
        *name == prefs::kManagedCookiesBlockedForUrls ||
        *name == prefs::kManagedCookiesSessionOnlyForUrls ||
        *name == prefs::kManagedImagesAllowedForUrls ||
        *name == prefs::kManagedImagesBlockedForUrls ||
        *name == prefs::kManagedJavaScriptAllowedForUrls ||
        *name == prefs::kManagedJavaScriptBlockedForUrls ||
        *name == prefs::kManagedPluginsAllowedForUrls ||
        *name == prefs::kManagedPluginsBlockedForUrls ||
        *name == prefs::kManagedPopupsAllowedForUrls ||
        *name == prefs::kManagedPopupsBlockedForUrls ||
        *name == prefs::kManagedNotificationsAllowedForUrls ||
        *name == prefs::kManagedNotificationsBlockedForUrls) {
      ReadManagedContentSettings(true);
      ReadManagedDefaultSettings();
    }
  } else {
    NOTREACHED() << "Unexpected notification";
    return;
  }
  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  CONTENT_SETTINGS_TYPE_DEFAULT,
                  std::string());
}

}  // namespace content_settings
