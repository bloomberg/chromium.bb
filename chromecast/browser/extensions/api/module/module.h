// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_MODULE_MODULE_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_MODULE_MODULE_H_

#include "extensions/browser/extension_function.h"

namespace extensions {
class ExtensionPrefs;

namespace extension {
// Return the extension's update URL data, if any.
std::string GetUpdateURLData(const ExtensionPrefs* prefs,
                             const std::string& extension_id);
}  // namespace extension

class ExtensionSetUpdateUrlDataFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.setUpdateUrlData",
                             EXTENSION_SETUPDATEURLDATA)

 protected:
  ~ExtensionSetUpdateUrlDataFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionIsAllowedIncognitoAccessFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.isAllowedIncognitoAccess",
                             EXTENSION_ISALLOWEDINCOGNITOACCESS)

 protected:
  ~ExtensionIsAllowedIncognitoAccessFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ExtensionIsAllowedFileSchemeAccessFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.isAllowedFileSchemeAccess",
                             EXTENSION_ISALLOWEDFILESCHEMEACCESS)

 protected:
  ~ExtensionIsAllowedFileSchemeAccessFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_MODULE_MODULE_H_
