// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_

#include <string>

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class SystemDisplayGetInfoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getInfo",
                             SYSTEM_DISPLAY_GETINFO);

 protected:
  virtual ~SystemDisplayGetInfoFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnGetDisplayInfoCompleted(bool success);
};

class SystemDisplaySetDisplayPropertiesFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayProperties",
                             SYSTEM_DISPLAY_SETDISPLAYPROPERTIES);

 protected:
  virtual ~SystemDisplaySetDisplayPropertiesFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnPropertiesSet(bool success, const std::string& error);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
