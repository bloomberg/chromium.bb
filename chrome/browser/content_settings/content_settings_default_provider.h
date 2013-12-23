// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

// Provider that provides default content settings based on
// user prefs. If no default values are set by the user we use the hard coded
// default values.
class DefaultProvider : public ObservableProvider {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  DefaultProvider(PrefService* prefs,
                  bool incognito);
  virtual ~DefaultProvider();

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
      base::Value* value) OVERRIDE;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) OVERRIDE;

  virtual void ShutdownOnUIThread() OVERRIDE;

 private:
  // Sets the fields of |settings| based on the values in |dictionary|.
  void GetSettingsFromDictionary(const base::DictionaryValue* dictionary);

  // Forces the default settings to be explicitly set instead of themselves
  // being CONTENT_SETTING_DEFAULT.
  void ForceDefaultsToBeExplicit();

  // Reads the default settings from the preferences service. If |overwrite| is
  // true and the preference is missing, the local copy will be cleared as well.
  void ReadDefaultSettings(bool overwrite);

  // Called on prefs change.
  void OnPreferenceChanged(const std::string& pref_name);

  typedef linked_ptr<base::Value> ValuePtr;
  typedef std::map<ContentSettingsType, ValuePtr> ValueMap;
  // Copies of the pref data, so that we can read it on the IO thread.
  ValueMap default_settings_;

  PrefService* prefs_;

  // Whether this settings map is for an Incognito session.
  bool is_incognito_;

  // Used around accesses to the |default_content_settings_| object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  PrefChangeRegistrar pref_change_registrar_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  DISALLOW_COPY_AND_ASSIGN(DefaultProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_
