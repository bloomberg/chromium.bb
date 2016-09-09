// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/browser/memory_monitor.h"

#include "base/process/process_metrics.h"

namespace memory_coordinator {

MemoryMonitorDelegate::~MemoryMonitorDelegate() {}

void MemoryMonitorDelegate::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) {
  base::GetSystemMemoryInfo(mem_info);
}

}  // namespace memory_coordinator
