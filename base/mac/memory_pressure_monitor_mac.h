// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MEMORY_PRESSURE_MONITOR_MAC_H_
#define BASE_MAC_MEMORY_PRESSURE_MONITOR_MAC_H_

#include <dispatch/dispatch.h>

#include "base/base_export.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"

// The following was added to <dispatch/source.h> after 10.8.
// TODO(shrike): Remove the DISPATCH_MEMORYPRESSURE_NORMAL ifndef once builders
// reach 10.9 or higher.
#ifndef DISPATCH_MEMORYPRESSURE_NORMAL

#define DISPATCH_MEMORYPRESSURE_NORMAL    0x01
#define DISPATCH_MEMORYPRESSURE_WARN      0x02
#define DISPATCH_MEMORYPRESSURE_CRITICAL  0x04

#endif  // DISPATCH_MEMORYPRESSURE_NORMAL

namespace base {

class TestMemoryPressureMonitorMac;

struct DispatchSourceSDeleter {
  void operator()(dispatch_source_s* ptr) {
    dispatch_source_cancel(ptr);
    dispatch_release(ptr);
  }
};

// Declares the interface for the Mac MemoryPressureMonitor, which reports
// memory pressure events and status.
class BASE_EXPORT MemoryPressureMonitorMac : public MemoryPressureMonitor {
 public:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;
  MemoryPressureMonitorMac();
  ~MemoryPressureMonitorMac() override;

  // Returns the currently-observed memory pressure.
  MemoryPressureLevel GetCurrentPressureLevel() const override;

 private:
  friend TestMemoryPressureMonitorMac;

  static MemoryPressureLevel
      MemoryPressureLevelForMacMemoryPressure(int mac_memory_pressure);
  static void NotifyMemoryPressureChanged(dispatch_source_s* event_source);

  scoped_ptr<dispatch_source_s, DispatchSourceSDeleter>
      memory_level_event_source_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureMonitorMac);
};

}  // namespace base

#endif  // BASE_MAC_MEMORY_PRESSURE_MONITOR_MAC_H_
