// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include "ash/metrics/task_switch_source.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle_event_filter.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_state.h"
#include "ash/wm_window.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

namespace {

// Returns the most recently active window from the |window_list| or nullptr
// if the list is empty.
aura::Window* GetActiveWindow(const WindowCycleList::WindowList& window_list) {
  return window_list.empty() ? nullptr : window_list[0];
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() {}

WindowCycleController::~WindowCycleController() {}

// static
bool WindowCycleController::CanCycle() {
  // Prevent window cycling if the screen is locked or a modal dialog is open.
  return !Shell::Get()->session_controller()->IsScreenLocked() &&
         !ShellPort::Get()->IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned();
}

void WindowCycleController::HandleCycleWindow(Direction direction) {
  if (!CanCycle())
    return;

  if (!IsCycling())
    StartCycling();

  Step(direction);
}

void WindowCycleController::StartCycling() {
  WindowCycleList::WindowList window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();
  // Exclude windows:
  // - non user positionable windows, such as extension popups.
  // - windows being dragged
  // - the AppList window, which will hide as soon as cycling starts
  //   anyway. It doesn't make sense to count it as a "switchable" window, yet
  //   a lot of code relies on the MRU list returning the app window. If we
  //   don't manually remove it, the window cycling UI won't crash or misbehave,
  //   but there will be a flicker as the target window changes. Also exclude
  //   unselectable windows such as extension popups.
  auto window_is_ineligible = [](aura::Window* window) {
    wm::WindowState* state = wm::GetWindowState(window);
    return !state->IsUserPositionable() || state->is_dragged() ||
           window->GetRootWindow()
               ->GetChildById(kShellWindowId_AppListContainer)
               ->Contains(window);
  };
  window_list.erase(std::remove_if(window_list.begin(), window_list.end(),
                                   window_is_ineligible),
                    window_list.end());

  active_window_before_window_cycle_ = GetActiveWindow(window_list);

  window_cycle_list_.reset(new WindowCycleList(window_list));
  event_filter_ = ShellPort::Get()->CreateWindowCycleEventFilter();
  cycle_start_time_ = base::Time::Now();
  ShellPort::Get()->RecordUserMetricsAction(UMA_WINDOW_CYCLE);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowCycleController.Items",
                           window_list.size());
}

void WindowCycleController::CompleteCycling() {
  window_cycle_list_->set_user_did_accept(true);
  StopCycling();
}

void WindowCycleController::CancelCycling() {
  StopCycling();
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

void WindowCycleController::Step(Direction direction) {
  DCHECK(window_cycle_list_.get());
  window_cycle_list_->Step(direction);
}

void WindowCycleController::StopCycling() {
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowCycleController.SelectionDepth",
                           window_cycle_list_->current_index() + 1);
  window_cycle_list_.reset();

  aura::Window* active_window_after_window_cycle =
      GetActiveWindow(Shell::Get()->mru_window_tracker()->BuildMruWindowList());

  // Remove our key event filter.
  event_filter_.reset();
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowCycleController.CycleTime",
                             base::Time::Now() - cycle_start_time_);

  if (active_window_after_window_cycle != nullptr &&
      active_window_before_window_cycle_ != active_window_after_window_cycle) {
    ShellPort::Get()->RecordTaskSwitchMetric(
        TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);
  }
  active_window_before_window_cycle_ = nullptr;
}

}  // namespace ash
