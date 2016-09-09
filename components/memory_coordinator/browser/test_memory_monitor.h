// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/browser/memory_monitor.h"

#include "base/process/process_metrics.h"

namespace memory_coordinator {

// A delegate that allows mocking the various inputs to MemoryMonitor.
class TestMemoryMonitorDelegate : public MemoryMonitorDelegate {
 public:
  TestMemoryMonitorDelegate() : calls_(0) { mem_info_ = {}; }
  ~TestMemoryMonitorDelegate() override;

  void GetSystemMemoryInfo(base::SystemMemoryInfoKB* mem_info) override;

  size_t calls() const { return calls_; }
  void ResetCalls() { calls_ = 0; }

  void SetTotalMemoryKB(int total_memory_kb) {
    mem_info_.total = total_memory_kb;
  }

 protected:
  // Because the fields of SystemMemoryInfoKB vary depending on the operating
  // system, specific derived classes will have to provide methods to load
  // values into |mem_info_|;
  base::SystemMemoryInfoKB mem_info_;

 private:
  size_t calls_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryMonitorDelegate);
};

}  // namespace memory_coordinator
