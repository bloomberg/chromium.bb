// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_event_handler.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm_window.h"
#include "ui/events/event.h"

namespace ash {
namespace wm {
namespace {

// The height of the area in which a touch operation leads to exiting the
// full screen mode.
const int kLeaveFullScreenAreaHeightInPixel = 2;

}  // namespace

MaximizeModeEventHandler::MaximizeModeEventHandler() {}

MaximizeModeEventHandler::~MaximizeModeEventHandler() {}

bool MaximizeModeEventHandler::ToggleFullscreen(const ui::TouchEvent& event) {
  if (event.type() != ui::ET_TOUCH_PRESSED)
    return false;

  const SessionController* controller = Shell::Get()->session_controller();

  if (controller->IsScreenLocked() ||
      controller->GetSessionState() != session_manager::SessionState::ACTIVE) {
    return false;
  }

  // Find the active window (from the primary screen) to un-fullscreen.
  WmWindow* window = WmWindow::Get(GetActiveWindow());
  if (!window)
    return false;

  WindowState* window_state = window->GetWindowState();
  if (!window_state->IsFullscreen() || window_state->in_immersive_fullscreen())
    return false;

  // Test that the touch happened in the top or bottom lines.
  int y = event.y();
  if (y >= kLeaveFullScreenAreaHeightInPixel &&
      y < (window->GetBounds().height() - kLeaveFullScreenAreaHeightInPixel)) {
    return false;
  }

  // Do not exit fullscreen in kiosk mode.
  if (Shell::Get()->session_controller()->IsKioskSession())
    return false;

  WMEvent toggle_fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  window->GetWindowState()->OnWMEvent(&toggle_fullscreen);
  return true;
}

}  // namespace wm
}  // namespace ash
