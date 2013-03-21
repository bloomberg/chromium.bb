// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELL_TEST_API_H_
#define ASH_TEST_SHELL_TEST_API_H_

#include "base/basictypes.h"

namespace views {
namespace corewm {
class InputMethodEventFilter;
}  // namespace corewm
}  // namespace views

namespace ash {
class AshNativeCursorManager;
class Shell;
class LauncherModel;

namespace internal {
class RootWindowLayoutManager;
class ScreenPositionController;
class SystemGestureEventFilter;
class WorkspaceController;
}  // namespace internal

namespace test {

// Accesses private data from a Shell for testing.
class ShellTestApi {
public:
  explicit ShellTestApi(Shell* shell);

  internal::RootWindowLayoutManager* root_window_layout();
  views::corewm::InputMethodEventFilter* input_method_event_filter();
  internal::SystemGestureEventFilter* system_gesture_event_filter();
  internal::WorkspaceController* workspace_controller();
  internal::ScreenPositionController* screen_position_controller();
  AshNativeCursorManager* ash_native_cursor_manager();
  LauncherModel* launcher_model();

  void DisableOutputConfiguratorAnimation();

 private:
  Shell* shell_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(ShellTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_SHELL_TEST_API_H_
