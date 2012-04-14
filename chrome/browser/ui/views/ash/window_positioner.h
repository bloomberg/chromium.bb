// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_WINDOW_POSITIONER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_WINDOW_POSITIONER_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

// WindowPositioner is used by the browser to move new popups automatically to
// a usable position on the closest work area (of the active window).
class WindowPositioner {
 public:
  WindowPositioner();
  ~WindowPositioner();

  // Find a suitable screen position for a popup window and return it. The
  // passed input position is only used to retrieve the width and height.
  // The position is determined on the left / right / top / bottom first. If
  // no smart space is found, the position will follow the standard what other
  // operating systems do (default cascading style).
  gfx::Rect GetPopupPosition(const gfx::Rect& old_pos);

 protected:
  // Find a smart way to position the popup window. If there is no space this
  // function will return an empty rectangle.
  gfx::Rect SmartPopupPosition(const gfx::Rect& old_pos,
                               const gfx::Rect& work_area,
                               int grid);

  // Find the next available cascading popup position (on the given screen).
  gfx::Rect NormalPopupPosition(const gfx::Rect& old_pos,
                                const gfx::Rect& work_area);

  // Align the location to the grid / snap to the right / bottom corner.
  gfx::Rect AlignPopupPosition(const gfx::Rect &pos,
                               const gfx::Rect &work_area,
                               int grid);

  // Constants to identify the type of resize.
  static const int kBoundsChange_None;

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

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_WINDOW_POSITIONER_H_
