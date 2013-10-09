// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_SUSPEND_OBSERVER_H_
#define ASH_SYSTEM_CHROMEOS_POWER_SUSPEND_OBSERVER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"

namespace ash {
namespace internal {

// A class to observe suspend events.
class SuspendObserver : public chromeos::PowerManagerClient::Observer,
                        public chromeos::SessionManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  SuspendObserver();
  virtual ~SuspendObserver();

  // chromeos::PowerManagerClient::Observer override.
  virtual void SuspendImminent() OVERRIDE;

  // chromeos::SessionManagerClient::Observer overrides.
  virtual void ScreenIsLocked() OVERRIDE;
  virtual void ScreenIsUnlocked() OVERRIDE;

 private:
  chromeos::PowerManagerClient* power_client_;  // not owned
  chromeos::SessionManagerClient* session_client_;  // not owned

  // Is the screen currently locked?
  bool screen_locked_;

  // If set, called when the lock screen has been shown to confirm that the
  // system is ready to be suspended.
  base::Closure screen_lock_callback_;

  DISALLOW_COPY_AND_ASSIGN(SuspendObserver);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_SUSPEND_OBSERVER_H_
