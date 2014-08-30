// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/resource_manager/public/resource_manager_delegate.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process_metrics.h"

namespace athena {

namespace {
// This is the minimum amount of time in milliseconds between checks for
// memory pressure.
const int kMemoryPressureIntervalMs = 750;
}  // namespace

class ResourceManagerDelegateImpl : public ResourceManagerDelegate {
 public:
  ResourceManagerDelegateImpl() {}
  virtual ~ResourceManagerDelegateImpl() {}

 private:
  virtual int GetUsedMemoryInPercent() OVERRIDE {
    base::SystemMemoryInfoKB info;
    if (!base::GetSystemMemoryInfo(&info)) {
      LOG(WARNING) << "Cannot determine the free memory of the system.";
      return 0;
    }
    // TODO(skuhne): Instead of adding the kernel memory pressure calculation
    // logic here, we should have a kernel mechanism similar to the low memory
    // notifier in ChromeOS which offers multiple pressure states.
    // To track this, we have crbug.com/381196.

    // The available memory consists of "real" and virtual (z)ram memory.
    // Since swappable memory uses a non pre-deterministic compression and
    // the compression creates its own "dynamic" in the system, it gets
    // de-emphasized by the |kSwapWeight| factor.
    const int kSwapWeight = 4;

    // The total memory we have is the "real memory" plus the virtual (z)ram.
    int total_memory = info.total + info.swap_total / kSwapWeight;

    // The kernel internally uses 50MB.
    const int kMinFileMemory = 50 * 1024;

    // Most file memory can be easily reclaimed.
    int file_memory = info.active_file + info.inactive_file;
    // unless it is dirty or it's a minimal portion which is required.
    file_memory -= info.dirty + kMinFileMemory;

    // Available memory is the sum of free, swap and easy reclaimable memory.
    int available_memory =
        info.free + info.swap_free / kSwapWeight + file_memory;

    DCHECK(available_memory < total_memory);
    int percentage = ((total_memory - available_memory) * 100) / total_memory;
    return percentage;
  }

  virtual int MemoryPressureIntervalInMS() OVERRIDE {
    return kMemoryPressureIntervalMs;
  }
  DISALLOW_COPY_AND_ASSIGN(ResourceManagerDelegateImpl);
};

// static
ResourceManagerDelegate*
ResourceManagerDelegate::CreateResourceManagerDelegate() {
  return new ResourceManagerDelegateImpl;
}

}  // namespace athena
