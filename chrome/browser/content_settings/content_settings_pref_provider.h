// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PREF_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PREF_PROVIDER_H_
#pragma once

// A content settings provider that takes its settings out of the pref service.

#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ContentSettingsDetails;
class HostContentSettingsMap;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace content_settings {

// Content settings provider that provides default content settings based on
// user prefs.
class PrefDefaultProvider : public ObservableDefaultProvider,
                            public NotificationObserver {
 public:
  PrefDefaultProvider(PrefService* prefs,
                      bool incognito);
  virtual ~PrefDefaultProvider();

  // DefaultContentSettingsProvider implementation.
  virtual ContentSetting ProvideDefaultSetting(
      ContentSettingsType content_type) const;
  virtual void UpdateDefaultSetting(ContentSettingsType content_type,
                                    ContentSetting setting);
  virtual bool DefaultSettingIsManaged(ContentSettingsType content_type) const;

  virtual void ShutdownOnUIThread();

  static void RegisterUserPrefs(PrefService* prefs);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Sets the fields of |settings| based on the values in |dictionary|.
  void GetSettingsFromDictionary(const base::DictionaryValue* dictionary,
                                 ContentSettings* settings);

  // Forces the default settings to be explicitly set instead of themselves
  // being CONTENT_SETTING_DEFAULT.
  void ForceDefaultsToBeExplicit();

  // Reads the default settings from the preferences service. If |overwrite| is
  // true and the preference is missing, the local copy will be cleared as well.
  void ReadDefaultSettings(bool overwrite);

  void MigrateObsoleteNotificationPref();
  void MigrateObsoleteGeolocationPref();

  // Copies of the pref data, so that we can read it on the IO thread.
  ContentSettings default_content_settings_;

  PrefService* prefs_;

  // Whether this settings map is for an Incognito session.
  bool is_incognito_;

  // Used around accesses to the default_content_settings_ object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  PrefChangeRegistrar pref_change_registrar_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  DISALLOW_COPY_AND_ASSIGN(PrefDefaultProvider);
};

// Content settings provider that provides content settings from the user
// preference.
class PrefProvider : public ObservableProvider,
                     public NotificationObserver {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

  PrefProvider(PrefService* prefs,
               bool incognito);
  virtual ~PrefProvider();

  // ProviderInterface implementations.
  virtual void SetContentSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting);

  virtual ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type);

  virtual void ShutdownOnUIThread();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Reads all content settings exceptions from the preference and load them
  // into the |value_map_|. The |value_map_| is cleared first if |overwrite| is
  // true.
  void ReadContentSettingsFromPref(bool overwrite);

  // Update the preference that stores content settings exceptions and syncs the
  // value to the obsolete preference.
  void UpdatePref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting setting);

  // Update the preference prefs::kContentSettingsPatternPairs, which is used to
  // persist content settings exceptions and supposed to replace the preferences
  // prefs::kContentSettingsPatterns and prefs::kGeolocationContentSettings.
  void UpdatePatternPairsPref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting setting);

  // Updates the preferences prefs::kContentSettingsPatterns. This preferences
  // is obsolete and only used for compatibility reasons.
  void UpdateObsoletePatternsPref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting setting);

  // Updates the preferences prefs::kGeolocationContentSettings. This preference
  // is obsolete and only used to keep sync working with older chrome versions
  // that do not know about the new preference.
  void UpdateObsoleteGeolocationPref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSetting setting);

  // Various migration methods (old cookie, popup and per-host data gets
  // migrated to the new format).
  void MigrateObsoletePerhostPref();
  void MigrateObsoletePopupsPref();
  void MigrateObsoleteContentSettingsPatternPref();
  void MigrateObsoleteGeolocationPref();

  // Copies the value of the preference that stores the content settings
  // exceptions to the obsolete preference for content settings exceptions. This
  // is necessary to allow content settings exceptions beeing synced to older
  // versions of chrome that only use the obsolete preference.
  void SyncObsoletePatternPref();

  // Copies the geolocation content settings exceptions from the preference that
  // stores the content settings exceptions to the obsolete preference for
  // geolocation content settings exceptions. This is necessary to allow
  // geolocation content settings exceptions being synced to older versions of
  // chrome that only use the obsolete preference.
  void SyncObsoleteGeolocationPref();

  static void CanonicalizeContentSettingsExceptions(
      base::DictionaryValue* all_settings_dictionary);

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  PrefService* prefs_;

  bool is_incognito_;

  PrefChangeRegistrar pref_change_registrar_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  OriginIdentifierValueMap value_map_;

  OriginIdentifierValueMap incognito_value_map_;

  // Used around accesses to the value map objects to guarantee
  // thread safety.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PrefProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PREF_PROVIDER_H_
