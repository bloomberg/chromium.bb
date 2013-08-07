// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_cpu/cpu_info_provider.h"

#include "base/sys_info.h"

namespace extensions {

using api::system_cpu::CpuInfo;

// Static member intialization.
base::LazyInstance<scoped_refptr<CpuInfoProvider> >
    CpuInfoProvider::provider_ = LAZY_INSTANCE_INITIALIZER;

CpuInfoProvider::CpuInfoProvider() {}

CpuInfoProvider::~CpuInfoProvider() {}

const CpuInfo& CpuInfoProvider::cpu_info() const {
  return info_;
}

void CpuInfoProvider::InitializeForTesting(
    scoped_refptr<CpuInfoProvider> provider) {
  DCHECK(provider.get() != NULL);
  provider_.Get() = provider;
}

bool CpuInfoProvider::QueryInfo() {
  info_.num_of_processors = base::SysInfo::NumberOfProcessors();
  info_.arch_name = base::SysInfo::OperatingSystemArchitecture();
  info_.model_name = base::SysInfo::CPUModelName();
  return true;
}

// static
CpuInfoProvider* CpuInfoProvider::Get() {
  if (provider_.Get().get() == NULL)
    provider_.Get() = new CpuInfoProvider();
  return provider_.Get();
}

}  // namespace extensions
