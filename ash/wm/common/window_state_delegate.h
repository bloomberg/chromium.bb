// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WINDOW_STATE_DELEGATE_H_
#define ASH_WM_COMMON_WINDOW_STATE_DELEGATE_H_

#include "ash/wm/common/ash_wm_common_export.h"
#include "base/macros.h"

namespace ash {
namespace wm {
class WindowState;

class ASH_WM_COMMON_EXPORT WindowStateDelegate {
 public:
  WindowStateDelegate();
  virtual ~WindowStateDelegate();

  // Invoked when the user uses Shift+F4/F4 to toggle the window fullscreen
  // state. If the window is not fullscreen and the window supports immersive
  // fullscreen ToggleFullscreen() should put the window into immersive
  // fullscreen instead of the default fullscreen type. The caller
  // (ash::wm::WindowState) falls backs to the default implementation if this
  // returns false.
  virtual bool ToggleFullscreen(WindowState* window_state);

  // Invoked when workspace fullscreen state changes and a window may need to
  // reassert its always on top state. Returns true if delegate has handled this
  // and no additional work is needed, false otherwise.
  virtual bool RestoreAlwaysOnTop(WindowState* window_state);

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowStateDelegate);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WINDOW_STATE_DELEGATE_H_
