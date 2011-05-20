// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_

#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/extensions/extension_content_settings_store.h"
#include "content/common/notification_details.h"

class ContentSettingsDetails;
class Profile;

namespace content_settings {

// A content settings provider which uses settings defined by extensions.
class ExtensionProvider : public ProviderInterface,
                          public ExtensionContentSettingsStore::Observer {
 public:
  ExtensionProvider(Profile* profile,
                    ExtensionContentSettingsStore* extensions_settings,
                    bool incognito);

  virtual ~ExtensionProvider();

  // ProviderInterface methods:
  virtual ContentSetting GetContentSetting(
      const GURL& embedded_url,
      const GURL& top_level_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  virtual void SetContentSetting(
      const ContentSettingsPattern& embedded_url_pattern,
      const ContentSettingsPattern& top_level_url_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting) {}

  // TODO(markusheintz): The UI needs a way to discover that these rules are
  // managed by an extension.
  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) {}

  virtual void ResetToDefaults() {}

  // ExtensionContentSettingsStore::Observer methods:
  virtual void OnContentSettingChanged(const std::string& extension_id,
                                       bool incognito);

  virtual void OnDestruction();

 private:
  void NotifyObservers(const ContentSettingsDetails& details);

  // TODO(markusheintz): That's only needed to send Notifications about changed
  // ContentSettings. This will be changed for all ContentSettingsProviders.
  // The HCSM will become an Observer of the ContentSettings Provider and send
  // out the Notifications itself.
  Profile* profile_;

  bool incognito_;

  ExtensionContentSettingsStore* extensions_settings_;  // Weak Pointer

  DISALLOW_COPY_AND_ASSIGN(ExtensionProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_
