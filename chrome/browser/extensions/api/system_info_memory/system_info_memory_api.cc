// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_memory/system_info_memory_api.h"

#include "chrome/browser/extensions/api/system_info_memory/memory_info_provider.h"

namespace extensions {

using api::experimental_system_info_memory::MemoryInfo;

SystemInfoMemoryGetFunction::SystemInfoMemoryGetFunction() {}

SystemInfoMemoryGetFunction::~SystemInfoMemoryGetFunction() {}

bool SystemInfoMemoryGetFunction::RunImpl() {
  MemoryInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemInfoMemoryGetFunction::OnGetMemoryInfoCompleted, this));
  return true;
}

void SystemInfoMemoryGetFunction::OnGetMemoryInfoCompleted(
    const MemoryInfo& info, bool success) {
  if (success)
    SetResult(info.ToValue().release());
  else
    SetError("Error occurred when querying memory information.");
  SendResponse(success);
}

}  // namespace extensions
