// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_cycle_list.h"
#include "base/metrics/histogram.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

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

WindowCycleEventFilter::WindowCycleEventFilter() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowCycleEventFilter::~WindowCycleEventFilter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowCycleEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::GetInstance()->window_cycle_controller()->StopCycling();
    // Warning: |this| will be deleted from here on.
  }
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() {
}

WindowCycleController::~WindowCycleController() {
}

// static
bool WindowCycleController::CanCycle() {
  // Don't allow window cycling if the screen is locked or a modal dialog is
  // open.
  return !Shell::GetInstance()->session_state_delegate()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen();
}

void WindowCycleController::HandleCycleWindow(Direction direction) {
  if (!CanCycle())
    return;

  if (!IsCycling())
    StartCycling();

  Step(direction);
}

void WindowCycleController::StartCycling() {
  window_cycle_list_.reset(new WindowCycleList(ash::Shell::GetInstance()->
      mru_window_tracker()->BuildMruWindowList()));
  event_handler_.reset(new WindowCycleEventFilter());
  cycle_start_time_ = base::Time::Now();
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(UMA_WINDOW_CYCLE);
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

void WindowCycleController::Step(Direction direction) {
  DCHECK(window_cycle_list_.get());
  window_cycle_list_->Step(direction);
}

void WindowCycleController::StopCycling() {
  window_cycle_list_.reset();
  // Remove our key event filter.
  event_handler_.reset();
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowCycleController.CycleTime",
                             base::Time::Now() - cycle_start_time_);
}

}  // namespace ash
