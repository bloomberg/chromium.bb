// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemDisplayGetInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getInfo",
                             SYSTEM_DISPLAY_GETINFO);

 protected:
  virtual ~SystemDisplayGetInfoFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class SystemDisplaySetDisplayPropertiesFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayProperties",
                             SYSTEM_DISPLAY_SETDISPLAYPROPERTIES);

 protected:
  virtual ~SystemDisplaySetDisplayPropertiesFunction() {}
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
