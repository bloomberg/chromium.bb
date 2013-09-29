// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_TYPES_H_
#define ASH_WM_WM_TYPES_H_

#include "ui/base/ui_base_types.h"

namespace ash {
namespace wm {

// This enum defines both common show state copied from
// ui::WindowShowState as well as new states intoruduced in ash.
// The separate enum is defined here because we don't want to leak
// these type to ui/base until they're stable and we know for sure
// that they'll persist over time.
enum WindowShowType {
  // Common state
  SHOW_TYPE_DEFAULT = 0,

  // Normal represents a state where the position/size has been
  // specified by a use.
  SHOW_TYPE_NORMAL,
  SHOW_TYPE_MINIMIZED,
  SHOW_TYPE_MAXIMIZED,
  SHOW_TYPE_INACTIVE,
  SHOW_TYPE_FULLSCREEN,
  SHOW_TYPE_DETACHED,
  SHOW_TYPE_END,  // to avoid using SHOW_STATE_END

  // Ash specific states:

  SHOW_TYPE_LEFT_SNAPPED,
  SHOW_TYPE_RIGHT_SNAPPED,

  // A window is in this state when it is automatically placed and
  // sized by the window manager. (it's newly opened, or pushed to the side
  // due to new window, for example).
  SHOW_TYPE_AUTO_POSITIONED,
};

// Utility functions to convert WindowShowType <-> ui::WindowShowState.
// Note: LEFT/RIGHT MAXIMIZED, AUTO_POSITIONED type will be lost when
// converting to ui::WindowShowState.
WindowShowType ToWindowShowType(ui::WindowShowState state);
ui::WindowShowState ToWindowShowState(WindowShowType type);

}  // namespace
}  // namespace

#endif  // ASH_WM_WM_TYPES_H_
