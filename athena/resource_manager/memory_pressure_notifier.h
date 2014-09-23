// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_RESOURCE_MANAGER_MEMORY_PRESSURE_NOTIFIER_H_
#define ATHENA_RESOURCE_MANAGER_MEMORY_PRESSURE_NOTIFIER_H_

#include "athena/athena_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"

namespace athena {

class MemoryPressureNotifierImpl;
class ResourceManagerDelegate;

////////////////////////////////////////////////////////////////////////////////
// MemoryPressureObserver
//
// This observer gets informed once about a |MEMORY_PRESSURE_LOW|. When the
// pressure exceeds, the observer will get polled until |MEMORY_PRESSURE_LOW| is
// reached again to counter memory consumption.
class MemoryPressureObserver {
 public:
  MemoryPressureObserver() {}
  virtual ~MemoryPressureObserver() {}

  // The reported memory pressure. Note: The value is intentionally abstracted
  // since the real amount of free memory is only estimated (due to e.g. zram).
  // Note: The bigger the index of the pressure level, the more resources are
  // in use.
  enum MemoryPressure {
    MEMORY_PRESSURE_UNKNOWN = 0,   // The memory pressure cannot be determined.
    MEMORY_PRESSURE_LOW,           // Single call if fill level is below 50%.
    MEMORY_PRESSURE_MODERATE,      // Polled for fill level of ~50 .. 75%.
    MEMORY_PRESSURE_HIGH,          // Polled for fill level of ~75% .. 90%.
    MEMORY_PRESSURE_CRITICAL,      // Polled for fill level of above ~90%.
  };
  // The observer.
  virtual void OnMemoryPressure(MemoryPressure pressure) = 0;

  // OS system interface functions. The delegate remains owned by the Observer.
  virtual ResourceManagerDelegate* GetDelegate() = 0;
};


////////////////////////////////////////////////////////////////////////////////
// MemoryPressureNotifier
//
// Class to handle the observation of our free memory. It notifies the owner of
// memory fill level changes, so that it can take action to reduce memory by
// reducing active activities.
//
// The observer will use 3 different fill levels: 50% full, 75% full and 90%
// full.
class ATHENA_EXPORT MemoryPressureNotifier {
 public:
  // The creator gets the |listener| object. Note that the ownership of the
  // listener object remains with the creator.
  explicit MemoryPressureNotifier(MemoryPressureObserver* listener);
  ~MemoryPressureNotifier();

  // Stop observing the memory fill level.
  // May be safely called if StartObserving has not been called.
  void StopObserving();

 private:
  // Starts observing the memory fill level.
  // Calls to StartObserving should always be matched with calls to
  // StopObserving.
  void StartObserving();

  // The function which gets periodically be called to check any changes in the
  // memory pressure.
  void CheckMemoryPressure();

  // Converts free percent of memory into a memory pressure value.
  MemoryPressureObserver::MemoryPressure GetMemoryPressureLevelFromFillLevel(
      int memory_fill_level);

  base::RepeatingTimer<MemoryPressureNotifier> timer_;

  // The listener which needs to be informed about memory pressure.
  MemoryPressureObserver* listener_;

  // Our current memory pressure.
  MemoryPressureObserver::MemoryPressure current_pressure_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureNotifier);
};

}  // namespace athena

#endif  // ATHENA_RESOURCE_MANAGER_MEMORY_PRESSURE_NOTIFIER_H_
