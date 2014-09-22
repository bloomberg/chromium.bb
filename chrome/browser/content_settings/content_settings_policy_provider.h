// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_

// A content settings provider that takes its settings out of policies.

#include <vector>

#include "base/basictypes.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

// PolicyProvider that provides managed content-settings.
class PolicyProvider : public ObservableProvider {
 public:
  explicit PolicyProvider(PrefService* prefs);
  virtual ~PolicyProvider();
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

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
  // Reads the policy managed default settings.
  void ReadManagedDefaultSettings();

  // Callback for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  // Reads the policy controlled default settings for a specific content type.
  void UpdateManagedDefaultSetting(ContentSettingsType content_type);

  void ReadManagedContentSettings(bool overwrite);

  void GetContentSettingsFromPreferences(OriginIdentifierValueMap* rules);

  void GetAutoSelectCertificateSettingsFromPreferences(
      OriginIdentifierValueMap* value_map);

  void ReadManagedContentSettingsTypes(ContentSettingsType content_type);

  OriginIdentifierValueMap value_map_;

  PrefService* prefs_;

  PrefChangeRegistrar pref_change_registrar_;

  // Used around accesses to the |value_map_| object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
