// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

namespace extensions {

using api::experimental_system_info_cpu::CpuInfo;

// CpuInfoProvider implementation on Windows platform.
class CpuInfoProviderWin : public CpuInfoProvider {
 public:
  CpuInfoProviderWin() {}
  virtual ~CpuInfoProviderWin() {}
  virtual bool QueryInfo(CpuInfo* info) OVERRIDE;
};

bool CpuInfoProviderWin::QueryInfo(CpuInfo* info) {
  // TODO(hmin): not implemented yet.
  return false;
}

// static
template<>
CpuInfoProvider* CpuInfoProvider::Get() {
  return CpuInfoProvider::GetInstance<CpuInfoProviderWin>();
}

}  // namespace extensions
