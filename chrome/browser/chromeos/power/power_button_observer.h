// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_POWER_BUTTON_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_POWER_BUTTON_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

// Listens for power button, login, and screen lock events and passes them to
// the Aura shell's PowerButtonController class.
class PowerButtonObserver : public content::NotificationObserver,
                            public PowerManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  PowerButtonObserver();
  virtual ~PowerButtonObserver();

 private:
  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // PowerManagerClient::Observer implementation.
  virtual void PowerButtonStateChanged(
      bool down, const base::TimeTicks& timestamp) OVERRIDE;
  virtual void LockButtonStateChanged(
      bool down, const base::TimeTicks& timestamp) OVERRIDE;
  virtual void LockScreen() OVERRIDE;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_POWER_BUTTON_OBSERVER_H_
