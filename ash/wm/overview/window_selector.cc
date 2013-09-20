// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_overview.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_panels.h"
#include "ash/wm/overview/window_selector_window.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram.h"
#include "base/timer/timer.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace ash {

namespace {

const int kOverviewDelayOnCycleMilliseconds = 500;

// A comparator for locating a given target window.
struct WindowSelectorItemComparator
    : public std::unary_function<WindowSelectorItem*, bool> {
  explicit WindowSelectorItemComparator(const aura::Window* target_window)
      : target(target_window) {
  }

  bool operator()(WindowSelectorItem* window) const {
    return window->TargetedWindow(target) != NULL;
  }

  const aura::Window* target;
};

// A comparator for locating a selector item for a given root.
struct WindowSelectorItemForRoot
    : public std::unary_function<WindowSelectorItem*, bool> {
  explicit WindowSelectorItemForRoot(const aura::RootWindow* root)
      : root_window(root) {
  }

  bool operator()(WindowSelectorItem* item) const {
    return item->GetRootWindow() == root_window;
  }

  const aura::RootWindow* root_window;
};

// Filter to watch for the termination of a keyboard gesture to cycle through
// multiple windows.
class WindowSelectorEventFilter : public ui::EventHandler {
 public:
  WindowSelectorEventFilter(WindowSelector* selector);
  virtual ~WindowSelectorEventFilter();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

 private:
  // A weak pointer to the WindowSelector which owns this instance.
  WindowSelector* selector_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorEventFilter);
};

// Watch for all keyboard events by filtering the root window.
WindowSelectorEventFilter::WindowSelectorEventFilter(WindowSelector* selector)
    : selector_(selector) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowSelectorEventFilter::~WindowSelectorEventFilter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowSelectorEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    selector_->SelectWindow();
    // Warning: |this| will be deleted from here on.
  }
}

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
  std::vector<WindowSelectorPanels*> panels_items;
  for (size_t i = 0; i < windows.size(); ++i) {
    windows[i]->AddObserver(this);
    observed_windows_.insert(windows[i]);

    if (windows[i]->type() == aura::client::WINDOW_TYPE_PANEL &&
        wm::GetWindowState(windows[i])->panel_attached()) {
      // Attached panel windows are grouped into a single overview item per
      // root window (display).
      std::vector<WindowSelectorPanels*>::iterator iter =
          std::find_if(panels_items.begin(), panels_items.end(),
                       WindowSelectorItemForRoot(windows[i]->GetRootWindow()));
      WindowSelectorPanels* panels_item = NULL;
      if (iter == panels_items.end()) {
        panels_item = new WindowSelectorPanels();
        panels_items.push_back(panels_item);
        windows_.push_back(panels_item);
      } else {
        panels_item = *iter;
      }
      panels_item->AddWindow(windows[i]);
    } else {
      windows_.push_back(new WindowSelectorWindow(windows[i]));
    }
  }
  RemoveFocusAndSetRestoreWindow();
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", windows_.size());

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

  if (mode == WindowSelector::CYCLE) {
    event_handler_.reset(new WindowSelectorEventFilter(this));
    start_overview_timer_.Reset();
  } else {
    StartOverview();
  }
}

WindowSelector::~WindowSelector() {
  ResetFocusRestoreWindow(true);
  for (std::set<aura::Window*>::iterator iter = observed_windows_.begin();
       iter != observed_windows_.end(); ++iter) {
    (*iter)->RemoveObserver(this);
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
    aura::Window* current_window =
        windows_[selected_window_]->SelectionWindow();
    current_window->Show();
    current_window->SetTransform(gfx::Transform());
    current_window->parent()->StackChildAtTop(current_window);
    start_overview_timer_.Reset();
  }
}

void WindowSelector::SelectWindow() {
  ResetFocusRestoreWindow(false);
  SelectWindow(windows_[selected_window_]->SelectionWindow());
}

void WindowSelector::SelectWindow(aura::Window* window) {
  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorItemComparator(window));
  DCHECK(iter != windows_.end());
  // The selected window should not be minimized when window selection is
  // ended.
  (*iter)->RestoreWindowOnExit(window);
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
  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorItemComparator(window));
  DCHECK(window == restore_focus_window_ || iter != windows_.end());
  window->RemoveObserver(this);
  if (window == restore_focus_window_)
    restore_focus_window_ = NULL;
  if (iter == windows_.end())
    return;

  observed_windows_.erase(window);
  (*iter)->RemoveWindow(window);
  // If there are still windows in this selector entry then the overview is
  // still active and the active selection remains the same.
  if (!(*iter)->empty())
    return;

  size_t deleted_index = iter - windows_.begin();
  windows_.erase(iter);
  if (windows_.empty()) {
    CancelSelection();
    return;
  }
  if (window_overview_)
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
      mode_ == CYCLE ? windows_[selected_window_]->GetRootWindow() : NULL));
  if (mode_ == CYCLE)
    window_overview_->SetSelection(selected_window_);
}

void WindowSelector::RemoveFocusAndSetRestoreWindow() {
  aura::client::FocusClient* focus_client = aura::client::GetFocusClient(
      Shell::GetPrimaryRootWindow());
  DCHECK(!restore_focus_window_);
  restore_focus_window_ = focus_client->GetFocusedWindow();
  if (restore_focus_window_) {
    focus_client->FocusWindow(NULL);
    if (observed_windows_.find(restore_focus_window_) ==
            observed_windows_.end()) {
      restore_focus_window_->AddObserver(this);
    }
  }
}

void WindowSelector::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;
  if (focus) {
    base::AutoReset<bool> restoring_focus(&restoring_focus_, true);
    restore_focus_window_->Focus();
  }
  // If the window is in the observed_windows_ list it needs to continue to be
  // observed.
  if (observed_windows_.find(restore_focus_window_) ==
          observed_windows_.end()) {
    restore_focus_window_->RemoveObserver(this);
  }
  restore_focus_window_ = NULL;
}

}  // namespace ash
