// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_SYSTEM_INFO_DISPLAY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_SYSTEM_INFO_DISPLAY_API_H_

#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class SystemInfoDisplayGetFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.systemInfo.display.get",
                             EXPERIMENTAL_SYSTEMINFO_DISPLAY_GET)

 protected:
  virtual ~SystemInfoDisplayGetFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnGetDisplayInfoCompleted(const DisplayInfo& info, bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_DISPLAY_SYSTEM_INFO_DISPLAY_API_H__
