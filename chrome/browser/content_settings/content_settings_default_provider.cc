// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_default_provider.h"

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace {

// The default setting for each content type.
const ContentSetting kDefaultSettings[] = {
  CONTENT_SETTING_ALLOW,    // CONTENT_SETTINGS_TYPE_COOKIES
  CONTENT_SETTING_ALLOW,    // CONTENT_SETTINGS_TYPE_IMAGES
  CONTENT_SETTING_ALLOW,    // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  CONTENT_SETTING_ALLOW,    // CONTENT_SETTINGS_TYPE_PLUGINS
  CONTENT_SETTING_BLOCK,    // CONTENT_SETTINGS_TYPE_POPUPS
  CONTENT_SETTING_ASK,      // CONTENT_SETTINGS_TYPE_GEOLOCATION
  CONTENT_SETTING_ASK,      // CONTENT_SETTINGS_TYPE_NOTIFICATIONS
  CONTENT_SETTING_ASK,      // CONTENT_SETTINGS_TYPE_INTENTS
  CONTENT_SETTING_DEFAULT,  // CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE
  CONTENT_SETTING_ASK,      // CONTENT_SETTINGS_TYPE_FULLSCREEN
  CONTENT_SETTING_ASK,      // CONTENT_SETTINGS_TYPE_MOUSELOCK
};
COMPILE_ASSERT(arraysize(kDefaultSettings) == CONTENT_SETTINGS_NUM_TYPES,
               default_settings_incorrect_size);

}  // namespace

namespace content_settings {

namespace {

class DefaultRuleIterator : public RuleIterator {
 public:
  explicit DefaultRuleIterator(ContentSetting setting)
      : setting_(setting) {}

  bool HasNext() const {
    return (setting_ != CONTENT_SETTING_DEFAULT);
  }

  Rule Next() {
    DCHECK(setting_ != CONTENT_SETTING_DEFAULT);
    ContentSetting setting_to_return = setting_;
    setting_ = CONTENT_SETTING_DEFAULT;
    return Rule(ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::Wildcard(),
                Value::CreateIntegerValue(setting_to_return));
  }

 private:
  ContentSetting setting_;
};

}  // namespace

// static
void DefaultProvider::RegisterUserPrefs(PrefService* prefs) {
  // The registration of the preference prefs::kDefaultContentSettings should
  // also include the default values for default content settings. This allows
  // functional tests to get default content settings by reading the preference
  // prefs::kDefaultContentSettings via pyauto.
  // TODO(markusheintz): Write pyauto hooks for the content settings map as
  // content settings should be read from the host content settings map.
  DictionaryValue* default_content_settings = new DictionaryValue();
  prefs->RegisterDictionaryPref(prefs::kDefaultContentSettings,
                                default_content_settings,
                                PrefService::SYNCABLE_PREF);

  // Obsolete prefs, for migrations:
  prefs->RegisterIntegerPref(
      prefs::kDesktopNotificationDefaultContentSetting,
      kDefaultSettings[CONTENT_SETTINGS_TYPE_NOTIFICATIONS],
      PrefService::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(
      prefs::kGeolocationDefaultContentSetting,
      kDefaultSettings[CONTENT_SETTINGS_TYPE_GEOLOCATION],
      PrefService::UNSYNCABLE_PREF);
}

DefaultProvider::DefaultProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs),
      is_incognito_(incognito),
      updating_preferences_(false) {
  DCHECK(prefs_);
  MigrateObsoleteNotificationPref();
  MigrateObsoleteGeolocationPref();

  // Read global defaults.
  ReadDefaultSettings(true);
  if (default_content_settings_[CONTENT_SETTINGS_TYPE_COOKIES] ==
      CONTENT_SETTING_BLOCK) {
    UserMetrics::RecordAction(
        UserMetricsAction("CookieBlockingEnabledPerDefault"));
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("CookieBlockingDisabledPerDefault"));
  }

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(prefs::kDefaultContentSettings, this);
  pref_change_registrar_.Add(prefs::kGeolocationDefaultContentSetting, this);
}

DefaultProvider::~DefaultProvider() {
}

void DefaultProvider::SetContentSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    ContentSetting setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);

  // Ignore non default settings
  if (primary_pattern != ContentSettingsPattern::Wildcard() ||
      secondary_pattern != ContentSettingsPattern::Wildcard()) {
    return;
  }

  // The default settings may not be directly modified for OTR sessions.
  // Instead, they are synced to the main profile's setting.
  if (is_incognito_)
    return;

  std::string dictionary_path = GetTypeName(content_type);
  {
    AutoReset<bool> auto_reset(&updating_preferences_, true);
    DictionaryPrefUpdate update(prefs_, prefs::kDefaultContentSettings);
    DictionaryValue* default_settings_dictionary = update.Get();

    // |DefaultProvider| should not send any notifications when holding
    // |lock_|. |DictionaryPrefUpdate| destructor and
    // |PrefService::SetInteger()| send out notifications. As a response, the
    // upper layers may call |GetAllContentSettingRules| which acquires |lock_|
    // again.
    {
      base::AutoLock lock(lock_);
      if (setting == CONTENT_SETTING_DEFAULT ||
          setting == kDefaultSettings[content_type]) {
        default_content_settings_[content_type] =
            kDefaultSettings[content_type];
        default_settings_dictionary->RemoveWithoutPathExpansion(dictionary_path,
                                                                NULL);
      } else {
        default_content_settings_[content_type] = setting;
        default_settings_dictionary->SetWithoutPathExpansion(
            dictionary_path, Value::CreateIntegerValue(setting));
      }
    }

    // Keep the obsolete pref in sync as long as backwards compatibility is
    // required. This is required to keep sync working correctly.
    if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
       prefs_->SetInteger(prefs::kGeolocationDefaultContentSetting,
                          setting == CONTENT_SETTING_DEFAULT ?
                              kDefaultSettings[content_type] : setting);
    }
  }

  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  std::string());
}

