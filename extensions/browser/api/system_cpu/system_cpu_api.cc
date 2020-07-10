// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_cpu/system_cpu_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

namespace extensions {

using api::system_cpu::CpuInfo;

SystemCpuGetInfoFunction::SystemCpuGetInfoFunction() {
}

SystemCpuGetInfoFunction::~SystemCpuGetInfoFunction() {
}

ExtensionFunction::ResponseAction SystemCpuGetInfoFunction::Run() {
  CpuInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemCpuGetInfoFunction::OnGetCpuInfoCompleted, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void SystemCpuGetInfoFunction::OnGetCpuInfoCompleted(bool success) {
  if (success)
    Respond(OneArgument(CpuInfoProvider::Get()->cpu_info().ToValue()));
  else
    Respond(Error("Error occurred when querying cpu information."));
}

}  // namespace extensions
