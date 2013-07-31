// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include <algorithm>

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_handler.h"

namespace ash {

namespace {

// Filter to watch for the termination of a keyboard gesture to cycle through
// multiple windows.
class WindowCycleEventFilter : public ui::EventHandler {
 public:
  WindowCycleEventFilter();
  virtual ~WindowCycleEventFilter();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(WindowCycleEventFilter);
};

// Watch for all keyboard events by filtering the root window.
WindowCycleEventFilter::WindowCycleEventFilter() {
}

WindowCycleEventFilter::~WindowCycleEventFilter() {
}

void WindowCycleEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::GetInstance()->window_cycle_controller()->AltKeyReleased();
    // Warning: |this| will be deleted from here on.
  }
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() {
}

WindowCycleController::~WindowCycleController() {
  StopCycling();
}

// static
bool WindowCycleController::CanCycle() {
  // Don't allow window cycling if the screen is locked or a modal dialog is
  // open.
  return !Shell::GetInstance()->session_state_delegate()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen();
}

void WindowCycleController::HandleCycleWindow(Direction direction,
                                              bool is_alt_down) {
  if (!CanCycle())
    return;

  if (is_alt_down) {
    if (!IsCycling()) {
      // This is the start of an alt-tab cycle through multiple windows, so
      // listen for the alt key being released to stop cycling.
      StartCycling();
      Step(direction);
      InstallEventFilter();
    } else {
      // We're in the middle of an alt-tab cycle, just step forward.
      Step(direction);
    }
  } else {
    // This is a simple, single-step window cycle.
    StartCycling();
    Step(direction);
    StopCycling();
  }
}

void WindowCycleController::HandleLinearCycleWindow() {
  if (!CanCycle() || IsCycling())
    return;

  // Use the reversed list of windows to prevent a 2-cycle of the most recent
  // windows occurring.
  WindowCycleList cycle_list(MruWindowTracker::BuildWindowList(true));
  cycle_list.Step(WindowCycleList::FORWARD);
}

void WindowCycleController::AltKeyReleased() {
  StopCycling();
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

void WindowCycleController::StartCycling() {
  windows_.reset(new WindowCycleList(ash::Shell::GetInstance()->
      mru_window_tracker()->BuildMruWindowList()));
}

void WindowCycleController::Step(Direction direction) {
  DCHECK(windows_.get());
  windows_->Step(direction == FORWARD ? WindowCycleList::FORWARD :
                 WindowCycleList::BACKWARD);
}

void WindowCycleController::StopCycling() {
  windows_.reset();
  // Remove our key event filter.
  if (event_handler_) {
    Shell::GetInstance()->RemovePreTargetHandler(event_handler_.get());
    event_handler_.reset();
  }
}

void WindowCycleController::InstallEventFilter() {
  event_handler_.reset(new WindowCycleEventFilter());
  Shell::GetInstance()->AddPreTargetHandler(event_handler_.get());
}

}  // namespace ash
