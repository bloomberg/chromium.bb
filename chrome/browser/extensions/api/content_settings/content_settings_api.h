// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace webkit {
namespace npapi {
class PluginGroup;
}
}

namespace extensions {

class ClearContentSettingsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.clear")

 protected:
  virtual ~ClearContentSettingsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetContentSettingFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.get")

 protected:
  virtual ~GetContentSettingFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SetContentSettingFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.set")

 protected:
  virtual ~SetContentSettingFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetResourceIdentifiersFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.getResourceIdentifiers")

 protected:
  virtual ~GetResourceIdentifiersFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest,
                           ContentSettingsGetResourceIdentifiers);

  void OnGotPluginGroups(const std::vector<webkit::npapi::PluginGroup>& groups);

  // Used to override the global plugin list in tests.
  static void SetPluginGroupsForTesting(
      const std::vector<webkit::npapi::PluginGroup>* plugin_groups);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_H__
