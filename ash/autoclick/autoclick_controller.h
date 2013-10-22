// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H
#define ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H

#include "ash/ash_export.h"

namespace ash {

// Controls the autoclick a11y feature in ash.
// If enabled, we will automatically send a click event a short time after
// the mouse had been at rest.
class ASH_EXPORT AutoclickController {
 public:
  virtual ~AutoclickController() {}

  // Set whether autoclicking is enabled.
  virtual void SetEnabled(bool enabled) = 0;

  // Returns true if autoclicking is enabled.
  virtual bool IsEnabled() const = 0;

  // Set the time to wait in milliseconds from when the mouse stops moving
  // to when the autoclick event is sent.
  virtual void SetAutoclickDelay(int delay_ms) = 0;

  // Returns the autoclick delay in milliseconds.
  virtual int GetAutoclickDelay() const = 0;

  static AutoclickController* CreateInstance();

  // The default wait time between last mouse movement and sending
  // the autoclick.
  static const int kDefaultAutoclickDelayMs;

 protected:
  AutoclickController() {}
};

}  // namespace ash

#endif  // ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H
