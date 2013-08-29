// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/wm/overview/window_overview.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_window.h"
#include "base/timer/timer.h"
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
      selected_window_(0) {
  DCHECK(delegate_);
  for (size_t i = 0; i < windows.size(); ++i) {
    windows[i]->AddObserver(this);
    windows_.push_back(new WindowSelectorWindow(windows[i]));
  }
  if (mode == WindowSelector::CYCLE)
    start_overview_timer_.Reset();
  else
    StartOverview();
}

WindowSelector::~WindowSelector() {
  for (size_t i = 0; i < windows_.size(); i++) {
    windows_[i]->window()->RemoveObserver(this);
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

void WindowSelector::OnWindowDestroyed(aura::Window* window) {
  ScopedVector<WindowSelectorWindow>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorWindowComparator(window));
  DCHECK(iter != windows_.end());
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

void WindowSelector::StartOverview() {
  DCHECK(!window_overview_);
  window_overview_.reset(new WindowOverview(this, &windows_,
      mode_ == CYCLE ? Shell::GetActiveRootWindow() : NULL));
  if (mode_ == CYCLE)
    window_overview_->SetSelection(selected_window_);
}

}  // namespace ash
