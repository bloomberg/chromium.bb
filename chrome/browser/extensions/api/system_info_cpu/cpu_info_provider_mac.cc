// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

namespace extensions {

namespace {

using api::experimental_system_info_cpu::CpuInfo;

// CpuInfoProvider implementation on Mac OS.
class CpuInfoProviderMac : public CpuInfoProvider {
 public:
  CpuInfoProviderMac() {}
  virtual ~CpuInfoProviderMac() {}
  virtual bool QueryInfo(CpuInfo* info) OVERRIDE;
};

bool CpuInfoProviderMac::QueryInfo(CpuInfo* info) {
  // TODO(hmin): not implemented yet.
  return false;
}

}  // namespace

// static
CpuInfoProvider* CpuInfoProvider::Get() {
  return CpuInfoProvider::GetInstance<CpuInfoProviderMac>();
}

}  // namespace extensions
