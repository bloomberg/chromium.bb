// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_SYSTEM_EVENT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_SYSTEM_EVENT_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

// Dispatches extension events in response to system events.
class ExtensionSystemEventObserver : public PowerManagerClient::Observer,
                                     public SessionManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  ExtensionSystemEventObserver();
  virtual ~ExtensionSystemEventObserver();

  // PowerManagerClient::Observer overrides:
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE;
  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE;

  // SessionManagerClient::Observer override.
  virtual void ScreenIsUnlocked() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionSystemEventObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_SYSTEM_EVENT_OBSERVER_H_
