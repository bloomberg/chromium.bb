// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_

#include <string>

#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/extensions/extension_content_settings_store.h"

class ContentSettingsDetails;
class HostContentSettingsMap;
class Profile;

namespace content_settings {

class Observer;

// A content settings provider which manages settings defined by extensions.
class ExtensionProvider : public ObservableProvider,
                          public ExtensionContentSettingsStore::Observer {
 public:
  ExtensionProvider(ExtensionContentSettingsStore* extensions_settings,
                    bool incognito);

  virtual ~ExtensionProvider();

  // Provider Interface implementations.
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

  virtual void SetContentSetting(
      const ContentSettingsPattern& embedded_url_pattern,
      const ContentSettingsPattern& top_level_url_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting) {}

  virtual void GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const;

  virtual void ClearAllContentSettingsRules(ContentSettingsType content_type) {}

  virtual void ResetToDefaults() {}

  virtual void ShutdownOnUIThread();

  // ExtensionContentSettingsStore::Observer methods:
  virtual void OnContentSettingChanged(const std::string& extension_id,
                                       bool incognito);

 private:
  // Specifies whether this provider manages settings for incognito or regular
  // sessions.
  bool incognito_;

  // The backend storing content setting rules defined by extensions.
  scoped_refptr<ExtensionContentSettingsStore> extensions_settings_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_
