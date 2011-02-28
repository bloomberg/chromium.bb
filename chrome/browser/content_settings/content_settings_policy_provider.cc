// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_policy_provider.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/pref_names.h"

#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

// Base pref path of the prefs that contain the managed default content
// settings values.
const std::string kManagedSettings =
      "profile.managed_default_content_settings";

// The preferences used to manage ContentSettingsTypes.
const char* kPrefToManageType[CONTENT_SETTINGS_NUM_TYPES] = {
  prefs::kManagedDefaultCookiesSetting,
  prefs::kManagedDefaultImagesSetting,
  prefs::kManagedDefaultJavaScriptSetting,
  prefs::kManagedDefaultPluginsSetting,
  prefs::kManagedDefaultPopupsSetting,
  NULL,  // Not used for Geolocation
  NULL,  // Not used for Notifications
};

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
  }
};

const std::string NO_IDENTIFIER = "";

}  // namespace

namespace content_settings {

PolicyDefaultProvider::PolicyDefaultProvider(Profile* profile)
    : profile_(profile),
      is_off_the_record_(profile_->IsOffTheRecord()) {
  PrefService* prefs = profile->GetPrefs();

  // Read global defaults.
  DCHECK_EQ(arraysize(kPrefToManageType),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  ReadManagedDefaultSettings();

  pref_change_registrar_.Init(prefs);
  // The following preferences are only used to indicate if a
  // default-content-setting is managed and to hold the managed default-setting
  // value. If the value for any of the following perferences is set then the
  // corresponding default-content-setting is managed. These preferences exist
  // in parallel to the preference default-content-settings.  If a
  // default-content-settings-type is managed any user defined excpetions
  // (patterns) for this type are ignored.
  pref_change_registrar_.Add(prefs::kManagedDefaultCookiesSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultImagesSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultJavaScriptSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultPluginsSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultPopupsSetting, this);
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

PolicyDefaultProvider::~PolicyDefaultProvider() {
  UnregisterObservers();
}

ContentSetting PolicyDefaultProvider::ProvideDefaultSetting(
    ContentSettingsType content_type) const {
  base::AutoLock auto_lock(lock_);
  return managed_default_content_settings_.settings[content_type];
}

void PolicyDefaultProvider::UpdateDefaultSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
}

bool PolicyDefaultProvider::DefaultSettingIsManaged(
    ContentSettingsType content_type) const {
  base::AutoLock lock(lock_);
  if (managed_default_content_settings_.settings[content_type] !=
      CONTENT_SETTING_DEFAULT) {
    return true;
  } else {
    return false;
  }
}

void PolicyDefaultProvider::ResetToDefaults() {
}

void PolicyDefaultProvider::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == NotificationType::PREF_CHANGED) {
    DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
    std::string* name = Details<std::string>(details).ptr();
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

void PolicyDefaultProvider::UnregisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  pref_change_registrar_.RemoveAll();
  notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                 Source<Profile>(profile_));
  profile_ = NULL;
}


void PolicyDefaultProvider::NotifyObservers(
    const ContentSettingsDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_ == NULL)
    return;
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(profile_->GetHostContentSettingsMap()),
      Details<const ContentSettingsDetails>(&details));
}

void PolicyDefaultProvider::ReadManagedDefaultSettings() {
  for (size_t type = 0; type < arraysize(kPrefToManageType); ++type) {
    if (kPrefToManageType[type] == NULL) {
      continue;
    }
    UpdateManagedDefaultSetting(ContentSettingsType(type));
  }
}

void PolicyDefaultProvider::UpdateManagedDefaultSetting(
    ContentSettingsType type) {
  // If a pref to manage a default-content-setting was not set (NOTICE:
  // "HasPrefPath" returns false if no value was set for a registered pref) then
  // the default value of the preference is used. The default value of a
  // preference to manage a default-content-settings is CONTENT_SETTING_DEFAULT.
  // This indicates that no managed value is set. If a pref was set, than it
  // MUST be managed.
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(!prefs->HasPrefPath(kPrefToManageType[type]) ||
          prefs->IsManagedPreference(kPrefToManageType[type]));
  base::AutoLock auto_lock(lock_);
  managed_default_content_settings_.settings[type] = IntToContentSetting(
      prefs->GetInteger(kPrefToManageType[type]));
}

