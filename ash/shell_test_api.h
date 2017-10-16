// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_TEST_API_H_
#define ASH_SHELL_TEST_API_H_

#include <memory>

#include "base/macros.h"

class PrefService;

namespace ash {
class DragDropController;
class MessageCenterController;
class NativeCursorManagerAsh;
class ScreenPositionController;
class Shell;
class SystemGestureEventFilter;
class TabletModeWindowManager;
class WorkspaceController;

// Accesses private data from a Shell for testing.
class ShellTestApi {
 public:
  ShellTestApi();
  explicit ShellTestApi(Shell* shell);

  MessageCenterController* message_center_controller();
  SystemGestureEventFilter* system_gesture_event_filter();
  WorkspaceController* workspace_controller();
  ScreenPositionController* screen_position_controller();
  NativeCursorManagerAsh* native_cursor_manager_ash();
  DragDropController* drag_drop_controller();
  TabletModeWindowManager* tablet_mode_window_manager();

  // Calls the private method.
  void OnLocalStatePrefServiceInitialized(
      std::unique_ptr<PrefService> pref_service);

  // Resets |shell_->power_button_controller_| to hold a new object to simulate
  // Chrome starting.
  void ResetPowerButtonControllerForTest();

 private:
  Shell* shell_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(ShellTestApi);
};

}  // namespace ash

#endif  // ASH_SHELL_TEST_API_H_
