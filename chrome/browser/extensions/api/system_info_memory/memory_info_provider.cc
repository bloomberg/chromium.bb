// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_memory/memory_info_provider.h"

#include "base/sys_info.h"

namespace extensions {

using api::experimental_system_info_memory::MemoryInfo;

MemoryInfoProvider::MemoryInfoProvider() {}

MemoryInfoProvider::~MemoryInfoProvider() {}

bool MemoryInfoProvider::QueryInfo(MemoryInfo* info) {
  info->capacity = static_cast<double>(base::SysInfo::AmountOfPhysicalMemory());
  info->available_capacity =
     static_cast<double>(base::SysInfo::AmountOfAvailablePhysicalMemory());
  return true;
}

// static
MemoryInfoProvider* MemoryInfoProvider::Get() {
  return MemoryInfoProvider::GetInstance<MemoryInfoProvider>();
}

}  // namespace extensions

