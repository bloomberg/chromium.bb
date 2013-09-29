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
    ui::SHOW_STATE_END == static_cast<ui::WindowShowState>(SHOW_TYPE_END),
    show_enum_mismatch);

WindowShowType ToWindowShowType(ui::WindowShowState state) {
  return static_cast<WindowShowType>(state);
}

ui::WindowShowState ToWindowShowState(WindowShowType type) {
  switch (type) {
    case SHOW_TYPE_DEFAULT:
      return ui::SHOW_STATE_DEFAULT;
    case SHOW_TYPE_NORMAL:
    case SHOW_TYPE_RIGHT_SNAPPED:
    case SHOW_TYPE_LEFT_SNAPPED:
    case SHOW_TYPE_AUTO_POSITIONED:
      return ui::SHOW_STATE_NORMAL;
    case SHOW_TYPE_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case SHOW_TYPE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case SHOW_TYPE_INACTIVE:
      return ui::SHOW_STATE_INACTIVE;
    case SHOW_TYPE_FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case SHOW_TYPE_DETACHED:
      return ui::SHOW_STATE_DETACHED;
    case SHOW_TYPE_END:
      NOTREACHED();
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

}  // namespace wm
}  // namespace ash
