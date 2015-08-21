// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemDisplayGetInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getInfo", SYSTEM_DISPLAY_GETINFO);

 protected:
  ~SystemDisplayGetInfoFunction() override {}
  bool RunSync() override;
};

class SystemDisplaySetDisplayPropertiesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayProperties",
                             SYSTEM_DISPLAY_SETDISPLAYPROPERTIES);

 protected:
  ~SystemDisplaySetDisplayPropertiesFunction() override {}
  bool RunSync() override;
};

class SystemDisplayEnableUnifiedDesktopFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.enableUnifiedDesktop",
                             SYSTEM_DISPLAY_ENABLEUNIFIEDDESKTOP);

 protected:
  ~SystemDisplayEnableUnifiedDesktopFunction() override {}
  bool RunSync() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
