// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PREF_CONTENT_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PREF_CONTENT_SETTINGS_PROVIDER_H_
#pragma once

// A content settings provider that takes its settings out of the pref service.

#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class ContentSettingsDetails;
class DictionaryValue;
class PrefService;
class Profile;

namespace content_settings {

class PrefDefaultProvider : public DefaultProviderInterface,
                            public NotificationObserver {
 public:
  explicit PrefDefaultProvider(Profile* profile);
  virtual ~PrefDefaultProvider();

  // DefaultContentSettingsProvider implementation.
  virtual ContentSetting ProvideDefaultSetting(
      ContentSettingsType content_type) const;
  virtual void UpdateDefaultSetting(ContentSettingsType content_type,
                                    ContentSetting setting);
  virtual void ResetToDefaults();
  virtual bool DefaultSettingIsManaged(ContentSettingsType content_type) const;

  static void RegisterUserPrefs(PrefService* prefs);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Informs observers that content settings have changed. Make sure that
  // |lock_| is not held when calling this, as listeners will usually call one
  // of the GetSettings functions in response, which would then lead to a
  // mutex deadlock.
  void NotifyObservers(const ContentSettingsDetails& details);

  void UnregisterObservers();

  // Sets the fields of |settings| based on the values in |dictionary|.
  void GetSettingsFromDictionary(const DictionaryValue* dictionary,
                                 ContentSettings* settings);

  // Forces the default settings to be explicitly set instead of themselves
  // being CONTENT_SETTING_DEFAULT.
  void ForceDefaultsToBeExplicit();

  // Reads the default settings from the preferences service. If |overwrite| is
  // true and the preference is missing, the local copy will be cleared as well.
  void ReadDefaultSettings(bool overwrite);

  // Copies of the pref data, so that we can read it on the IO thread.
  ContentSettings default_content_settings_;

  Profile* profile_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Used around accesses to the default_content_settings_ object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  PrefChangeRegistrar pref_change_registrar_;
  NotificationRegistrar notification_registrar_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  DISALLOW_COPY_AND_ASSIGN(PrefDefaultProvider);
};

class PrefProvider : public ProviderInterface,
                     public NotificationObserver {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

  explicit PrefProvider(Profile* profile);
  virtual ~PrefProvider();

  // ContentSettingsProvider implementation.
  virtual bool ContentSettingsTypeIsManaged(
      ContentSettingsType content_type);

  virtual ContentSetting GetContentSetting(
      const GURL& requesting_url,
      const GURL& embedding_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  virtual void SetContentSetting(
      const ContentSettingsPattern& requesting_pattern,
      const ContentSettingsPattern& embedding_pattern,
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
  typedef std::pair<ContentSettingsType, std::string>
      ContentSettingsTypeResourceIdentifierPair;
  typedef std::map<ContentSettingsTypeResourceIdentifierPair, ContentSetting>
      ResourceContentSettings;

  struct ExtendedContentSettings;

  typedef std::map<std::string, ExtendedContentSettings> HostContentSettings;

  // Various migration methods (old cookie, popup and per-host data gets
  // migrated to the new format).
  void MigrateObsoletePerhostPref(PrefService* prefs);
  void MigrateObsoletePopupsPref(PrefService* prefs);

  bool AllDefault(const ExtendedContentSettings& settings) const;

  void ReadExceptions(bool overwrite);

  bool RequiresResourceIdentifier(
      ContentSettingsType content_type) const;

  void CanonicalizeContentSettingsExceptions(
      DictionaryValue* all_settings_dictionary);

  void GetSettingsFromDictionary(
      const DictionaryValue* dictionary,
      ContentSettings* settings);

  void GetResourceSettingsFromDictionary(
      const DictionaryValue* dictionary,
      ResourceContentSettings* settings);

  void NotifyObservers(const ContentSettingsDetails& details);

  void UnregisterObservers();

  Profile* profile_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Used around accesses to the content_settings_ object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  PrefChangeRegistrar pref_change_registrar_;
  NotificationRegistrar notification_registrar_;

  // Copies of the pref data, so that we can read it on threads other than the
  // UI thread.
  HostContentSettings host_content_settings_;

  // Differences to the preference-stored host content settings for
  // off-the-record settings.
  HostContentSettings off_the_record_settings_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  // Do not fire any Notifications as long as we are in the constructor.
  bool initializing_;

  // LEGACY: TBR
  ContentSettings GetNonDefaultContentSettings(const GURL& url) const;

  DISALLOW_COPY_AND_ASSIGN(PrefProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PREF_CONTENT_SETTINGS_PROVIDER_H_
