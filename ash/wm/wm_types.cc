// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_types.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace ash {
namespace wm {

// This is to catch the change to WindowShowState.
COMPILE_ASSERT(
    ui::SHOW_STATE_END ==
    static_cast<ui::WindowShowState>(WINDOW_STATE_TYPE_END),
    show_enum_mismatch);

WindowStateType ToWindowStateType(ui::WindowShowState state) {
  return static_cast<WindowStateType>(state);
}

ui::WindowShowState ToWindowShowState(WindowStateType type) {
  switch (type) {
    case WINDOW_STATE_TYPE_DEFAULT:
      return ui::SHOW_STATE_DEFAULT;
    case WINDOW_STATE_TYPE_NORMAL:
    case WINDOW_STATE_TYPE_RIGHT_SNAPPED:
    case WINDOW_STATE_TYPE_LEFT_SNAPPED:
    case WINDOW_STATE_TYPE_AUTO_POSITIONED:
      return ui::SHOW_STATE_NORMAL;
    case WINDOW_STATE_TYPE_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case WINDOW_STATE_TYPE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case WINDOW_STATE_TYPE_INACTIVE:
      return ui::SHOW_STATE_INACTIVE;
    case WINDOW_STATE_TYPE_FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case WINDOW_STATE_TYPE_DETACHED:
      return ui::SHOW_STATE_DETACHED;
    case WINDOW_STATE_TYPE_END:
      NOTREACHED();
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

bool IsMaximizedOrFullscreenWindowStateType(WindowStateType type) {
  return type == WINDOW_STATE_TYPE_MAXIMIZED ||
      type == WINDOW_STATE_TYPE_FULLSCREEN;
}

}  // namespace wm
}  // namespace ash
