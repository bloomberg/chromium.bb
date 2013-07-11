// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

#include "base/sys_info.h"

namespace extensions {

using api::system_info_cpu::CpuInfo;

// Static member intialization.
template<>
base::LazyInstance<scoped_refptr<SystemInfoProvider<CpuInfo> > >
  SystemInfoProvider<CpuInfo>::provider_ = LAZY_INSTANCE_INITIALIZER;

CpuInfoProvider::CpuInfoProvider() {}

CpuInfoProvider::~CpuInfoProvider() {}

const CpuInfo& CpuInfoProvider::cpu_info() const {
  return info_;
}

bool CpuInfoProvider::QueryInfo() {
  info_.num_of_processors = base::SysInfo::NumberOfProcessors();
  info_.arch_name = base::SysInfo::OperatingSystemArchitecture();
  info_.model_name = base::SysInfo::CPUModelName();
  return true;
}

// static
CpuInfoProvider* CpuInfoProvider::Get() {
  return CpuInfoProvider::GetInstance<CpuInfoProvider>();
}

}  // namespace extensions
