// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_POWER_MONITOR_H_
#define BASE_POWER_MONITOR_POWER_MONITOR_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/power_monitor/power_observer.h"
#include "base/synchronization/lock.h"

namespace base {

class PowerMonitorSource;

// A class used to monitor the power state change and notify the observers about
// the change event.
class BASE_EXPORT PowerMonitor {
 public:
  // Takes ownership of |source|.
  static void Initialize(scoped_ptr<PowerMonitorSource> source);
  static void ShutdownForTesting();

  // Has the PowerMonitor already been initialized?
  static bool IsInitialized();

  // Add and remove an observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  static bool AddObserver(PowerObserver* observer);
  static bool RemoveObserver(PowerObserver* observer);

  // Is the computer currently on battery power.
  static bool IsOnBatteryPower();

 private:
  friend class PowerMonitorSource;

  static Lock* GetLock();
  // Must manually acquire lock before calling any of the following functions.
  static bool IsInitializedLocked();
  static PowerMonitorSource* GetSource();
  static void NotifyPowerStateChange(bool battery_in_use);
  static void NotifySuspend();
  static void NotifyResume();

  DISALLOW_COPY_AND_ASSIGN(PowerMonitor);
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_POWER_MONITOR_H_
