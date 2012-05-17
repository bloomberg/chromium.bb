// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"

namespace content_settings {

// A content settings provider which manages settings defined by extensions.
class ExtensionProvider : public ObservableProvider,
                          public extensions::ContentSettingsStore::Observer {
 public:
  ExtensionProvider(extensions::ContentSettingsStore* extensions_settings,
                    bool incognito);

  virtual ~ExtensionProvider();

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
      OVERRIDE {}

  virtual void ShutdownOnUIThread() OVERRIDE;

  // extensions::ContentSettingsStore::Observer methods:
  virtual void OnContentSettingChanged(const std::string& extension_id,
                                       bool incognito) OVERRIDE;

 private:
  // Specifies whether this provider manages settings for incognito or regular
  // sessions.
  bool incognito_;

  // The backend storing content setting rules defined by extensions.
  scoped_refptr<extensions::ContentSettingsStore> extensions_settings_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_EXTENSION_PROVIDER_H_
