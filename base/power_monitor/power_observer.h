// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_POWER_OBSERVER_H_
#define BASE_POWER_MONITOR_POWER_OBSERVER_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"

namespace base {

class BASE_EXPORT PowerObserver {
  friend class PowerMonitor;

 public:
  PowerObserver();

  // Notification of a change in power status of the computer, such
  // as from switching between battery and A/C power.
  virtual void OnPowerStateChange(bool on_battery_power) {}

  // Notification that the system is suspending.
  virtual void OnSuspend() {}

  // Notification that the system is resuming.
  virtual void OnResume() {}

 protected:
  virtual ~PowerObserver();

 private:
  // Allows PowerMonitor to ensure this observer is added and removed
  // from the same thread, which is required by the ObserverListThreadSafe.
  scoped_ptr<base::ThreadChecker> power_monitor_thread_checker_;
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_POWER_OBSERVER_H_
