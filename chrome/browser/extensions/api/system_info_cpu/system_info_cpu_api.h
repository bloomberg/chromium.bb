// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_SYSTEM_INFO_CPU_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_SYSTEM_INFO_CPU_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/experimental_system_info_cpu.h"

namespace extensions {

class SystemInfoCpuGetFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.systemInfo.cpu.get",
                             EXPERIMENTAL_SYSTEMINFO_CPU_GET)
  SystemInfoCpuGetFunction();

 private:
  virtual ~SystemInfoCpuGetFunction();
  virtual bool RunImpl() OVERRIDE;
  void OnGetCpuInfoCompleted(
      const api::experimental_system_info_cpu::CpuInfo& info, bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_SYSTEM_INFO_CPU_API_H_