// static
void PolicyDefaultProvider::RegisterUserPrefs(PrefService* prefs) {
  // Preferences for default content setting policies. A policy is not set of
  // the corresponding preferences below is set to CONTENT_SETTING_DEFAULT.
  prefs->RegisterIntegerPref(prefs::kManagedDefaultCookiesSetting,
      CONTENT_SETTING_DEFAULT);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultImagesSetting,
      CONTENT_SETTING_DEFAULT);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultJavaScriptSetting,
      CONTENT_SETTING_DEFAULT);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultPluginsSetting,
      CONTENT_SETTING_DEFAULT);
  prefs->RegisterIntegerPref(prefs::kManagedDefaultPopupsSetting,
      CONTENT_SETTING_DEFAULT);
}

// ////////////////////////////////////////////////////////////////////////////
// PolicyProvider

// static
void PolicyProvider::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kManagedCookiesAllowedForUrls);
  prefs->RegisterListPref(prefs::kManagedCookiesBlockedForUrls);
  prefs->RegisterListPref(prefs::kManagedCookiesSessionOnlyForUrls);
  prefs->RegisterListPref(prefs::kManagedImagesAllowedForUrls);
  prefs->RegisterListPref(prefs::kManagedImagesBlockedForUrls);
  prefs->RegisterListPref(prefs::kManagedJavaScriptAllowedForUrls);
  prefs->RegisterListPref(prefs::kManagedJavaScriptBlockedForUrls);
  prefs->RegisterListPref(prefs::kManagedPluginsAllowedForUrls);
  prefs->RegisterListPref(prefs::kManagedPluginsBlockedForUrls);
  prefs->RegisterListPref(prefs::kManagedPopupsAllowedForUrls);
  prefs->RegisterListPref(prefs::kManagedPopupsBlockedForUrls);
}

PolicyProvider::PolicyProvider(Profile* profile)
    : BaseProvider(profile->IsOffTheRecord()),
      profile_(profile) {
  Init();
}

PolicyProvider::~PolicyProvider() {
  UnregisterObservers();
}

void PolicyProvider::ReadManagedContentSettingsTypes(
    ContentSettingsType content_type) {
  PrefService* prefs = profile_->GetPrefs();
  if (kPrefToManageType[content_type] == NULL) {
    content_type_is_managed_[content_type] = false;
  } else {
    content_type_is_managed_[content_type] =
         prefs->IsManagedPreference(kPrefToManageType[content_type]);
  }
}

void PolicyProvider::Init() {
  PrefService* prefs = profile_->GetPrefs();

  ReadManagedContentSettings(false);
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    ReadManagedContentSettingsTypes(ContentSettingsType(i));

  pref_change_registrar_.Init(prefs);
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

  pref_change_registrar_.Add(prefs::kManagedDefaultCookiesSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultImagesSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultJavaScriptSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultPluginsSetting, this);
  pref_change_registrar_.Add(prefs::kManagedDefaultPopupsSetting, this);

  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

bool PolicyProvider::ContentSettingsTypeIsManaged(
    ContentSettingsType content_type) {
  return content_type_is_managed_[content_type];
}

void PolicyProvider::GetContentSettingsFromPreferences(
    PrefService* prefs,
    ContentSettingsRules* rules) {
  for (size_t i = 0; i < arraysize(kPrefsForManagedContentSettingsMap); ++i) {
    const char* pref_name = kPrefsForManagedContentSettingsMap[i].pref_name;
    // Skip unset policies.
    if (!prefs->HasPrefPath(pref_name)) {
      LOG(INFO) << "Skiping unset preference: " << pref_name;
      continue;
    }

    const PrefService::Preference* pref = prefs->FindPreference(pref_name);
    DCHECK(pref->IsManaged());
    DCHECK_EQ(Value::TYPE_LIST, pref->GetType());

    const ListValue* pattern_str_list =
        static_cast<const ListValue*>(pref->GetValue());
    for (size_t j = 0; j < pattern_str_list->GetSize(); ++j) {
      std::string original_pattern_str;
      pattern_str_list->GetString(j, &original_pattern_str);
      ContentSettingsPattern pattern(original_pattern_str);
      if (!pattern.IsValid()) {
        // Ignore invalid patterns
        continue;
        LOG(WARNING) << "Ignoring invalid content settings pattern: "
                     << pattern.AsString();
      }
      rules->push_back(MakeTuple(
          pattern,
          pattern,
          kPrefsForManagedContentSettingsMap[i].content_type,
          NO_IDENTIFIER,
          kPrefsForManagedContentSettingsMap[i].setting));
    }
  }
}

void PolicyProvider::ReadManagedContentSettings(bool overwrite) {
  ContentSettingsRules rules;
  PrefService* prefs = profile_->GetPrefs();
  GetContentSettingsFromPreferences(prefs, &rules);
  {
    base::AutoLock auto_lock(lock());
    HostContentSettings* content_settings_map = host_content_settings();
    if (overwrite)
      content_settings_map->clear();
    for (ContentSettingsRules::iterator rule = rules.begin();
         rule != rules.end();
         ++rule) {
      DispatchToMethod(this, &PolicyProvider::UpdateContentSettingsMap, *rule);
    }
  }
}

// Since the PolicyProvider is a read only content settings provider, all
// methodes of the ProviderInterface that set or delete any settings do nothing.
void PolicyProvider::SetContentSetting(
    const ContentSettingsPattern& requesting_pattern,
    const ContentSettingsPattern& embedding_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    ContentSetting content_setting) {
}

ContentSetting PolicyProvider::GetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  return BaseProvider::GetContentSetting(
      requesting_url,
      embedding_url,
      content_type,
      NO_IDENTIFIER);
}

void PolicyProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
}

void PolicyProvider::ResetToDefaults() {
}

void PolicyProvider::UnregisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  pref_change_registrar_.RemoveAll();
  notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                 Source<Profile>(profile_));
  profile_ = NULL;
}

void PolicyProvider::NotifyObservers(
    const ContentSettingsDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_ == NULL)
    return;
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(profile_->GetHostContentSettingsMap()),
      Details<const ContentSettingsDetails>(&details));
}

void PolicyProvider::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == NotificationType::PREF_CHANGED) {
    DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kManagedCookiesAllowedForUrls ||
        *name == prefs::kManagedCookiesBlockedForUrls ||
        *name == prefs::kManagedCookiesSessionOnlyForUrls ||
        *name == prefs::kManagedImagesAllowedForUrls ||
        *name == prefs::kManagedImagesBlockedForUrls ||
        *name == prefs::kManagedJavaScriptAllowedForUrls ||
        *name == prefs::kManagedJavaScriptBlockedForUrls ||
        *name == prefs::kManagedPluginsAllowedForUrls ||
        *name == prefs::kManagedPluginsBlockedForUrls ||
        *name == prefs::kManagedPopupsAllowedForUrls ||
        *name == prefs::kManagedPopupsBlockedForUrls) {
      ReadManagedContentSettings(true);
      NotifyObservers(ContentSettingsDetails(
          ContentSettingsPattern(), CONTENT_SETTINGS_TYPE_DEFAULT, ""));
      // We do not want to sent a notification when managed default content
      // settings change. The DefaultProvider will take care of that. We are
      // only a passive observer.
      // TODO(markusheintz): NOTICE: This is still work in progress and part of
      // a larger refactoring. The code will change and be much cleaner and
      // clearer in the end.
    } else if (*name == prefs::kManagedDefaultCookiesSetting) {
      ReadManagedContentSettingsTypes(CONTENT_SETTINGS_TYPE_COOKIES);
    } else if (*name == prefs::kManagedDefaultImagesSetting) {
      ReadManagedContentSettingsTypes(CONTENT_SETTINGS_TYPE_IMAGES);
    } else if (*name == prefs::kManagedDefaultJavaScriptSetting) {
      ReadManagedContentSettingsTypes(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
    } else if (*name == prefs::kManagedDefaultPluginsSetting) {
      ReadManagedContentSettingsTypes(CONTENT_SETTINGS_TYPE_PLUGINS);
    } else if (*name == prefs::kManagedDefaultPopupsSetting) {
      ReadManagedContentSettingsTypes(CONTENT_SETTINGS_TYPE_POPUPS);
    }
  } else if (type == NotificationType::PROFILE_DESTROYED) {
    DCHECK_EQ(profile_, Source<Profile>(source).ptr());
    UnregisterObservers();
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

}  // namespace content_settings
