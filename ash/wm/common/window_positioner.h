// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WINDOW_POSITIONER_H_
#define ASH_WM_COMMON_WINDOW_POSITIONER_H_

#include "ash/wm/common/ash_wm_common_export.h"
#include "base/macros.h"
#include "ui/base/ui_base_types.h"

namespace display {
class Display;
}

namespace gfx {
class Rect;
}

namespace ash {
namespace wm {
class WmGlobals;
class WmWindow;
}

namespace test {
class WindowPositionerTest;
}

// WindowPositioner is used by the browser to move new popups automatically to
// a usable position on the closest work area (of the active window).
class ASH_WM_COMMON_EXPORT WindowPositioner {
 public:
  // When the screen resolution width is smaller then this size, The algorithm
  // will default to maximized.
  static int GetForceMaximizedWidthLimit();

  // The number of pixels which are kept free top, left and right when a window
  // gets positioned to its default location.
  static const int kDesktopBorderSize;

  // Maximum width of a window even if there is more room on the desktop.
  static const int kMaximumWindowWidth;

  // Computes and returns the bounds and show state for new window
  // based on the parameter passed AND existing windows. |window| is
  // the one this function will generate a bounds for and used to
  // exclude the self window in making decision how to position the
  // window. |window| can be (and in most case) NULL.
  // |is_saved_bounds| indicates the |bounds_in_out| is the saved
  // bounds.
  static void GetBoundsAndShowStateForNewWindow(
      const wm::WmWindow* new_window,
      bool is_saved_bounds,
      ui::WindowShowState show_state_in,
      gfx::Rect* bounds_in_out,
      ui::WindowShowState* show_state_out);

  // Returns the default bounds for a window to be created in the |display|.
  static gfx::Rect GetDefaultWindowBounds(const display::Display& display);

  // Check if after removal or hide of the given |removed_window| an
  // automated desktop location management can be performed and
  // rearrange accordingly.
  static void RearrangeVisibleWindowOnHideOrRemove(
      const wm::WmWindow* removed_window);

  // Turn the automatic positioning logic temporarily off. Returns the previous
  // state.
  static bool DisableAutoPositioning(bool ignore);

  // Check if after insertion or showing of the given |added_window|
  // an automated desktop location management can be performed and
  // rearrange accordingly.
  static void RearrangeVisibleWindowOnShow(wm::WmWindow* added_window);

  explicit WindowPositioner(wm::WmGlobals* globals);
  ~WindowPositioner();

  // Find a suitable screen position for a popup window and return it. The
  // passed input position is only used to retrieve the width and height.
  // The position is determined on the left / right / top / bottom first. If
  // no smart space is found, the position will follow the standard what other
  // operating systems do (default cascading style).
  gfx::Rect GetPopupPosition(const gfx::Rect& old_pos);

  // Accessor to set a flag indicating whether the first window in ASH should
  // be maximized.
  static void SetMaximizeFirstWindow(bool maximize);

 protected:
  friend class test::WindowPositionerTest;

  // Find a smart way to position the popup window. If there is no space this
  // function will return an empty rectangle.
  gfx::Rect SmartPopupPosition(const gfx::Rect& old_pos,
                               const gfx::Rect& work_area,
                               int grid);

  // Find the next available cascading popup position (on the given screen).
  gfx::Rect NormalPopupPosition(const gfx::Rect& old_pos,
                                const gfx::Rect& work_area);

  // Align the location to the grid / snap to the right / bottom corner.
  gfx::Rect AlignPopupPosition(const gfx::Rect& pos,
                               const gfx::Rect& work_area,
                               int grid);

  // Constant exposed for unittest.
  static const int kMinimumWindowOffset;

  wm::WmGlobals* globals_;

  // The offset in X and Y for the next popup which opens.
  int pop_position_offset_increment_x;
  int pop_position_offset_increment_y;

  // The position on the screen for the first popup which gets shown if no
  // empty space can be found.
  int popup_position_offset_from_screen_corner_x;
  int popup_position_offset_from_screen_corner_y;

  // The last used position.
  int last_popup_position_x_;
  int last_popup_position_y_;

  DISALLOW_COPY_AND_ASSIGN(WindowPositioner);
};

}  // namespace ash

#endif  // ASH_WM_COMMON_WINDOW_POSITIONER_H_
