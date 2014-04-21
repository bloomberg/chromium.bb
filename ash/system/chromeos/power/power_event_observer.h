// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_POWER_EVENT_OBSERVER_H_
#define ASH_SYSTEM_CHROMEOS_POWER_POWER_EVENT_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"

namespace ash {

// A class that observes power-management-related events.
class ASH_EXPORT PowerEventObserver
    : public chromeos::PowerManagerClient::Observer,
      public chromeos::SessionManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  PowerEventObserver();
  virtual ~PowerEventObserver();

  // chromeos::PowerManagerClient::Observer overrides:
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE;
  virtual void SuspendImminent() OVERRIDE;
  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE;

  // chromeos::SessionManagerClient::Observer overrides.
  virtual void ScreenIsLocked() OVERRIDE;
  virtual void ScreenIsUnlocked() OVERRIDE;

  // Is the screen currently locked?
  bool screen_locked_;

  // If set, called when the lock screen has been shown to confirm that the
  // system is ready to be suspended.
  base::Closure screen_lock_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerEventObserver);
};

}  // namespace chromeos

#endif  // ASH_SYSTEM_CHROMEOS_POWER_POWER_EVENT_OBSERVER_H_
