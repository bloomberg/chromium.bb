// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/shell_test_api.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"

namespace ash {
namespace test {

ShellTestApi::ShellTestApi(Shell* shell) : shell_(shell) {}

SystemGestureEventFilter* ShellTestApi::system_gesture_event_filter() {
  return shell_->system_gesture_filter_.get();
}

WorkspaceController* ShellTestApi::workspace_controller() {
  return shell_->GetPrimaryRootWindowController()->workspace_controller();
}

ScreenPositionController* ShellTestApi::screen_position_controller() {
  return shell_->screen_position_controller_.get();
}

AshNativeCursorManager* ShellTestApi::ash_native_cursor_manager() {
  return shell_->native_cursor_manager_;
}

DragDropController* ShellTestApi::drag_drop_controller() {
  return shell_->drag_drop_controller_.get();
}

void ShellTestApi::DisableDisplayAnimator() {
  shell_->display_configuration_controller()->ResetAnimatorForTest();
}

void ShellTestApi::SetSessionStateDelegate(
    SessionStateDelegate* session_state_delegate) {
  shell_->session_state_delegate_.reset(session_state_delegate);
}

}  // namespace test
}  // namespace ash
