// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PREF_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PREF_PROVIDER_H_
#pragma once

// A content settings provider that takes its settings out of the pref service.

#include <vector>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;

namespace base {
class DictionaryValue;
}

namespace content_settings {

// Content settings provider that provides content settings from the user
// preference.
class PrefProvider : public ObservableProvider,
                     public content::NotificationObserver {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

  PrefProvider(PrefService* prefs,
               bool incognito);
  virtual ~PrefProvider();

  // ProviderInterface implementations.
  virtual RuleIterator* GetRuleIterator(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      bool incognito) const OVERRIDE;

  virtual bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Value* value) OVERRIDE;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) OVERRIDE;

  virtual void ShutdownOnUIThread() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class DeadlockCheckerThread;  // For testing.
  // Reads all content settings exceptions from the preference and load them
  // into the |value_map_|. The |value_map_| is cleared first if |overwrite| is
  // true.
  void ReadContentSettingsFromPref(bool overwrite);

  // Update the preference that stores content settings exceptions and syncs the
  // value to the obsolete preference. When calling this function, |lock_|
  // should not be held, since this function will send out notifications of
  // preference changes.
  void UpdatePref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      const base::Value* value);

  // Updates the given |pattern_pairs_settings| dictionary value.
  void UpdatePatternPairsSettings(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      const base::Value* value,
      DictionaryValue* pattern_pairs_settings);

  // Updates the preferences prefs::kContentSettingsPatterns. This preferences
  // is obsolete and only used for compatibility reasons.
  void UpdateObsoletePatternsPref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting setting);

  // Updates the preference prefs::kGeolocationContentSettings. This preference
  // is obsolete and only used to keep sync working with older chrome versions
  // that do not know about the new preference.
  void UpdateObsoleteGeolocationPref(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSetting setting);

  // Updates the obsolete notifications settings in the passed list values
  // |allowed_sites| and |denied_sites|.
  void UpdateObsoleteNotificationsSettings(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSetting setting,
      ListValue* allowed_sites,
      ListValue* denied_sites);

  // Various migration methods (old cookie, popup and per-host data gets
  // migrated to the new format). When calling these functions, |lock_|
  // should not be held, since these functions will send out notifications of
  // preference changes.
  void MigrateObsoletePerhostPref();
  void MigrateObsoletePopupsPref();
  void MigrateObsoleteContentSettingsPatternPref();
  void MigrateObsoleteGeolocationPref();
  void MigrateObsoleteNotificationsPrefs();

  // Copies the value of the preference that stores the content settings
  // exceptions to the obsolete preference for content settings exceptions. This
  // is necessary to allow content settings exceptions beeing synced to older
  // versions of chrome that only use the obsolete preference.
  void SyncObsoletePatternPref();

  // Copies the notifications and geolocation content settings exceptions from
  // the preference that stores the content settings exceptions to the obsolete
  // preference for notification and geolocation content settings exceptions.
  // This is necessary to allow notifications and geolocation content settings
  // exceptions being synced to older versions of chrome that only use the
  // obsolete preference.
  void SyncObsoletePrefs();

  static void CanonicalizeContentSettingsExceptions(
      base::DictionaryValue* all_settings_dictionary);

  // In the debug mode, asserts that |lock_| is not held by this thread. It's
  // ok if some other thread holds |lock_|, as long as it will eventually
  // release it.
  void AssertLockNotHeld() const;

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  PrefService* prefs_;

  bool is_incognito_;

  PrefChangeRegistrar pref_change_registrar_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  OriginIdentifierValueMap value_map_;

  OriginIdentifierValueMap incognito_value_map_;

  // Used around accesses to the value map objects to guarantee thread safety.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PrefProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PREF_PROVIDER_H_
