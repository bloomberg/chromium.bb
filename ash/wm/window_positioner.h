// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_POSITIONER_H_
#define ASH_WM_WINDOW_POSITIONER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
class Size;
}

namespace ash {

// WindowPositioner is used by the browser to move new popups automatically to
// a usable position on the closest work area (of the active window).
class ASH_EXPORT WindowPositioner {
 public:
  // When the screen resolution width is smaller then this size, The algorithm
  // will default to maximized.
  static int GetForceMaximizedWidthLimit();

  // Computes and returns the bounds and show state for new window
  // based on the parameter passed AND existing windows. |is_saved_bounds|
  // indicates the |bounds_in_out| is the saved bounds.
  static void GetBoundsAndShowStateForNewWindow(
      bool is_saved_bounds,
      ui::WindowShowState show_state_in,
      gfx::Rect* bounds_in_out,
      ui::WindowShowState* show_state_out);

  // Check if after removal or hide of the given |removed_window| an
  // automated desktop location management can be performed and
  // rearrange accordingly.
  static void RearrangeVisibleWindowOnHideOrRemove(
      const aura::Window* removed_window);

  // Turn the automatic positioning logic temporarily off. Returns the previous
  // state.
  static bool DisableAutoPositioning(bool ignore);

  // Check if after insertion or showing of the given |added_window|
  // an automated desktop location management can be performed and
  // rearrange accordingly.
  static void RearrangeVisibleWindowOnShow(aura::Window* added_window);

  WindowPositioner();
  ~WindowPositioner();

  // Find a suitable screen position for a popup window and return it.
  // The position is determined on the left / right / top / bottom first. If
  // no smart space is found, the position will follow the standard what other
  // operating systems do (default cascading style).
  gfx::Rect GetPopupPosition(const gfx::Size& popup_size);

 protected:
  friend class WindowPositionerTest;

  // Find a smart way to position the popup window. If there is no space this
  // function will return an empty rectangle.
  static gfx::Rect SmartPopupPosition(const gfx::Size& popup_size,
                                      const gfx::Rect& work_area);

  // Find the next available cascading popup position (on the given screen).
  gfx::Rect NormalPopupPosition(const gfx::Size& popup_size,
                                const gfx::Rect& work_area);

  // Align the location to the grid / snap to the right / bottom corner.
  static gfx::Rect AlignPopupPosition(const gfx::Rect& pos,
                                      const gfx::Rect& work_area);

  // The grid size in DIPs used to align popup windows.
  static constexpr int kPopupGridSize = 32;

  // The offset in X and Y for the next popup which opens.
  static constexpr int kNextPopupOffset = 32;

  // The position on the screen for the first popup which gets shown if no
  // empty space can be found.
  static constexpr int kFirstPopupOffset = 32;

  // The last used position.
  // TODO(jamescook): These seem more like *next* popup position.
  int last_popup_position_x_ = 0;
  int last_popup_position_y_ = 0;
  bool has_last_popup_position_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowPositioner);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_POSITIONER_H_
