// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_

// A content settings provider that takes its settings out of the pref service.

#include <vector>

#include "base/basictypes.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/browser/content_settings_utils.h"

class PrefService;

namespace base {
class Clock;
class DictionaryValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

// Content settings provider that provides content settings from the user
// preference.
class PrefProvider : public ObservableProvider {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  PrefProvider(PrefService* prefs, bool incognito);
  ~PrefProvider() override;

  // ProviderInterface implementations.
  RuleIterator* GetRuleIterator(ContentSettingsType content_type,
                                const ResourceIdentifier& resource_identifier,
                                bool incognito) const override;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const ResourceIdentifier& resource_identifier,
                         base::Value* value) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

  void ShutdownOnUIThread() override;

  // Records the last time the given pattern has used a certain content setting.
  void UpdateLastUsage(const ContentSettingsPattern& primary_pattern,
                       const ContentSettingsPattern& secondary_pattern,
                       ContentSettingsType content_type);

  base::Time GetLastUsage(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          ContentSettingsType content_type);

  // Gains ownership of |clock|.
  void SetClockForTesting(scoped_ptr<base::Clock> clock);

 private:
  friend class DeadlockCheckerThread;  // For testing.
  // Reads all content settings exceptions from the preference and load them
  // into the |value_map_|. The |value_map_| is cleared first.
  void ReadContentSettingsFromPref();

  // Callback for changes in the pref with the same name.
  void OnContentSettingsPatternPairsChanged();

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

  // Migrate the old media setting into new mic/camera content settings.
  void MigrateObsoleteMediaContentSetting();

  static void CanonicalizeContentSettingsExceptions(
      base::DictionaryValue* all_settings_dictionary);

  // In the debug mode, asserts that |lock_| is not held by this thread. It's
  // ok if some other thread holds |lock_|, as long as it will eventually
  // release it.
  void AssertLockNotHeld() const;

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  PrefService* prefs_;

  // Can be set for testing.
  scoped_ptr<base::Clock> clock_;

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

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
