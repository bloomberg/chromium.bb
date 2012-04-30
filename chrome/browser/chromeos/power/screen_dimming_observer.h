// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_SCREEN_DIMMING_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_SCREEN_DIMMING_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// Listens for requests to enable or disable dimming of the screen in software
// (as opposed to by adjusting a backlight).
class ScreenDimmingObserver : public PowerManagerClient::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  ScreenDimmingObserver();
  virtual ~ScreenDimmingObserver();

 private:
  // PowerManagerClient::Observer implementation.
  virtual void ScreenDimmingRequested(ScreenDimmingState state) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ScreenDimmingObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_SCREEN_DIMMING_OBSERVER_H_
