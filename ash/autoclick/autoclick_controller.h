// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H
#define ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

// Controls the autoclick a11y feature in ash.
// If enabled, we will automatically send a click event a short time after
// the mouse had been at rest.
class ASH_EXPORT AutoclickController {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Called when an autoclick gesture begins.
    virtual void StartGesture(base::TimeDelta duration,
                              const gfx::Point& center_point_in_screen) = 0;

    // Called when the gesture has ended, either because the mouse moved or
    // because the autoclick completed.
    virtual void StopGesture() = 0;

    // Called when the autoclick target has changed.
    virtual void SetGestureCenter(const gfx::Point& center_point_in_screen) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  virtual ~AutoclickController() {}

  // Set the delegate to work with.
  virtual void SetDelegate(std::unique_ptr<Delegate> delegate) = 0;

  // Set whether autoclicking is enabled.
  virtual void SetEnabled(bool enabled) = 0;

  // Returns true if autoclicking is enabled.
  virtual bool IsEnabled() const = 0;

  // Set the time to wait in milliseconds from when the mouse stops moving
  // to when the autoclick event is sent.
  virtual void SetAutoclickDelay(base::TimeDelta delay) = 0;

  // Returns the autoclick delay in milliseconds.
  virtual base::TimeDelta GetAutoclickDelay() const = 0;

  static AutoclickController* CreateInstance();

  // The default wait time between last mouse movement and sending
  // the autoclick.
  static const int kDefaultAutoclickDelayMs;

  // Gets the default wait time as a base::TimeDelta object.
  static base::TimeDelta GetDefaultAutoclickDelay();

 protected:
  AutoclickController() {}
};

}  // namespace ash

#endif  // ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H
