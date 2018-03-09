// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_SYSTEM_EVENT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_SYSTEM_EVENT_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

// Dispatches extension events in response to system events.
class ExtensionSystemEventObserver
    : public PowerManagerClient::Observer,
      public session_manager::SessionManagerObserver {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  ExtensionSystemEventObserver();
  ~ExtensionSystemEventObserver() override;

  // PowerManagerClient::Observer overrides:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // session_manager::SessionManagerObserver override:
  void OnSessionStateChanged() override;

 private:
  bool screen_locked_ = false;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSystemEventObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_SYSTEM_EVENT_OBSERVER_H_
