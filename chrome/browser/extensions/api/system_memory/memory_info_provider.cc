// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_memory/memory_info_provider.h"

#include "base/sys_info.h"

namespace extensions {

using api::system_memory::MemoryInfo;

// Static member intialization.
template<>
base::LazyInstance<scoped_refptr<SystemInfoProvider<MemoryInfo> > >
  SystemInfoProvider<MemoryInfo>::provider_ = LAZY_INSTANCE_INITIALIZER;

MemoryInfoProvider::MemoryInfoProvider() {}

MemoryInfoProvider::~MemoryInfoProvider() {}

const MemoryInfo& MemoryInfoProvider::memory_info() const {
  return info_;
}

bool MemoryInfoProvider::QueryInfo() {
  info_.capacity = static_cast<double>(base::SysInfo::AmountOfPhysicalMemory());
  info_.available_capacity =
     static_cast<double>(base::SysInfo::AmountOfAvailablePhysicalMemory());
  return true;
}

// static
MemoryInfoProvider* MemoryInfoProvider::Get() {
  return MemoryInfoProvider::GetInstance<MemoryInfoProvider>();
}

}  // namespace extensions

