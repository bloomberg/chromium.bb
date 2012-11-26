// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ROOT_POWER_MANAGER_OBSERVER_H_
#define CHROMEOS_DBUS_ROOT_POWER_MANAGER_OBSERVER_H_

#include "base/time.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class CHROMEOS_EXPORT RootPowerManagerObserver {
 public:
  // Called when the power button is pressed or released.
  virtual void OnPowerButtonEvent(bool down,
                                  const base::TimeTicks& timestamp) {}

  // Called when the device's lid is opened or closed.
  virtual void OnLidEvent(bool open, const base::TimeTicks& timestamp) {}

  // Called when the system resumes from sleep.
  virtual void OnResume(const base::TimeDelta& sleep_duration) {}

 protected:
  virtual ~RootPowerManagerObserver() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ROOT_POWER_MANAGER_OBSERVER_H_
