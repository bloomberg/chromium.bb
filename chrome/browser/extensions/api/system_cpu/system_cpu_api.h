// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_SYSTEM_CPU_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_SYSTEM_CPU_API_H_

#include "chrome/common/extensions/api/system_cpu.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemCpuGetInfoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.cpu.getInfo", SYSTEM_CPU_GETINFO)
  SystemCpuGetInfoFunction();

 private:
  virtual ~SystemCpuGetInfoFunction();
  virtual bool RunAsync() OVERRIDE;
  void OnGetCpuInfoCompleted(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_SYSTEM_CPU_API_H_
