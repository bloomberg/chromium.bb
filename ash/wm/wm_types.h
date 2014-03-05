// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_TYPES_H_
#define ASH_WM_WM_TYPES_H_

#include "ash/ash_export.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace wm {

// This enum defines both common show state copied from
// ui::WindowShowState as well as new states intoruduced in ash.
// The separate enum is defined here because we don't want to leak
// these type to ui/base until they're stable and we know for sure
// that they'll persist over time.
enum WindowStateType {
  // Common state
  WINDOW_STATE_TYPE_DEFAULT = 0,

  // Normal represents a state where the position/size has been
  // specified by a use.
  WINDOW_STATE_TYPE_NORMAL,
  WINDOW_STATE_TYPE_MINIMIZED,
  WINDOW_STATE_TYPE_MAXIMIZED,
  WINDOW_STATE_TYPE_INACTIVE,
  WINDOW_STATE_TYPE_FULLSCREEN,
  WINDOW_STATE_TYPE_DETACHED,
  WINDOW_STATE_TYPE_END,  // to avoid using SHOW_STATE_END

  // Ash specific states:

  WINDOW_STATE_TYPE_LEFT_SNAPPED,
  WINDOW_STATE_TYPE_RIGHT_SNAPPED,

  // A window is in this state when it is automatically placed and
  // sized by the window manager. (it's newly opened, or pushed to the side
  // due to new window, for example).
  WINDOW_STATE_TYPE_AUTO_POSITIONED,
};

// Utility functions to convert WindowStateType <-> ui::WindowShowState.
// Note: LEFT/RIGHT MAXIMIZED, AUTO_POSITIONED type will be lost when
// converting to ui::WindowShowState.
ASH_EXPORT WindowStateType ToWindowStateType(ui::WindowShowState state);
ASH_EXPORT ui::WindowShowState ToWindowShowState(WindowStateType type);

// Returns true if |type| is WINDOW_STATE_TYPE_MAXIMIZED or
// WINDOW_STATE_TYPE_FULLSCREEN.
ASH_EXPORT bool IsMaximizedOrFullscreenWindowStateType(WindowStateType type);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WM_TYPES_H_
