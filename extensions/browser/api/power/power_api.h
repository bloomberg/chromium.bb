// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_POWER_POWER_API_H_
#define EXTENSIONS_BROWSER_API_POWER_POWER_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

// Implementation of the chrome.power.requestKeepAwake API.
class PowerRequestKeepAwakeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("power.requestKeepAwake", POWER_REQUESTKEEPAWAKE)

 protected:
  virtual ~PowerRequestKeepAwakeFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

// Implementation of the chrome.power.releaseKeepAwake API.
class PowerReleaseKeepAwakeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("power.releaseKeepAwake", POWER_RELEASEKEEPAWAKE)

 protected:
  virtual ~PowerReleaseKeepAwakeFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_POWER_POWER_API_H_
