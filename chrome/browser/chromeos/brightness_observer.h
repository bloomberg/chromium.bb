// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BRIGHTNESS_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_BRIGHTNESS_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"

namespace chromeos {

// This observer displays a bubble at the bottom of the screen showing the
// current brightness level whenever the user changes it.
class BrightnessObserver : public PowerManagerClient::Observer {
 public:
  BrightnessObserver() {}
  virtual ~BrightnessObserver() {}

 private:
  // PowerManagerClient::Observer implementation.
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BrightnessObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BRIGHTNESS_OBSERVER_H_
