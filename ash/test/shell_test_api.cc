// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/shell_test_api.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shell.h"

#if defined(OS_CHROMEOS)
#include "ash/display/display_configurator_animation.h"
#include "ui/display/chromeos/display_configurator.h"
#endif

namespace ash {
namespace test {

ShellTestApi::ShellTestApi(Shell* shell) : shell_(shell) {}

RootWindowLayoutManager* ShellTestApi::root_window_layout() {
  return shell_->GetPrimaryRootWindowController()->root_window_layout();
}

wm::InputMethodEventFilter*
ShellTestApi::input_method_event_filter() {
  return shell_->input_method_filter_.get();
}

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

ShelfModel* ShellTestApi::shelf_model() {
  return shell_->shelf_model_.get();
}

DragDropController* ShellTestApi::drag_drop_controller() {
  return shell_->drag_drop_controller_.get();
}

AppListController* ShellTestApi::app_list_controller() {
  return shell_->app_list_controller_.get();
}

void ShellTestApi::DisableDisplayConfiguratorAnimation() {
#if defined(OS_CHROMEOS)
  if (shell_->display_configurator_animation_) {
    shell_->display_configurator_->RemoveObserver(
        shell_->display_configurator_animation_.get());
    shell_->display_configurator_animation_.reset();
  }
#endif  // defined(OS_CHROMEOS)
}

void ShellTestApi::SetShelfDelegate(ShelfDelegate* delegate) {
  shell_->shelf_delegate_.reset(delegate);
}

}  // namespace test
}  // namespace ash
