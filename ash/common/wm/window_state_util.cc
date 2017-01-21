// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/window_state_util.h"

#include "ash/common/wm/window_state.h"
#include "ash/common/wm/window_state_delegate.h"
#include "ash/common/wm_window.h"

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
  window_state->window()->SetFullscreen(!is_fullscreen);
}

}  // namespace wm
}  // namespace ash
