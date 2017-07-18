// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_test_api.h"

#include <utility>

#include "ash/palette_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"

namespace ash {

ShellTestApi::ShellTestApi() : ShellTestApi(Shell::Get()) {}

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

NativeCursorManagerAsh* ShellTestApi::native_cursor_manager_ash() {
  return shell_->native_cursor_manager_;
}

DragDropController* ShellTestApi::drag_drop_controller() {
  return shell_->drag_drop_controller_.get();
}

void ShellTestApi::SetPaletteDelegate(
    std::unique_ptr<PaletteDelegate> palette_delegate) {
  shell_->palette_delegate_ = std::move(palette_delegate);
}

}  // namespace ash
