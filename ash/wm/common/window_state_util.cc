// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/window_state_util.h"

#include "ash/wm/common/window_state.h"
#include "ash/wm/common/window_state_delegate.h"
#include "ash/wm/common/wm_window.h"

namespace ash {
namespace wm {

void ToggleFullScreen(wm::WindowState* window_state,
                      WindowStateDelegate* delegate) {
  // Window which cannot be maximized should not be full screen'ed.
  // It can, however, be restored if it was full screen'ed.
  bool is_fullscreen = window_state->IsFullscreen();
  if (!is_fullscreen && !window_state->CanMaximize())
    return;

  if (delegate && delegate->ToggleFullscreen(window_state))
    return;

  if (is_fullscreen) {
    window_state->Restore();
  } else {
    // Set the property to activate full screen.
    window_state->window()->SetFullscreen();
  }
}

}  // namespace wm
}  // namespace ash
