// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PLATFORM_APP_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PLATFORM_APP_PROVIDER_H_

#include "base/synchronization/lock.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content_settings {

// A content settings provider which disables certain plugins for platform apps.
class PlatformAppProvider : public ObservableProvider,
                            public content::NotificationObserver {
 public:
  PlatformAppProvider();

  virtual ~PlatformAppProvider();

  // ProviderInterface methods:
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

  virtual void ClearAllContentSettingsRules(ContentSettingsType content_type)
      OVERRIDE;

  virtual void ShutdownOnUIThread() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  OriginIdentifierValueMap value_map_;

  // Used around accesses to the |value_map_| list to guarantee thread safety.
  mutable base::Lock lock_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAppProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PLATFORM_APP_PROVIDER_H_
