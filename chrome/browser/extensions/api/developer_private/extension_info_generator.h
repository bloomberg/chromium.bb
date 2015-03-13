// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_

#include "chrome/common/extensions/api/developer_private.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ErrorConsole;
class Extension;
class ExtensionPrefs;
class ExtensionSystem;
class WarningService;

// Generates the developerPrivate api's specification for ExtensionInfo.
class ExtensionInfoGenerator {
 public:
  using ExtensionInfoList =
      std::vector<linked_ptr<api::developer_private::ExtensionInfo>>;

  explicit ExtensionInfoGenerator(content::BrowserContext* context);
  ~ExtensionInfoGenerator();

  // Return the ExtensionInfo for a given |extension| and |state|.
  scoped_ptr<api::developer_private::ExtensionInfo> CreateExtensionInfo(
      const Extension& extension,
      api::developer_private::ExtensionState state);

  // Return a collection of ExtensionInfos, optionally including disabled and
  // terminated.
  ExtensionInfoList CreateExtensionsInfo(bool include_disabled,
                                         bool include_terminated);

 private:
  // Various systems, cached for convenience.
  content::BrowserContext* browser_context_;
  ExtensionSystem* extension_system_;
  ExtensionPrefs* extension_prefs_;
  WarningService* warning_service_;
  ErrorConsole* error_console_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoGenerator);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
