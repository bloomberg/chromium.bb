// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_

// A content settings provider that takes its settings out of the pref service.

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
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

class ContentSettingsPref;

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

  void Notify(const ContentSettingsPattern& primary_pattern,
              const ContentSettingsPattern& secondary_pattern,
              ContentSettingsType content_type,
              const std::string& resource_identifier);

  // Gains ownership of |clock|.
  void SetClockForTesting(scoped_ptr<base::Clock> clock);

 private:
  friend class DeadlockCheckerThread;  // For testing.

  // Migrate the old media setting into new mic/camera content settings.
  void MigrateObsoleteMediaContentSetting();

  // Migrate the settings from the old aggregate dictionary into the new format.
  void MigrateAllExceptions();

  // Writes the contents of the old aggregate dictionary preferences into
  // separate dictionaries for content types. If |syncable_only| is true,
  // only syncable content types will be written.
  void WriteSettingsToNewPreferences(bool syncable_only);

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  PrefService* prefs_;

  // Can be set for testing.
  scoped_ptr<base::Clock> clock_;

  bool is_incognito_;

  PrefChangeRegistrar pref_change_registrar_;

  ScopedVector<ContentSettingsPref> content_settings_prefs_;

  DISALLOW_COPY_AND_ASSIGN(PrefProvider);

  bool TestAllLocks() const;

  // All functionality regarding reading and writing of preferences has been
  // moved to |ContentSettingsPref|, which manages one content type per
  // instance. However, for backward compatibility, we need to be able to write
  // to the old and deprecated aggregate dictionary preference which maintains
  // all content types. Therefore, |ContentSettingsPrefProvider| must still
  // retain some of the functionality of |ContentSettingsPref|. The following
  // attributes and methods serve this purpose.
  // TODO(msramek): Remove this migration code after two stable releases.
  struct ContentSettingsPrefEntry {
    ContentSettingsPrefEntry(const ContentSettingsPattern primary_pattern,
                             const ContentSettingsPattern secondary_pattern,
                             const ResourceIdentifier resource_identifier,
                             base::Value* value);
    ContentSettingsPrefEntry(const ContentSettingsPrefEntry& entry);
    ContentSettingsPrefEntry& operator=(const ContentSettingsPrefEntry& entry);
    ~ContentSettingsPrefEntry();

    ContentSettingsPattern primary_pattern;
    ContentSettingsPattern secondary_pattern;
    ResourceIdentifier resource_identifier;
    scoped_ptr<base::Value> value;
  };

  // Stores exceptions read from the old preference before writing them
  // to the new one.
  ScopedVector<ContentSettingsPrefEntry>
      pref_entry_map_[CONTENT_SETTINGS_NUM_TYPES];

  // Clears |pref_entry_map_|.
  void ClearPrefEntryMap();

  // Guards access to |pref_entry_map_|.
  mutable base::Lock old_lock_;

  // Indicates whether the old preferences are updated.
  bool updating_old_preferences_;

  // Called when the old preference changes.
  void OnOldContentSettingsPatternPairsChanged();

  // Reads the old preference and writes it to |pref_entry_map_|.
  void ReadContentSettingsFromOldPref();

  base::ThreadChecker thread_checker_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
