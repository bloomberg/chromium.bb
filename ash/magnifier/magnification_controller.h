// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MAGNIFIER_MAGNIFICATION_CONTROLLER_H_
#define ASH_MAGNIFIER_MAGNIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ash {

class ASH_EXPORT MagnificationController {
 public:
  enum ScrollDirection {
    SCROLL_NONE,
    SCROLL_LEFT,
    SCROLL_RIGHT,
    SCROLL_UP,
    SCROLL_DOWN
  };

  virtual ~MagnificationController() {}

  // Factor of magnification scale. For example, when this value is 1.189, scale
  // value will be changed x1.000, x1.189, x1.414, x1.681, x2.000, ...
  // Note: this value is 2.0 ^ (1 / 4).
  static constexpr float kMagnificationScaleFactor = 1.18920712f;

  // Creates a new MagnificationController. The caller takes ownership of the
  // returned object.
  static MagnificationController* CreateInstance();

  // Enables (or disables if |enabled| is false) screen magnifier feature.
  virtual void SetEnabled(bool enabled) = 0;

  // Returns if the screen magnifier is enabled or not.
  virtual bool IsEnabled() const = 0;

  // Enables or disables the feature for keeping the text input focus centered.
  virtual void SetKeepFocusCentered(bool keep_focus_centered) = 0;

  // Returns true if magnifier will keep the focus centered in screen for text
  // input.
  virtual bool KeepFocusCentered() const = 0;

  // Sets the magnification ratio. 1.0f means no magnification.
  virtual void SetScale(float scale, bool animate) = 0;
  // Returns the current magnification ratio.
  virtual float GetScale() const = 0;

  // Set the top-left point of the magnification window.
  virtual void MoveWindow(int x, int y, bool animate) = 0;
  virtual void MoveWindow(const gfx::Point& point, bool animate) = 0;
  // Returns the current top-left point of the magnification window.
  virtual gfx::Point GetWindowPosition() const = 0;

  virtual void SetScrollDirection(ScrollDirection direction) = 0;

  // Returns the view port(i.e. the current visible window)'s Rect in root
  // window coordinates.
  virtual gfx::Rect GetViewportRect() const = 0;

  // Follows the focus on web page for non-editable controls.
  virtual void HandleFocusedNodeChanged(
      bool is_editable_node,
      const gfx::Rect& node_bounds_in_screen) = 0;

  // Returns |point_of_interest_| in MagnificationControllerImpl. This is
  // the internal variable to stores the last mouse cursor (or last touched)
  // location. This method is only for test purpose.
  virtual gfx::Point GetPointOfInterestForTesting() = 0;

  // Returns true if magnifier is still on animation for moving viewport.
  // This is only used for testing purpose.
  virtual bool IsOnAnimationForTesting() const = 0;

  // Disables the delay for moving magnifier window in testing mode.
  virtual void DisableMoveMagnifierDelayForTesting() = 0;

  // Switch the magnified root window to |new_root_window|. This does following:
  //  - Unzoom the current root_window.
  //  - Zoom the given new root_window |new_root_window|.
  //  - Switch the target window from current window to |new_root_window|.
  virtual void SwitchTargetRootWindow(aura::Window* new_root_window,
                                      bool redraw_original_root) = 0;

 protected:
  MagnificationController() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MagnificationController);
};

}  // namespace ash

#endif  // ASH_MAGNIFIER_MAGNIFICATION_CONTROLLER_H_
