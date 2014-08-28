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
    base::SystemMemoryInfoKB memory;
    // TODO(skuhne): According to semenzato this calculation has to change.
    if (base::GetSystemMemoryInfo(&memory) &&
        memory.total > 0 && memory.free >= 0) {
      return ((memory.total - memory.free) * 100) / memory.total;
    }
    LOG(WARNING) << "Cannot determine the free memory of the system.";
    return 0;
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
