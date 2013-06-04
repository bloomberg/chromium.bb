// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

#include <vector>

#include "base/timer.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_info_cpu.h"

namespace extensions {

class CpuInfoProvider
    : public SystemInfoProvider<api::system_info_cpu::CpuInfo> {
 public:
  // Overriden from SystemInfoProvider<CpuInfo>.
  virtual bool QueryInfo(
      api::system_info_cpu::CpuInfo* info) OVERRIDE;

  // Return the single shared instance of CpuInfoProvider.
  static CpuInfoProvider* Get();

 private:
  friend class SystemInfoProvider<api::system_info_cpu::CpuInfo>;
  friend class MockCpuInfoProviderImpl;
  friend class TestCpuInfoProvider;

  CpuInfoProvider();

  virtual ~CpuInfoProvider();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

