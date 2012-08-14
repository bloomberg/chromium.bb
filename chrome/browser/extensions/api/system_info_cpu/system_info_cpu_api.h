// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_SYSTEM_INFO_CPU_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_SYSTEM_INFO_CPU_API_H_

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class CpuInfoProvider;

class SystemInfoCpuGetFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.systemInfo.cpu.get");
  SystemInfoCpuGetFunction();

 private:
  virtual ~SystemInfoCpuGetFunction();
  virtual bool RunImpl() OVERRIDE;
  void WorkOnFileThread();
  void RespondOnUIThread(bool success);
  void GetCpuInfoOnFileThread();

  // The CpuInfoProvider instance, lives on FILE thread.
  CpuInfoProvider* provider_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_SYSTEM_INFO_CPU_API_H_
