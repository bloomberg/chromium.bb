// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHROMEOS_MEMORY_PRESSURE_OBSERVER_CHROMEOS_H_
#define BASE_CHROMEOS_MEMORY_PRESSURE_OBSERVER_CHROMEOS_H_

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace base {

class TestMemoryPressureObserver;

////////////////////////////////////////////////////////////////////////////////
// MemoryPressureObserverChromeOS
//
// A class to handle the observation of our free memory. It notifies the
// MemoryPressureListener of memory fill level changes, so that it can take
// action to reduce memory resources accordingly.
//
class BASE_EXPORT MemoryPressureObserverChromeOS {
 public:
  using GetUsedMemoryInPercentCallback = int (*)();

  MemoryPressureObserverChromeOS();
  virtual ~MemoryPressureObserverChromeOS();

  // Redo the memory pressure calculation soon and call again if a critical
  // memory pressure prevails. Note that this call will trigger an asynchronous
  // action which gives the system time to release memory back into the pool.
  void ScheduleEarlyCheck();

  // Get the current memory pressure level.
  base::MemoryPressureListener::MemoryPressureLevel GetCurrentPressureLevel() {
    return current_memory_pressure_level_;
  }

 private:
  friend TestMemoryPressureObserver;
  // Starts observing the memory fill level.
  // Calls to StartObserving should always be matched with calls to
  // StopObserving.
  void StartObserving();

  // Stop observing the memory fill level.
  // May be safely called if StartObserving has not been called.
  void StopObserving();

  // The function which gets periodically called to check any changes in the
  // memory pressure. It will report pressure changes as well as continuous
  // critical pressure levels.
  void CheckMemoryPressure();

  // Get the memory pressure in percent (virtual for testing).
  virtual int GetUsedMemoryInPercent();

  // The current memory pressure.
  base::MemoryPressureListener::MemoryPressureLevel
      current_memory_pressure_level_;

  // A periodic timer to check for resource pressure changes. This will get
  // replaced by a kernel triggered event system (see crbug.com/381196).
  base::RepeatingTimer<MemoryPressureObserverChromeOS> timer_;

  // To slow down the amount of moderate pressure event calls, this counter
  // gets used to count the number of events since the last event occured.
  int moderate_pressure_repeat_count_;

  base::WeakPtrFactory<MemoryPressureObserverChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureObserverChromeOS);
};

}  // namespace base

#endif  // BASE_CHROMEOS_MEMORY_PRESSURE_OBSERVER_CHROMEOS_H_
