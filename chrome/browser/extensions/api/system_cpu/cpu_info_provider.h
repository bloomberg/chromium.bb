// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_

#include <vector>

#include "base/cpu.h"
#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/system_cpu.h"

namespace extensions {

class CpuInfoProvider : public SystemInfoProvider {
 public:
  // Return the single shared instance of CpuInfoProvider.
  static CpuInfoProvider* Get();

  const api::system_cpu::CpuInfo& cpu_info() const;

  static void InitializeForTesting(scoped_refptr<CpuInfoProvider> provider);

 private:
  friend class MockCpuInfoProviderImpl;

  CpuInfoProvider();
  virtual ~CpuInfoProvider();

  // Platform specific implementation for querying the CPU time information
  // for each processor.
  virtual bool QueryCpuTimePerProcessor(
      std::vector<linked_ptr<api::system_cpu::ProcessorInfo> >* infos);

  // Overriden from SystemInfoProvider.
  virtual bool QueryInfo() OVERRIDE;

  // Creates a list of codenames for currently active features.
  std::vector<std::string> GetFeatures() const;

  // The last information filled up by QueryInfo and is accessed on multiple
  // threads, but the whole class is being guarded by SystemInfoProvider base
  // class.
  //
  // |info_| is accessed on the UI thread while |is_waiting_for_completion_| is
  // false and on the sequenced worker pool while |is_waiting_for_completion_|
  // is true.
  api::system_cpu::CpuInfo info_;

  static base::LazyInstance<scoped_refptr<CpuInfoProvider> > provider_;
  base::CPU cpu_;

  DISALLOW_COPY_AND_ASSIGN(CpuInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_
