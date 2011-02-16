// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_policy_provider.h"

#include <string>

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
      // TODO(markusheintz): Handle Geolocation and notification separately.
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

}  // namespace content_settings
