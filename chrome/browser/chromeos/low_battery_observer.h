// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOW_BATTERY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOW_BATTERY_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"

class Profile;

namespace chromeos {

// The low battery observer displays a system notification when the battery
// is low.

class LowBatteryObserver : public PowerLibrary::Observer {
 public:
  explicit LowBatteryObserver(Profile* profile);
  virtual ~LowBatteryObserver();

 private:
  virtual void PowerChanged(PowerLibrary* object);
  virtual void SystemResumed() {}

  void Show(base::TimeDelta remaining, bool urgent);
  void Hide();

  SystemNotification notification_;
  int remaining_;  // Last displayed remaining time in minutes

  DISALLOW_COPY_AND_ASSIGN(LowBatteryObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOW_BATTERY_OBSERVER_H_
