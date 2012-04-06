// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_SCREEN_LOCK_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_SCREEN_LOCK_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// A class to observe screen lock events and dispatch onScreenUnlocked extension
// API events.
class ScreenLockObserver : public PowerManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  ScreenLockObserver();
  virtual ~ScreenLockObserver();

  // PowerManagerClient::Observer override.
  virtual void UnlockScreen() OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenLockObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_SCREEN_LOCK_OBSERVER_H_
