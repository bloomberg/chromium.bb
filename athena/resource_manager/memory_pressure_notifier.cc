// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/resource_manager/memory_pressure_notifier.h"

#include "athena/resource_manager/public/resource_manager_delegate.h"
#include "base/time/time.h"


namespace athena {

MemoryPressureNotifier::MemoryPressureNotifier(
    MemoryPressureObserver* listener)
      : listener_(listener),
        current_pressure_(MemoryPressureObserver::MEMORY_PRESSURE_UNKNOWN) {
  StartObserving();
}

MemoryPressureNotifier::~MemoryPressureNotifier() {
  StopObserving();
}

void MemoryPressureNotifier::StartObserving() {
  int time_in_ms = listener_->GetDelegate()->MemoryPressureIntervalInMS();
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(time_in_ms),
               base::Bind(&MemoryPressureNotifier::CheckMemoryPressure,
                          base::Unretained(this)));
}

void MemoryPressureNotifier::StopObserving() {
  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
}

void MemoryPressureNotifier::CheckMemoryPressure() {
  MemoryPressureObserver::MemoryPressure pressure =
      GetMemoryPressureLevelFromFillLevel(
          listener_->GetDelegate()->GetUsedMemoryInPercent());
  if (current_pressure_ != pressure ||
      (pressure != MemoryPressureObserver::MEMORY_PRESSURE_LOW &&
       pressure != MemoryPressureObserver::MEMORY_PRESSURE_UNKNOWN)) {
    // If we are anything worse than |MEMORY_PRESSURE_LOW|, we notify the
    // listener.
    current_pressure_ = pressure;
    listener_->OnMemoryPressure(current_pressure_);
  }
}

MemoryPressureObserver::MemoryPressure
MemoryPressureNotifier::GetMemoryPressureLevelFromFillLevel(
    int memory_fill_level) {
  if (memory_fill_level == 0)
    return MemoryPressureObserver::MEMORY_PRESSURE_UNKNOWN;
  if (memory_fill_level < 50)
    return MemoryPressureObserver::MEMORY_PRESSURE_LOW;
  if (memory_fill_level < 75)
    return MemoryPressureObserver::MEMORY_PRESSURE_MODERATE;
  if (memory_fill_level < 90)
    return MemoryPressureObserver::MEMORY_PRESSURE_HIGH;
  return MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL;
}

}  // namespace athena
