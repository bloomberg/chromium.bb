// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/chromeos/memory_pressure_observer_chromeos.h"

#include "base/message_loop/message_loop.h"
#include "base/process/process_metrics.h"
#include "base/time/time.h"

namespace base {

namespace {

// The time between memory pressure checks. While under critical pressure, this
// is also the timer to repeat cleanup attempts.
const int kMemoryPressureIntervalMs = 1000;

// The time which should pass between two moderate memory pressure calls.
const int kModerateMemoryPressureCooldownMs = 10000;

// Number of event polls before the next moderate pressure event can be sent.
const int kModerateMemoryPressureCooldown =
    kModerateMemoryPressureCooldownMs / kMemoryPressureIntervalMs;

// Threshold constants to emit pressure events.
const int kMemoryPressureModerateThresholdPercent = 70;
const int kMemoryPressureCriticalThresholdPercent = 90;

// Converts free percent of memory into a memory pressure value.
MemoryPressureListener::MemoryPressureLevel GetMemoryPressureLevelFromFillLevel(
    int memory_fill_level) {
  if (memory_fill_level < kMemoryPressureModerateThresholdPercent)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  return memory_fill_level < kMemoryPressureCriticalThresholdPercent ?
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE :
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
}

}  // namespace

MemoryPressureObserverChromeOS::MemoryPressureObserverChromeOS()
    : current_memory_pressure_level_(
        MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      moderate_pressure_repeat_count_(0),
      weak_ptr_factory_(this) {
  StartObserving();
}

MemoryPressureObserverChromeOS::~MemoryPressureObserverChromeOS() {
  StopObserving();
}

void MemoryPressureObserverChromeOS::ScheduleEarlyCheck() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&MemoryPressureObserverChromeOS::CheckMemoryPressure,
           weak_ptr_factory_.GetWeakPtr()));
}

void MemoryPressureObserverChromeOS::StartObserving() {
  timer_.Start(FROM_HERE,
               TimeDelta::FromMilliseconds(kMemoryPressureIntervalMs),
               Bind(&MemoryPressureObserverChromeOS::CheckMemoryPressure,
                    weak_ptr_factory_.GetWeakPtr()));
}

void MemoryPressureObserverChromeOS::StopObserving() {
  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
}

void MemoryPressureObserverChromeOS::CheckMemoryPressure() {
  MemoryPressureListener::MemoryPressureLevel old_pressure =
      current_memory_pressure_level_;
  current_memory_pressure_level_ =
      GetMemoryPressureLevelFromFillLevel(GetUsedMemoryInPercent());
  // In case there is no memory pressure we do not notify.
  if (current_memory_pressure_level_ ==
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }
  if (old_pressure == current_memory_pressure_level_) {
    // If the memory pressure is still at the same level, we notify again for a
    // critical level. In case of a moderate level repeat however, we only send
    // a notification after a certain time has passed.
    if (current_memory_pressure_level_ ==
        MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE &&
          ++moderate_pressure_repeat_count_ <
              kModerateMemoryPressureCooldown) {
      return;
    }
  } else if (current_memory_pressure_level_ ==
               MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE &&
             old_pressure ==
               MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    // When we reducing the pressure level from critical to moderate, we
    // restart the timeout and do not send another notification.
    moderate_pressure_repeat_count_ = 0;
    return;
  }
  moderate_pressure_repeat_count_ = 0;
  MemoryPressureListener::NotifyMemoryPressure(current_memory_pressure_level_);
}

// Gets the used ChromeOS memory in percent.
int MemoryPressureObserverChromeOS::GetUsedMemoryInPercent() {
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

}  // namespace base
