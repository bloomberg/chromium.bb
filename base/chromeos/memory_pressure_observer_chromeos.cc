// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/chromeos/memory_pressure_observer_chromeos.h"

#include "base/process/process_metrics.h"
#include "base/time/time.h"

namespace base {

namespace {

// The time between memory pressure checks.
const int kMemoryPressureIntervalInMS = 1000;

// Converts free percent of memory into a memory pressure value.
MemoryPressureListener::MemoryPressureLevel GetMemoryPressureLevelFromFillLevel(
    int memory_fill_level) {
  if (memory_fill_level < 70)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  return memory_fill_level < 90 ?
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE :
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
}

// Gets the used ChromeOS memory in percent.
int GetUsedMemoryInPercent() {
  base::SystemMemoryInfoKB info;
  if (!base::GetSystemMemoryInfo(&info)) {
    VLOG(1) << "Cannot determine the free memory of the system.";
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

}  // namespace

MemoryPressureObserverChromeOS::MemoryPressureObserverChromeOS()
    : current_memory_pressure_level_(
        MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
  StartObserving();
}

MemoryPressureObserverChromeOS::~MemoryPressureObserverChromeOS() {
  StopObserving();
}

void MemoryPressureObserverChromeOS::StartObserving() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kMemoryPressureIntervalInMS),
               base::Bind(&MemoryPressureObserverChromeOS::CheckMemoryPressure,
                          base::Unretained(this)));
}

void MemoryPressureObserverChromeOS::StopObserving() {
  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
}

void MemoryPressureObserverChromeOS::CheckMemoryPressure() {
  MemoryPressureListener::MemoryPressureLevel old_pressure =
      current_memory_pressure_level_;
  MemoryPressureListener::MemoryPressureLevel new_pressure =
      GetMemoryPressureLevelFromFillLevel(GetUsedMemoryInPercent());
  if (old_pressure != new_pressure) {
    current_memory_pressure_level_ = new_pressure;
    // Everything but NONE will be sent to the listener.
    if (new_pressure != MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE)
      MemoryPressureListener::NotifyMemoryPressure(new_pressure);
  }
}

}  // namespace base
