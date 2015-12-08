// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_DELEGATE_H_
#define COMPONENTS_EXO_POINTER_DELEGATE_H_

#include "base/time/time.h"
#include "ui/events/event_constants.h"

namespace gfx {
class Point;
class Vector2d;
}

namespace exo {
class Pointer;
class Surface;

// Handles events on pointers in context-specific ways.
class PointerDelegate {
 public:
  // Called at the top of the pointer's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnPointerDestroying(Pointer* pointer) = 0;

  // This should return true if |surface| is a valid target for this pointer.
  // E.g. the surface is owned by the same client as the pointer.
  virtual bool CanAcceptPointerEventsForSurface(Surface* surface) const = 0;

  // Called when pointer enters a new valid target surface. |location|
  // is the location of pointer relative to the origin of surface and
  // |button_flags| contains all currently pressed buttons.
  virtual void OnPointerEnter(Surface* surface,
                              const gfx::Point& location,
                              int pressed_button_flags) = 0;

  // Called when pointer leaves a valid target surface.
  virtual void OnPointerLeave(Surface* surface) = 0;

  // Called when pointer moved within the current target surface.
  virtual void OnPointerMotion(base::TimeDelta time_stamp,
                               const gfx::Point& location) = 0;

  // Called when pointer button state changed. |changed_button_flags| contains
  // all buttons that changed. |pressed| is true if buttons entered pressed
  // state.
  virtual void OnPointerButton(base::TimeDelta time_stamp,
                               int changed_button_flags,
                               bool pressed) = 0;

  // Called when pointer wheel changed. |offset| contains the direction and
  // distance of the change.
  virtual void OnPointerWheel(base::TimeDelta time_stamp,
                              const gfx::Vector2d& offset) = 0;

 protected:
  virtual ~PointerDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_DELEGATE_H_
