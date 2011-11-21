// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace webkit {
namespace npapi {
class PluginGroup;
}
}

class ClearContentSettingsFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.clear")
};

class GetContentSettingFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.get")
};

class SetContentSettingFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.set")
};

class GetResourceIdentifiersFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("contentSettings.getResourceIdentifiers")

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest,
                           ContentSettingsGetResourceIdentifiers);

  void OnGotPluginGroups(const std::vector<webkit::npapi::PluginGroup>& groups);

  // Used to override the global plugin list in tests.
  static void SetPluginGroupsForTesting(
      const std::vector<webkit::npapi::PluginGroup>* plugin_groups);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_API_H__
