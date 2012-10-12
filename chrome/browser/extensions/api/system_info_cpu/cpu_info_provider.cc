// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

#include "base/sys_info.h"

namespace extensions {

using api::experimental_system_info_cpu::CpuInfo;

bool CpuInfoProvider::QueryInfo(CpuInfo* info) {
  if (info == NULL)
    return false;

  info->num_of_processors = base::SysInfo::NumberOfProcessors();
  info->arch_name = base::SysInfo::CPUArchitecture();
  info->model_name = base::SysInfo::CPUModelName();
  return true;
}

bool CpuInfoProvider::StartSampling(const QueryCpuTimeCallback& callback) {
  // TODO(hongbo): Implement sampling functionality for event router.
  return false;
}

bool CpuInfoProvider::StopSampling() {
  // TODO(hongbo): Implement sampling functionality for event router.
  return false;
}

// static
CpuInfoProvider* CpuInfoProvider::Get() {
  return CpuInfoProvider::GetInstance<CpuInfoProvider>();
}

}  // namespace extensions
