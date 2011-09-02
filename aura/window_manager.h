// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_WINDOW_MANAGER_H_
#define AURA_WINDOW_MANAGER_H_
#pragma once

#include "base/logging.h"
#include "ui/gfx/point.h"

namespace aura {

class Window;
class MouseEvent;

// An object that filters events sent to an owner window, potentially performing
// adjustments to the window's position, size and z-index.
//
// TODO(beng): Make this into an interface so we can have specializations for
//             different types of Window and product.
class WindowManager {
 public:
  explicit WindowManager(Window* owner);
  ~WindowManager();

  // Try to handle |event| (before the owner's delegate gets a chance to).
  // Returns true if the event was handled by the WindowManager and should not
  // be forwarded to the owner's delegate.
  bool OnMouseEvent(MouseEvent* event);

 private:
  // Moves the owner window and all of its parents to the front of their
  // respective z-orders.
  void MoveWindowToFront();

  Window* owner_;
  gfx::Point mouse_down_offset_;
  gfx::Point window_location_;
  int window_component_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace aura

#endif  // AURA_WINDOW_MANAGER_H_
