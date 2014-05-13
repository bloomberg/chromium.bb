// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/switchable_windows.h"
#include "ash/wm/overview/window_overview.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_panels.h"
#include "ash/wm/overview/window_selector_window.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// A comparator for locating a given selectable window.
struct WindowSelectorItemComparator
    : public std::unary_function<WindowSelectorItem*, bool> {
  explicit WindowSelectorItemComparator(const aura::Window* window)
      : window_(window) {
  }

  bool operator()(WindowSelectorItem* window) const {
    return window->HasSelectableWindow(window_);
  }

  const aura::Window* window_;
};

// A comparator for locating a selectable window given a targeted window.
struct WindowSelectorItemTargetComparator
    : public std::unary_function<WindowSelectorItem*, bool> {
  explicit WindowSelectorItemTargetComparator(const aura::Window* target_window)
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
  explicit WindowSelectorItemForRoot(const aura::Window* root)
      : root_window(root) {
  }

  bool operator()(WindowSelectorItem* item) const {
    return item->GetRootWindow() == root_window;
  }

  const aura::Window* root_window;
};

// Triggers a shelf visibility update on all root window controllers.
void UpdateShelfVisibility() {
  Shell::RootWindowControllerList root_window_controllers =
      Shell::GetInstance()->GetAllRootWindowControllers();
  for (Shell::RootWindowControllerList::iterator iter =
          root_window_controllers.begin();
       iter != root_window_controllers.end(); ++iter) {
    (*iter)->UpdateShelfVisibility();
  }
}

}  // namespace

WindowSelector::WindowSelector(const WindowList& windows,
                               WindowSelectorDelegate* delegate)
    : delegate_(delegate),
      restore_focus_window_(aura::client::GetFocusClient(
          Shell::GetPrimaryRootWindow())->GetFocusedWindow()),
      ignore_activations_(false) {
  DCHECK(delegate_);

  if (restore_focus_window_)
    restore_focus_window_->AddObserver(this);

  std::vector<WindowSelectorPanels*> panels_items;
  for (size_t i = 0; i < windows.size(); ++i) {
    WindowSelectorItem* item = NULL;
    if (windows[i] != restore_focus_window_)
      windows[i]->AddObserver(this);
    observed_windows_.insert(windows[i]);

    if (windows[i]->type() == ui::wm::WINDOW_TYPE_PANEL &&
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
      item = panels_item;
    } else {
      item = new WindowSelectorWindow(windows[i]);
      windows_.push_back(item);
    }
    // Verify that the window has been added to an item in overview.
    CHECK(item->TargetedWindow(windows[i]));
  }
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", windows_.size());

  // Observe window activations and switchable containers on all root windows
  // for newly created windows during overview.
  Shell::GetInstance()->activation_client()->AddObserver(this);
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
      aura::Window* container = Shell::GetContainer(*iter,
          kSwitchableWindowContainerIds[i]);
      container->AddObserver(this);
      observed_windows_.insert(container);
    }
  }

  StartOverview();
}

WindowSelector::~WindowSelector() {
  ResetFocusRestoreWindow(true);
  for (std::set<aura::Window*>::iterator iter = observed_windows_.begin();
       iter != observed_windows_.end(); ++iter) {
    (*iter)->RemoveObserver(this);
  }
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  window_overview_.reset();
  // Clearing the window list resets the ignored_by_shelf flag on the windows.
  windows_.clear();
  UpdateShelfVisibility();
}

void WindowSelector::SelectWindow(aura::Window* window) {
  ResetFocusRestoreWindow(false);
  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorItemTargetComparator(window));
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
  if (new_window->type() != ui::wm::WINDOW_TYPE_NORMAL &&
      new_window->type() != ui::wm::WINDOW_TYPE_PANEL) {
    return;
  }

  for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
    if (new_window->parent()->id() == kSwitchableWindowContainerIds[i] &&
        !::wm::GetTransientParent(new_window)) {
      // The new window is in one of the switchable containers, abort overview.
      CancelSelection();
      return;
    }
  }
}

void WindowSelector::OnWindowDestroying(aura::Window* window) {
  // window is one of a container, the restore_focus_window and/or
  // one of the selectable windows in overview.
  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorItemComparator(window));
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  if (window == restore_focus_window_)
    restore_focus_window_ = NULL;
  if (iter == windows_.end())
    return;

  (*iter)->RemoveWindow(window);
  // If there are still windows in this selector entry then the overview is
  // still active and the active selection remains the same.
  if (!(*iter)->empty())
    return;

  windows_.erase(iter);
  if (windows_.empty()) {
    CancelSelection();
    return;
  }
  if (window_overview_)
    window_overview_->OnWindowsChanged();
}

void WindowSelector::OnWindowBoundsChanged(aura::Window* window,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  if (!window_overview_)
    return;

  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorItemTargetComparator(window));
  if (iter == windows_.end())
    return;

  // Immediately finish any active bounds animation.
  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);

  // Recompute the transform for the window.
  (*iter)->RecomputeWindowTransforms();
}

void WindowSelector::OnWindowActivated(aura::Window* gained_active,
                                       aura::Window* lost_active) {
  if (ignore_activations_ || !gained_active)
    return;
  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  CancelSelection();
}

void WindowSelector::OnAttemptToReactivateWindow(aura::Window* request_active,
                                                 aura::Window* actual_active) {
  if (ignore_activations_)
    return;
  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  CancelSelection();
}

void WindowSelector::StartOverview() {
  DCHECK(!window_overview_);
  // Remove focus from active window before entering overview.
  aura::client::GetFocusClient(
      Shell::GetPrimaryRootWindow())->FocusWindow(NULL);

  window_overview_.reset(new WindowOverview(this, &windows_));
  UpdateShelfVisibility();
}

void WindowSelector::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;
  if (focus) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
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
