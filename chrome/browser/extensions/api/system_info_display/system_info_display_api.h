// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_SYSTEM_INFO_DISPLAY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_SYSTEM_INFO_DISPLAY_API_H_

#include <string>

#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class SystemInfoDisplayGetDisplayInfoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("systemInfo.display.getDisplayInfo",
                             SYSTEMINFO_DISPLAY_GETDISPLAYINFO);

 protected:
  virtual ~SystemInfoDisplayGetDisplayInfoFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnGetDisplayInfoCompleted(const DisplayInfo& info, bool success);
};

class SystemInfoDisplaySetDisplayPropertiesFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("systemInfo.display.setDisplayProperties",
                             SYSTEMINFO_DISPLAY_SETDISPLAYPROPERTIES);

 protected:
  virtual ~SystemInfoDisplaySetDisplayPropertiesFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnPropertiesSet(bool success, const std::string& error);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_SYSTEM_INFO_DISPLAY_API_H__
