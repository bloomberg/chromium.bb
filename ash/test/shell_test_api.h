// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELL_TEST_API_H_
#define ASH_TEST_SHELL_TEST_API_H_

#include "base/macros.h"

namespace ash {
class AshNativeCursorManager;
class DragDropController;
class MaximizeModeWindowManager;
class SessionStateDelegate;
class ScreenPositionController;
class Shell;
class SystemGestureEventFilter;
class WorkspaceController;

namespace test {

// Accesses private data from a Shell for testing.
class ShellTestApi {
 public:
  explicit ShellTestApi(Shell* shell);

  SystemGestureEventFilter* system_gesture_event_filter();
  WorkspaceController* workspace_controller();
  ScreenPositionController* screen_position_controller();
  AshNativeCursorManager* ash_native_cursor_manager();
  DragDropController* drag_drop_controller();
  MaximizeModeWindowManager* maximize_mode_window_manager();
  void DisableDisplayAnimator();

  // Set SessionStateDelegate.
  void SetSessionStateDelegate(SessionStateDelegate* session_state_delegate);

 private:
  Shell* shell_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(ShellTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_SHELL_TEST_API_H_
