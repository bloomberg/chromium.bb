// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHROMEOS_MEMORY_PRESSURE_OBSERVER_CHROMEOS_H_
#define BASE_CHROMEOS_MEMORY_PRESSURE_OBSERVER_CHROMEOS_H_

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/timer/timer.h"

namespace base {

////////////////////////////////////////////////////////////////////////////////
// MemoryPressureObserverChromeOS
//
// A class to handle the observation of our free memory. It notifies the
// MemoryPressureListener of memory fill level changes, so that it can take
// action to reduce memory resources accordingly.
//
class BASE_EXPORT MemoryPressureObserverChromeOS {
 public:
  MemoryPressureObserverChromeOS();
  ~MemoryPressureObserverChromeOS();

  // Get the current memory pressure level.
  base::MemoryPressureListener::MemoryPressureLevel GetCurrentPressureLevel() {
    return current_memory_pressure_level_;
  }

 private:
  // Starts observing the memory fill level.
  // Calls to StartObserving should always be matched with calls to
  // StopObserving.
  void StartObserving();

  // Stop observing the memory fill level.
  // May be safely called if StartObserving has not been called.
  void StopObserving();

  // The function which gets periodically be called to check any changes in the
  // memory pressure.
  void CheckMemoryPressure();

  // The current memory pressure.
  base::MemoryPressureListener::MemoryPressureLevel
      current_memory_pressure_level_;

  // A periodic timer to check for resource pressure changes. This will get
  // replaced by a kernel triggered event system (see crbug.com/381196).
  base::RepeatingTimer<MemoryPressureObserverChromeOS> timer_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureObserverChromeOS);
};

}  // namespace base

#endif  // BASE_CHROMEOS_MEMORY_PRESSURE_OBSERVER_CHROMEOS_H_
