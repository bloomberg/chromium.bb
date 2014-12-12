// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {
class Extension;
}

namespace content_settings {

// A content settings provider which disables certain plugins for platform apps.
class InternalExtensionProvider : public ObservableProvider,
                                  public content::NotificationObserver {
 public:
  explicit InternalExtensionProvider(Profile* profile);

  ~InternalExtensionProvider() override;

  // ProviderInterface methods:
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

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void ApplyPluginContentSettingsForExtension(
      const extensions::Extension* extension,
      ContentSetting setting);
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

  // Extension IDs used by the Chrome Remote Desktop app.
  std::set<std::string> chrome_remote_desktop_;

  DISALLOW_COPY_AND_ASSIGN(InternalExtensionProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_
