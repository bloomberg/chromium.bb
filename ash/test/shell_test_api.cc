// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/shell_test_api.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"

#if defined(OS_CHROMEOS)
#include "ash/display/output_configurator_animation.h"
#include "chromeos/display/output_configurator.h"
#endif

namespace ash {
namespace test {

ShellTestApi::ShellTestApi(Shell* shell) : shell_(shell) {}

internal::RootWindowLayoutManager* ShellTestApi::root_window_layout() {
  return shell_->GetPrimaryRootWindowController()->root_window_layout();
}

views::corewm::InputMethodEventFilter*
ShellTestApi::input_method_event_filter() {
  return shell_->input_method_filter_.get();
}

internal::SystemGestureEventFilter*
ShellTestApi::system_gesture_event_filter() {
  return shell_->system_gesture_filter_.get();
}

internal::WorkspaceController* ShellTestApi::workspace_controller() {
  return shell_->GetPrimaryRootWindowController()->workspace_controller();
}

internal::ScreenPositionController*
ShellTestApi::screen_position_controller() {
  return shell_->screen_position_controller_.get();
}

AshNativeCursorManager* ShellTestApi::ash_native_cursor_manager() {
  return shell_->native_cursor_manager_;
}

LauncherModel* ShellTestApi::launcher_model() {
  return shell_->launcher_model_.get();
}

void ShellTestApi::DisableOutputConfiguratorAnimation() {
#if defined(OS_CHROMEOS)
  if (shell_->output_configurator_animation_.get()) {
    shell_->output_configurator_->RemoveObserver(
        shell_->output_configurator_animation_.get());
    shell_->output_configurator_animation_.reset();
  }
#endif  // defined(OS_CHROMEOS)
}

}  // namespace test
}  // namespace ash
