// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/wm_types.h"

#include "base/logging.h"

namespace ash {
namespace wm {

// This is to catch the change to WindowShowState.
static_assert(ui::SHOW_STATE_END ==
                  static_cast<ui::WindowShowState>(WINDOW_STATE_TYPE_END),
              "show enum mismatch");

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

    // TODO(afakhry): Remove Docked Windows in M58.
    case WINDOW_STATE_TYPE_DOCKED:
      return ui::SHOW_STATE_DOCKED;
    case WINDOW_STATE_TYPE_MINIMIZED:
    case WINDOW_STATE_TYPE_DOCKED_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case WINDOW_STATE_TYPE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case WINDOW_STATE_TYPE_INACTIVE:
      return ui::SHOW_STATE_INACTIVE;
    case WINDOW_STATE_TYPE_FULLSCREEN:
    case WINDOW_STATE_TYPE_PINNED:
    case WINDOW_STATE_TYPE_TRUSTED_PINNED:
      return ui::SHOW_STATE_FULLSCREEN;
    case WINDOW_STATE_TYPE_END:
      NOTREACHED();
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

bool IsMaximizedOrFullscreenOrPinnedWindowStateType(WindowStateType type) {
  return type == WINDOW_STATE_TYPE_MAXIMIZED ||
         type == WINDOW_STATE_TYPE_FULLSCREEN ||
         type == WINDOW_STATE_TYPE_PINNED ||
         type == WINDOW_STATE_TYPE_TRUSTED_PINNED;
}

}  // namespace wm
}  // namespace ash
