// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

#include "chrome/browser/extensions/system_info_provider.h"

#include <vector>

#include "chrome/common/extensions/api/experimental_system_info_cpu.h"

namespace extensions {

class CpuInfoProvider
    : public SystemInfoProvider<api::experimental_system_info_cpu::CpuInfo> {
 public:
  typedef base::Callback<api::experimental_system_info_cpu::CpuUpdateInfo>
      QueryCpuTimeCallback;
  virtual ~CpuInfoProvider() {}

  // Overriden from SystemInfoProvider<CpuInfo>.
  virtual bool QueryInfo(
      api::experimental_system_info_cpu::CpuInfo* info) OVERRIDE;

  // Start sampling the CPU usage. The callback gets called when one sampling
  // cycle is completed periodically with the CPU updated usage info for each
  // processors. Return true if it succeeds to start, otherwise, false is
  // returned.
  bool StartSampling(const QueryCpuTimeCallback& callback);

  // Stop the sampling cycle. Return true if it succeeds to stop.
  bool StopSampling();

  // Return the single shared instance of CpuInfoProvider.
  static CpuInfoProvider* Get();

 private:
  // The amount of time that CPU spent on performing different kinds of work.
  // It is used to calculate the usage percent for processors.
  struct CpuTime {
    int64 user;   // user mode.
    int64 kernel; // kernel mode.
    int64 idle;   // twiddling thumbs.
  };

  // Platform specific implementation for querying the CPU time information
  // for each processor. Note that the first element is the total aggregated
  // numbers of all logic processors.
  bool QueryCpuTimePerProcessor(std::vector<CpuTime>* times);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

