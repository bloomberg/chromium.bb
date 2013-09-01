// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_overview.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_window.h"
#include "base/auto_reset.h"
#include "base/timer/timer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

const int kOverviewDelayOnCycleMilliseconds = 300;

// A comparator for locating a given target window.
struct WindowSelectorWindowComparator
    : public std::unary_function<WindowSelectorWindow*, bool> {
  explicit WindowSelectorWindowComparator(const aura::Window* target_window)
      : target(target_window) {
  }

  bool operator()(const WindowSelectorWindow* window) const {
    return target == window->window();
  }

  const aura::Window* target;
};

}  // namespace

WindowSelector::WindowSelector(const WindowList& windows,
                               WindowSelector::Mode mode,
                               WindowSelectorDelegate* delegate)
    : mode_(mode),
      start_overview_timer_(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kOverviewDelayOnCycleMilliseconds),
          this, &WindowSelector::StartOverview),
      delegate_(delegate),
      selected_window_(0),
      restore_focus_window_(NULL),
      restoring_focus_(false) {
  DCHECK(delegate_);
  RemoveFocusAndSetRestoreWindow();
  for (size_t i = 0; i < windows.size(); ++i) {
    // restore_focus_window_ is already observed from the call to
    // RemoveFocusAndSetRestoreWindow.
    if (windows[i] != restore_focus_window_)
      windows[i]->AddObserver(this);
    windows_.push_back(new WindowSelectorWindow(windows[i]));
  }

  // Observe window activations and switchable containers on all root windows
  // for newly created windows during overview.
  Shell::GetInstance()->activation_client()->AddObserver(this);
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
      Shell::GetContainer(*iter,
                          kSwitchableWindowContainerIds[i])->AddObserver(this);
    }
  }

  if (mode == WindowSelector::CYCLE)
    start_overview_timer_.Reset();
  else
    StartOverview();
}

WindowSelector::~WindowSelector() {
  ResetFocusRestoreWindow(true);
  for (size_t i = 0; i < windows_.size(); i++) {
    windows_[i]->window()->RemoveObserver(this);
  }
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
      Shell::GetContainer(*iter,
          kSwitchableWindowContainerIds[i])->RemoveObserver(this);
    }
  }
}

void WindowSelector::Step(WindowSelector::Direction direction) {
  DCHECK_EQ(CYCLE, mode_);
  DCHECK(!windows_.empty());
  selected_window_ = (selected_window_ + windows_.size() +
      (direction == WindowSelector::FORWARD ? 1 : -1)) % windows_.size();
  if (window_overview_) {
    window_overview_->SetSelection(selected_window_);
  } else {
    aura::Window* current_window = windows_[selected_window_]->window();
    current_window->Show();
    current_window->SetTransform(gfx::Transform());
    current_window->parent()->StackChildAtTop(current_window);
    start_overview_timer_.Reset();
  }
}

void WindowSelector::SelectWindow() {
  ResetFocusRestoreWindow(false);
  SelectWindow(windows_[selected_window_]->window());
}

void WindowSelector::SelectWindow(aura::Window* window) {
  ScopedVector<WindowSelectorWindow>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorWindowComparator(window));
  DCHECK(iter != windows_.end());
  // The selected window should not be minimized when window selection is
  // ended.
  (*iter)->RestoreWindowOnExit();
  delegate_->OnWindowSelected(window);
}

void WindowSelector::CancelSelection() {
  delegate_->OnSelectionCanceled();
}

void WindowSelector::OnWindowAdded(aura::Window* new_window) {
  if (new_window->type() != aura::client::WINDOW_TYPE_NORMAL &&
      new_window->type() != aura::client::WINDOW_TYPE_PANEL) {
    return;
  }

  for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
    if (new_window->parent()->id() == kSwitchableWindowContainerIds[i] &&
        !new_window->transient_parent()) {
      // The new window is in one of the switchable containers, abort overview.
      CancelSelection();
      return;
    }
  }
}

void WindowSelector::OnWindowDestroyed(aura::Window* window) {
  ScopedVector<WindowSelectorWindow>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorWindowComparator(window));
  DCHECK(window == restore_focus_window_ || iter != windows_.end());
  window->RemoveObserver(this);
  if (window == restore_focus_window_)
    restore_focus_window_ = NULL;
  if (iter == windows_.end())
    return;

  size_t deleted_index = iter - windows_.begin();
  (*iter)->OnWindowDestroyed();
  windows_.erase(iter);
  if (windows_.empty()) {
    CancelSelection();
    return;
  }
  window_overview_->OnWindowsChanged();
  if (mode_ == CYCLE && selected_window_ >= deleted_index) {
    if (selected_window_ > deleted_index)
      selected_window_--;
    selected_window_ = selected_window_ % windows_.size();
    if (window_overview_)
      window_overview_->SetSelection(selected_window_);
  }
}

void WindowSelector::OnWindowActivated(aura::Window* gained_active,
                                       aura::Window* lost_active) {
  if (restoring_focus_ || !gained_active)
    return;
  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  CancelSelection();
}

void WindowSelector::OnAttemptToReactivateWindow(aura::Window* request_active,
                                                 aura::Window* actual_active) {
  if (restoring_focus_)
    return;
  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  CancelSelection();
}

void WindowSelector::StartOverview() {
  DCHECK(!window_overview_);
  window_overview_.reset(new WindowOverview(this, &windows_,
      mode_ == CYCLE ? Shell::GetActiveRootWindow() : NULL));
  if (mode_ == CYCLE)
    window_overview_->SetSelection(selected_window_);
}

void WindowSelector::RemoveFocusAndSetRestoreWindow() {
  aura::client::FocusClient* focus_client = aura::client::GetFocusClient(
      Shell::GetActiveRootWindow());
  DCHECK(!restore_focus_window_);
  restore_focus_window_ = focus_client->GetFocusedWindow();
  if (restore_focus_window_) {
    focus_client->FocusWindow(NULL);
    restore_focus_window_->AddObserver(this);
  }
}

void WindowSelector::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;
  if (focus) {
    base::AutoReset<bool> restoring_focus(&restoring_focus_, true);
    restore_focus_window_->Focus();
  }
  // If the window is in the windows_ list it needs to continue to be observed.
  if (std::find_if(windows_.begin(), windows_.end(),
          WindowSelectorWindowComparator(restore_focus_window_)) ==
              windows_.end()) {
    restore_focus_window_->RemoveObserver(this);
  }
  restore_focus_window_ = NULL;
}

}  // namespace ash