RuleIterator* DefaultProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  base::AutoLock lock(lock_);
  if (resource_identifier.empty()) {
    return new DefaultRuleIterator(default_content_settings_[content_type]);
  } else {
    return new EmptyRuleIterator();
  }
}

void DefaultProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // TODO(markusheintz): This method is only called when the
  // |DesktopNotificationService| calls |ClearAllSettingsForType| method on the
  // |HostContentSettingsMap|. Don't implement this method yet, otherwise the
  // default notification settings will be cleared as well.
}

void DefaultProvider::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void DefaultProvider::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    DCHECK_EQ(prefs_, content::Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = content::Details<std::string>(details).ptr();
    if (*name == prefs::kDefaultContentSettings) {
      ReadDefaultSettings(true);
    } else if (*name == prefs::kGeolocationDefaultContentSetting) {
      MigrateObsoleteGeolocationPref();
      // Return and don't send a notifications. Migrating the obsolete
      // geolocation pref will change the prefs::kDefaultContentSettings and
      // cause the notification to be fired.
      return;
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    NotifyObservers(ContentSettingsPattern(),
                    ContentSettingsPattern(),
                    CONTENT_SETTINGS_TYPE_DEFAULT,
                    std::string());
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

void DefaultProvider::ReadDefaultSettings(bool overwrite) {
  base::AutoLock lock(lock_);
  const DictionaryValue* default_settings_dictionary =
      prefs_->GetDictionary(prefs::kDefaultContentSettings);

  if (overwrite) {
    for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      default_content_settings_[i] = CONTENT_SETTING_DEFAULT;
  }

  // Careful: The returned value could be NULL if the pref has never been set.
  if (default_settings_dictionary) {
    GetSettingsFromDictionary(default_settings_dictionary,
                              default_content_settings_);
  }
  ForceDefaultsToBeExplicit();
}

void DefaultProvider::ForceDefaultsToBeExplicit() {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (default_content_settings_[i] == CONTENT_SETTING_DEFAULT)
      default_content_settings_[i] = kDefaultSettings[i];
  }
}

void DefaultProvider::GetSettingsFromDictionary(
    const DictionaryValue* dictionary,
    ContentSetting* settings) {
  for (DictionaryValue::key_iterator i(dictionary->begin_keys());
       i != dictionary->end_keys(); ++i) {
    const std::string& content_type(*i);
    for (size_t type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
      if (content_type == GetTypeName(ContentSettingsType(type))) {
        int setting = CONTENT_SETTING_DEFAULT;
        bool found = dictionary->GetIntegerWithoutPathExpansion(content_type,
                                                                &setting);
        DCHECK(found);
        settings[type] = IntToContentSetting(setting);
        break;
      }
    }
  }
  // Migrate obsolete cookie prompt mode/
  if (settings[CONTENT_SETTINGS_TYPE_COOKIES] == CONTENT_SETTING_ASK)
    settings[CONTENT_SETTINGS_TYPE_COOKIES] = CONTENT_SETTING_BLOCK;

  settings[CONTENT_SETTINGS_TYPE_PLUGINS] =
      ClickToPlayFixup(CONTENT_SETTINGS_TYPE_PLUGINS,
                       settings[CONTENT_SETTINGS_TYPE_PLUGINS]);
}

void DefaultProvider::MigrateObsoleteNotificationPref() {
  if (prefs_->HasPrefPath(prefs::kDesktopNotificationDefaultContentSetting)) {
    ContentSetting setting = IntToContentSetting(
        prefs_->GetInteger(prefs::kDesktopNotificationDefaultContentSetting));
    SetContentSetting(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        std::string(),
        setting);
    prefs_->ClearPref(prefs::kDesktopNotificationDefaultContentSetting);
  }
}

void DefaultProvider::MigrateObsoleteGeolocationPref() {
  if (prefs_->HasPrefPath(prefs::kGeolocationDefaultContentSetting)) {
    ContentSetting setting = IntToContentSetting(
        prefs_->GetInteger(prefs::kGeolocationDefaultContentSetting));
    // Do not clear the old preference yet as long as we need to maintain
    // backward compatibility.
    SetContentSetting(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_GEOLOCATION,
        std::string(),
        setting);
  }
}

}  // namespace content_settings
