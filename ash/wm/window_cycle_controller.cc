// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"

namespace ash {

// Filter to watch for the termination of a keyboard gesture to cycle through
// multiple windows.
class WindowCycleEventFilter : public aura::EventFilter {
 public:
  WindowCycleEventFilter();
  virtual ~WindowCycleEventFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(WindowCycleEventFilter);
};

// Watch for all keyboard events by filtering the root window.
WindowCycleEventFilter::WindowCycleEventFilter()
    : aura::EventFilter(aura::RootWindow::GetInstance()) {
}

WindowCycleEventFilter::~WindowCycleEventFilter() {
}

bool WindowCycleEventFilter::PreHandleKeyEvent(
    aura::Window* target,
    aura::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::GetInstance()->window_cycle_controller()->AltKeyReleased();
    // Warning: |this| will be deleted from here on.
  }
  return false;  // Always let the event propagate.
}

bool WindowCycleEventFilter::PreHandleMouseEvent(
    aura::Window* target,
    aura::MouseEvent* event) {
  return false;  // Not handled.
}

ui::TouchStatus WindowCycleEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;  // Not handled.
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() : current_index_(-1) {
}

WindowCycleController::~WindowCycleController() {
  StopCycling();
}

void WindowCycleController::HandleCycleWindow(Direction direction,
                                              bool is_alt_down) {
  // Don't allow window cycling if the screen is locked.
  if (Shell::GetInstance()->IsScreenLocked())
    return;
  // Don't cycle away from modal dialogs.
  if (Shell::GetInstance()->IsModalWindowOpen())
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

void WindowCycleController::AltKeyReleased() {
  StopCycling();
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

void WindowCycleController::StartCycling() {
  // Most-recently-used cycling is confusing in compact window mode because
  // you can't see all the windows.
  windows_ = ash::Shell::GetInstance()->delegate()->GetCycleWindowList(
      Shell::GetInstance()->IsWindowModeCompact() ?
          ShellDelegate::ORDER_LINEAR :
          ShellDelegate::ORDER_MRU);

  // Locate the currently active window in the list to use as our start point.
  aura::Window* active_window = GetActiveWindow();
  if (!active_window) {
    DLOG(ERROR) << "No active window";
    return;
  }
  // The active window may not be in the cycle list, which is expected if there
  // are additional modal windows on the screen. Our index will be -1 and we
  // won't step through the windows below.
  current_index_ = GetWindowIndex(active_window);
}

void WindowCycleController::Step(Direction direction) {
  // Ensure we have at least one window to step to.
  if (windows_.empty()) {
    DLOG(ERROR) << "No windows in cycle list";
    return;
  }
  if (current_index_ == -1) {
    // We weren't able to find our active window in the shell delegate's
    // provided window list.  Just switch to the first (or last) one.
    current_index_ = (direction == FORWARD ? 0 : windows_.size() - 1);
  } else {
    // We're in a valid cycle, so step forward or backward.
    current_index_ += (direction == FORWARD ? 1 : -1);
  }
  // Wrap to window list size.
  current_index_ = (current_index_ + windows_.size()) % windows_.size();
  DCHECK(windows_[current_index_]);
  ActivateWindow(windows_[current_index_]);
}

void WindowCycleController::StopCycling() {
  windows_.clear();
  current_index_ = -1;
  // Remove our key event filter.
  if (event_filter_.get()) {
    Shell::GetInstance()->RemoveRootWindowEventFilter(event_filter_.get());
    event_filter_.reset();
  }
}

void WindowCycleController::InstallEventFilter() {
  event_filter_.reset(new WindowCycleEventFilter());
  Shell::GetInstance()->AddRootWindowEventFilter(event_filter_.get());
}

int WindowCycleController::GetWindowIndex(aura::Window* window) {
  WindowList::const_iterator it =
      std::find(windows_.begin(), windows_.end(), window);
  if (it == windows_.end())
    return -1;  // Not found.
  return it - windows_.begin();
}

}  // namespace ash
