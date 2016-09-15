// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_monitor.h"

#include "base/process/process_metrics.h"

namespace content {

// static
MemoryMonitorDelegate* MemoryMonitorDelegate::GetInstance() {
  return base::Singleton<
      MemoryMonitorDelegate,
      base::LeakySingletonTraits<MemoryMonitorDelegate>>::get();
}

MemoryMonitorDelegate::~MemoryMonitorDelegate() {}

void MemoryMonitorDelegate::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) {
  base::GetSystemMemoryInfo(mem_info);
}

}  // namespace content
