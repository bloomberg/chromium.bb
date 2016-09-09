// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/browser/test_memory_monitor.h"

namespace memory_coordinator {

TestMemoryMonitorDelegate::~TestMemoryMonitorDelegate() {}

void TestMemoryMonitorDelegate::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) {
  *mem_info = mem_info_;
  ++calls_;
}

}  // namespace memory_coordinator
