// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_MONITOR_H_
#define COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_MONITOR_H_

#include <memory>

#include "base/macros.h"
#include "components/memory_coordinator/common/memory_coordinator_export.h"

namespace memory_coordinator {

// A simple class that monitors the amount of free memory available on a system.
// This is an interface to facilitate dependency injection for testing.
class MEMORY_COORDINATOR_EXPORT MemoryMonitor {
 public:
  MemoryMonitor() {}
  virtual ~MemoryMonitor() {}

  // Returns the amount of free memory available on the system until the system
  // will be in a critical state. Critical is as defined by the OS (swapping
  // will occur, or physical memory will run out, etc). It is possible for this
  // to return negative values, in which case that much memory would have to be
  // freed in order to exit a critical memory state.
  virtual int GetFreeMemoryUntilCriticalMB() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryMonitor);
};

// Factory function for creating a monitor for the current platform.
MEMORY_COORDINATOR_EXPORT std::unique_ptr<MemoryMonitor> CreateMemoryMonitor();

}  // namespace memory_coordinator

#endif  // COMPONENTS_MEMORY_COORDINATOR_BROWSER_MEMORY_MONITOR_H_
