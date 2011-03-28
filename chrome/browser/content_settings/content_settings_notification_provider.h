// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_NOTIFICATION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_NOTIFICATION_PROVIDER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class GURL;
class PrefService;
class Profile;

namespace content_settings {

class NotificationProvider : public ProviderInterface,
                             public NotificationObserver {
 public:
  static void RegisterUserPrefs(PrefService* user_prefs);

  static ContentSettingsPattern ToContentSettingsPattern(const GURL& origin);

  static GURL ToGURL(const ContentSettingsPattern& pattern);

  explicit NotificationProvider(Profile* profile);

  virtual ~NotificationProvider();

  // ProviderInterface implementation
  virtual bool ContentSettingsTypeIsManaged(
      ContentSettingsType content_type);

  virtual ContentSetting GetContentSetting(
      const GURL& requesting_url,
      const GURL& embedding_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  virtual void SetContentSetting(
      const ContentSettingsPattern& requesting_url_pattern,
      const ContentSettingsPattern& embedding_url_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting);

  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type);

  virtual void ResetToDefaults();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  void StartObserving();
  void StopObserving();

  void OnPrefsChanged(const std::string& pref_name);

  // Notifies the observers when permissions settings change.
  void NotifySettingsChange();

  // Returns all origins that explicitly have been allowed.
  std::vector<GURL> GetAllowedOrigins() const;

  // Returns all origins that explicitly have been denied.
  std::vector<GURL> GetBlockedOrigins() const;

  // Methods to setup and modify permission preferences.
  void GrantPermission(const GURL& origin);
  void DenyPermission(const GURL& origin);

  void PersistPermissionChange(const GURL& origin, bool is_allowed);

  ContentSetting GetContentSetting(const GURL& origin) const;

  // Removes an origin from the "explicitly allowed" set.
  void ResetAllowedOrigin(const GURL& origin);

  // Removes an origin from the "explicitly denied" set.
  void ResetBlockedOrigin(const GURL& origin);

  // Clears the sets of explicitly allowed and denied origins.
  void ResetAllOrigins();

  Profile* profile_;

  PrefChangeRegistrar prefs_registrar_;
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NotificationProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_NOTIFICATION_PROVIDER_H_
