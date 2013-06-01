// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/api/system_info_cpu/system_info_cpu_api.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/features/base_feature_provider.h"

namespace extensions {

using api::system_info_cpu::CpuInfo;

SystemInfoCpuGetFunction::SystemInfoCpuGetFunction() {
}

SystemInfoCpuGetFunction::~SystemInfoCpuGetFunction() {
}

bool SystemInfoCpuGetFunction::RunImpl() {
  CpuInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemInfoCpuGetFunction::OnGetCpuInfoCompleted, this));
  return true;
}

void SystemInfoCpuGetFunction::OnGetCpuInfoCompleted(const CpuInfo& info,
    bool success) {
  if (success)
    SetResult(info.ToValue().release());
  else
    SetError("Error occurred when querying cpu information.");
  SendResponse(success);
}

}  // namespace extensions
