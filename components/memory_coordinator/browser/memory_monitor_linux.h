// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_MONITOR_LINUX_H_
#define COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_MONITOR_LINUX_H_

#include "components/memory_coordinator/browser/memory_monitor.h"
#include "components/memory_coordinator/common/memory_coordinator_export.h"

namespace base {
struct SystemMemoryInfoKB;
}  // namespace base

namespace memory_coordinator {

// A memory monitor for the Linux platform.
class MEMORY_COORDINATOR_EXPORT MemoryMonitorLinux : public MemoryMonitor {
 public:
  MemoryMonitorLinux(MemoryMonitorDelegate* delegate);
  ~MemoryMonitorLinux() override {}

  // MemoryMonitor:
  int GetFreeMemoryUntilCriticalMB() override;

  // Factory function to create an instance of this class.
  static std::unique_ptr<MemoryMonitorLinux> Create(
      MemoryMonitorDelegate* delegate);

 private:
  // The delegate to be used for retrieving system memory information. Used as a
  // testing seam.
  MemoryMonitorDelegate* delegate_;
};

}  // namespace memory_coordinator

#endif  // COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_MONITOR_LINUX_H_
