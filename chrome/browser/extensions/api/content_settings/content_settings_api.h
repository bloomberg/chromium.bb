// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H_

#include "chrome/browser/extensions/extension_function.h"

class PluginFinder;

namespace webkit {
struct WebPluginInfo;
}

namespace extensions {

class ContentSettingsClearFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.clear", CONTENTSETTINGS_CLEAR)

 protected:
  virtual ~ContentSettingsClearFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ContentSettingsGetFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.get", CONTENTSETTINGS_GET)

 protected:
  virtual ~ContentSettingsGetFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ContentSettingsSetFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.set", CONTENTSETTINGS_SET)

 protected:
  virtual ~ContentSettingsSetFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ContentSettingsGetResourceIdentifiersFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentSettings.getResourceIdentifiers",
                             CONTENTSETTINGS_GETRESOURCEIDENTIFIERS)

 protected:
  virtual ~ContentSettingsGetResourceIdentifiersFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest,
                           ContentSettingsGetResourceIdentifiers);

  // Callback method that gets executed when |plugins|
  // are asynchronously fetched.
  void OnGotPlugins(const std::vector<webkit::WebPluginInfo>& plugins);

  // Used to override the global plugin list in tests.
  static void SetPluginsForTesting(
      const std::vector<webkit::WebPluginInfo>* plugins);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H_
