// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/pref_content_settings_provider.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/pref_names.h"

namespace {

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


// Map ASK for the plugins content type to BLOCK if click-to-play is
// not enabled.
ContentSetting ClickToPlayFixup(ContentSettingsType content_type,
                                ContentSetting setting) {
  if (setting == CONTENT_SETTING_ASK &&
      content_type == CONTENT_SETTINGS_TYPE_PLUGINS &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay)) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

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
      ClickToPlayFixup(CONTENT_SETTINGS_TYPE_PLUGINS,
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

}  // namespace content_settings
