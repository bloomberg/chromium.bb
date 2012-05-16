// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MAGNIFIER_MAGNIFICATION_CONTROLLER_H_
#define ASH_MAGNIFIER_MAGNIFICATION_CONTROLLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
}

namespace ash {
namespace internal {

class MagnificationController {
 public:
  MagnificationController();

  virtual ~MagnificationController() {}

  // Sets the magnification ratio. 1.0f means no magnification.
  void SetScale(float scale);
  // Returns the current magnification ratio.
  float GetScale() const { return scale_; }

  // Set the top-left point of the magnification window.
  void MoveWindow(int x, int y);
  void MoveWindow(const gfx::Point& point);
  // Returns the current top-left point of the magnification window.
  gfx::Point GetWindowPosition() const { return gfx::Point(x_, y_); }

  // Ensures that the given point/rect is inside the magnification window. If
  // not, the controller moves the window to contain the given point/rect.
  void EnsureShowRect(const gfx::Rect& rect);
  void EnsureShowPoint(const gfx::Point& point, bool animation);

 private:
  aura::RootWindow* root_window_;

  // Current scale, position of the magnification window.
  float scale_;
  int x_;
  int y_;

  // Returns the rect of the magnification window.
  gfx::Rect GetWindowRect();
  // Moves the magnification window to new scale and position. This method
  // should be called internally just after the scale and/or the position are
  // changed.
  void RedrawScreen(bool animation);

  DISALLOW_COPY_AND_ASSIGN(MagnificationController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_MAGNIFIER_MAGNIFICATION_CONTROLLER_H_
