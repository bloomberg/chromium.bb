// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionService;

namespace extensions {
class Extension;
}

namespace content_settings {

// A content settings provider which disables certain plugins for platform apps.
class InternalExtensionProvider : public ObservableProvider,
                            public content::NotificationObserver {
 public:
  explicit InternalExtensionProvider(ExtensionService* extension_service);

  virtual ~InternalExtensionProvider();

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
      base::Value* value) OVERRIDE;

  virtual void ClearAllContentSettingsRules(ContentSettingsType content_type)
      OVERRIDE;

  virtual void ShutdownOnUIThread() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  void SetContentSettingForExtension(const extensions::Extension* extension,
                                     ContentSetting setting);
  void SetContentSettingForExtensionAndResource(
      const extensions::Extension* extension,
      const ResourceIdentifier& resource,
      ContentSetting setting);

  OriginIdentifierValueMap value_map_;

  // Used around accesses to the |value_map_| list to guarantee thread safety.
  mutable base::Lock lock_;
  scoped_ptr<content::NotificationRegistrar> registrar_;

  DISALLOW_COPY_AND_ASSIGN(InternalExtensionProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_
