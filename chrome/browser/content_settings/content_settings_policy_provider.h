// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
#pragma once

// A content settings provider that takes its settings out of policies.

#include <vector>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "base/tuple.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ContentSettingsDetails;
class HostContentSettingsMap;
class PrefService;

namespace content_settings {

// PolicyProvider that provider managed content-settings.
class PolicyProvider : public ObservableProvider,
                       public NotificationObserver {
 public:
  explicit PolicyProvider(PrefService* prefs);
  virtual ~PolicyProvider();
  static void RegisterUserPrefs(PrefService* prefs);

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

  virtual Value* GetContentSettingValue(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      std::vector<Rule>* content_setting_rules) const;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type);

  virtual void ShutdownOnUIThread();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  // Reads the policy managed default settings.
  void ReadManagedDefaultSettings();

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

  // Used around accesses to the content_settings_ object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
