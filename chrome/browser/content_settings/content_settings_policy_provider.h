// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
#pragma once

// A content settings provider that takes its settings out of policies.

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

class PolicyDefaultProvider : public DefaultProviderInterface,
                              public NotificationObserver {
 public:
  explicit PolicyDefaultProvider(Profile* profile);
  virtual ~PolicyDefaultProvider();

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

  // Reads the policy managed default settings.
  void ReadManagedDefaultSettings();

  // Reads the policy controlled default settings for a specific content type.
  void UpdateManagedDefaultSetting(ContentSettingsType content_type);

  // Copies of the pref data, so that we can read it on the IO thread.
  ContentSettings managed_default_content_settings_;

  Profile* profile_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Used around accesses to the managed_default_content_settings_ object to
  // guarantee thread safety.
  mutable base::Lock lock_;

  PrefChangeRegistrar pref_change_registrar_;
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PolicyDefaultProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_POLICY_PROVIDER_H_
