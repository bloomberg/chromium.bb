// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor.h"

#include <windows.h>

#include "base/optional.h"

namespace performance_monitor {

namespace {

const DWORDLONG kMBBytes = 1024 * 1024;

// Returns the amount of physical memory available on the system.
base::Optional<int> GetFreePhysMemoryMb() {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status))
    return base::nullopt;

  return (mem_status.ullAvailPhys / kMBBytes);
}

}  // namespace

void SystemMonitor::FreePhysMemoryMetricEvaluator::Evaluate() {
  auto value = GetFreePhysMemoryMb();
  if (value) {
    value_ = value.value();
    has_value_ = true;
  }
}

}  // namespace performance_monitor
