// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionRegistry;
}

namespace content_settings {

// A content settings provider which disables certain plugins for platform apps.
class InternalExtensionProvider : public ObservableProvider,
                                  public content::NotificationObserver,
                                  public extensions::ExtensionRegistryObserver {
 public:
  explicit InternalExtensionProvider(Profile* profile);

  ~InternalExtensionProvider() override;

  // ProviderInterface methods:
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
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

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;

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
  std::unique_ptr<content::NotificationRegistrar> registrar_;

  // Extension IDs used by the Chrome Remote Desktop app.
  std::set<std::string> chrome_remote_desktop_;

  extensions::ExtensionRegistry* extension_registry_;

  DISALLOW_COPY_AND_ASSIGN(InternalExtensionProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_INTERNAL_EXTENSION_PROVIDER_H_
