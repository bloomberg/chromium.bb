// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_EVENT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_EVENT_OBSERVER_H_
#pragma once

#include "chrome/browser/chromeos/dbus/power_manager_client.h"

namespace chromeos {
namespace system {

// A singleton class to observe system events like wake up from sleep and
// screen unlock.
class SystemEventObserver : public PowerManagerClient::Observer {
 public:
  virtual ~SystemEventObserver();

  // PowerManagerClient::Observer overrides.
  virtual void SystemResumed() OVERRIDE;

  virtual void UnlockScreen() OVERRIDE;

  // Creates the global SystemEventObserver instance.
  static void Initialize();

  // Returns a pointer to the global SystemEventObserver instance.
  // Initialize() should already have been called.
  static SystemEventObserver* GetInstance();

  // Destroys the global SystemEventObserver Instance.
  static void Shutdown();

 private:
  SystemEventObserver();

  DISALLOW_COPY_AND_ASSIGN(SystemEventObserver);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_EVENT_OBSERVER_H_
