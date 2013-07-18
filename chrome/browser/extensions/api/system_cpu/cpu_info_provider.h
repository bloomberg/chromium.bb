// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_

#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_cpu.h"

namespace extensions {

class CpuInfoProvider
    : public SystemInfoProvider<api::system_cpu::CpuInfo> {
 public:
  // Overriden from SystemInfoProvider<CpuInfo>.
  virtual bool QueryInfo() OVERRIDE;

  // Return the single shared instance of CpuInfoProvider.
  static CpuInfoProvider* Get();

  const api::system_cpu::CpuInfo& cpu_info() const;

 private:
  friend class SystemInfoProvider<api::system_cpu::CpuInfo>;
  friend class MockCpuInfoProviderImpl;

  CpuInfoProvider();

  virtual ~CpuInfoProvider();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_

