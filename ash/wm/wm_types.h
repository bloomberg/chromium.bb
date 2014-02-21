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

// Set of operations that can change the window's state type and bounds.
enum WMEvent {
  // Following events are the request to become corresponding state.
  // Note that this does not mean the window will be in corresponding
  // state and the request may not be fullfilled.

  // NORMAL is used as a restore operation with a few exceptions.
  NORMAL,
  MAXIMIZE,
  MINIMIZE,
  FULLSCREEN,
  // TODO(oshima): Consolidate these two events.
  SNAP_LEFT,
  SNAP_RIGHT,

  // Following events are compond events which may lead to different
  // states depending on the current state.

  // A user requested to toggle maximized state by double clicking window
  // header.
  TOGGLE_MAXIMIZE_CAPTION,

  // A user requested to toggle maximized state using shortcut.
  TOGGLE_MAXIMIZE,

  // A user requested to toggle vertical maximize by double clicking
  // top/bottom edge.
  TOGGLE_VERTICAL_MAXIMIZE,

  // A user requested to toggle horizontal maximize by double clicking
  // left/right edge.
  TOGGLE_HORIZONTAL_MAXIMIZE,

  // A user requested to toggle fullscreen state.
  TOGGLE_FULLSCREEN,

  // TODO(oshima): Investigate if this can be removed from ash.
  // Widget requested to show in inactive state.
  SHOW_INACTIVE,

  // Following events are generated when the workspace envrionment has changed.
  // The window's state type will not be changed by these events.

  // The window is added to the workspace, either as a new window, due to
  // display disconnection or dragging.
  ADDED_TO_WORKSPACE,

  // Bounds of the display has changed.
  DISPLAY_BOUNDS_CHANGED,

  // Bounds of the work area has changed. This will not occur when the work
  // area has changed as a result of DISPLAY_BOUNDS_CHANGED.
  WORKAREA_BOUNDS_CHANGED,
};

// Utility functions to convert WindowShowType <-> ui::WindowShowState.
// Note: LEFT/RIGHT MAXIMIZED, AUTO_POSITIONED type will be lost when
// converting to ui::WindowShowState.
ASH_EXPORT WindowShowType ToWindowShowType(ui::WindowShowState state);
ASH_EXPORT ui::WindowShowState ToWindowShowState(WindowShowType type);

// Returns true if |type| is SHOW_TYPE_MAXIMIZED or SHOW_TYPE_FULLSCREEN.
ASH_EXPORT bool IsMaximizedOrFullscreenWindowShowType(WindowShowType type);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WM_TYPES_H_
