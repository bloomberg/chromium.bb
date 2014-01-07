// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_overview.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_panels.h"
#include "ash/wm/overview/window_selector_window.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/timer.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/views/corewm/window_util.h"

namespace ash {

namespace {

// The time from when the user pressed alt+tab while still holding alt before
// overview is engaged.
const int kOverviewDelayOnCycleMilliseconds = 10000;

// If the delay before overview is less than or equal to this threshold the
// initial monitor is used for multi-display overview, otherwise the monitor
// of the currently selected window is used.
const int kOverviewDelayInitialMonitorThreshold = 100;

// The maximum amount of time allowed for the delay before overview on cycling.
// If the specified time exceeds this the timer will not be started.
const int kMaxOverviewDelayOnCycleMilliseconds = 10000;

int GetOverviewDelayOnCycleMilliseconds() {
  static int value = -1;
  if (value == -1) {
    value = kOverviewDelayOnCycleMilliseconds;
    if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshOverviewDelayOnAltTab)) {
      if (!base::StringToInt(CommandLine::ForCurrentProcess()->
            GetSwitchValueASCII(switches::kAshOverviewDelayOnAltTab), &value)) {
        LOG(ERROR) << "Expected int value for "
                   << switches::kAshOverviewDelayOnAltTab;
      }
    }
  }
  return value;
}

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

// Returns the window immediately below |window| in the current container.
aura::Window* GetWindowBelow(aura::Window* window) {
  aura::Window* parent = window->parent();
  if (!parent)
    return NULL;
  aura::Window* below = NULL;
  for (aura::Window::Windows::const_iterator iter = parent->children().begin();
       iter != parent->children().end(); ++iter) {
    if (*iter == window)
      return below;
    below = *iter;
  }
  NOTREACHED();
  return NULL;
}

}  // namespace

// This class restores and moves a window to the front of the stacking order for
// the duration of the class's scope.
class ScopedShowWindow : public aura::WindowObserver {
 public:
  ScopedShowWindow();
  virtual ~ScopedShowWindow();

  // Show |window| at the top of the stacking order.
  void Show(aura::Window* window);

  // Cancel restoring the window on going out of scope.
  void CancelRestore();

  aura::Window* window() { return window_; }

  // aura::WindowObserver:
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;

 private:
  // The window being shown.
  aura::Window* window_;

  // The window immediately below where window_ belongs.
  aura::Window* stack_window_above_;

  // If true, minimize window_ on going out of scope.
  bool minimized_;

  DISALLOW_COPY_AND_ASSIGN(ScopedShowWindow);
};

ScopedShowWindow::ScopedShowWindow()
    : window_(NULL),
      stack_window_above_(NULL),
      minimized_(false) {
}

void ScopedShowWindow::Show(aura::Window* window) {
  DCHECK(!window_);
  window_ = window;
  stack_window_above_ = GetWindowBelow(window);
  minimized_ = wm::GetWindowState(window)->IsMinimized();
  window_->parent()->AddObserver(this);
  window_->Show();
  wm::GetWindowState(window_)->Activate();
}

ScopedShowWindow::~ScopedShowWindow() {
  if (window_) {
    window_->parent()->RemoveObserver(this);

    // Restore window's stacking position.
    if (stack_window_above_)
      window_->parent()->StackChildAbove(window_, stack_window_above_);
    else
      window_->parent()->StackChildAtBottom(window_);

    // Restore minimized state.
    if (minimized_)
      wm::GetWindowState(window_)->Minimize();
  }
}

void ScopedShowWindow::CancelRestore() {
  if (!window_)
    return;
  window_->parent()->RemoveObserver(this);
  window_ = stack_window_above_ = NULL;
}

void ScopedShowWindow::OnWillRemoveWindow(aura::Window* window) {
  if (window == window_) {
    CancelRestore();
  } else if (window == stack_window_above_) {
    // If the window this window was above is removed, use the next window down
    // as the restore marker.
    stack_window_above_ = GetWindowBelow(stack_window_above_);
  }
}

WindowSelector::WindowSelector(const WindowList& windows,
                               WindowSelector::Mode mode,
                               WindowSelectorDelegate* delegate)
    : mode_(mode),
      timer_enabled_(GetOverviewDelayOnCycleMilliseconds() <
                         kMaxOverviewDelayOnCycleMilliseconds),
      start_overview_timer_(FROM_HERE,
          base::TimeDelta::FromMilliseconds(
              GetOverviewDelayOnCycleMilliseconds()),
          this, &WindowSelector::StartOverview),
      delegate_(delegate),
      selected_window_(0),
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

  if (mode == WindowSelector::CYCLE) {
    cycle_start_time_ = base::Time::Now();
    event_handler_.reset(new WindowSelectorEventFilter(this));
    if (timer_enabled_)
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
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  window_overview_.reset();
  // Clearing the window list resets the ignored_by_shelf flag on the windows.
  windows_.clear();
  UpdateShelfVisibility();

  if (!cycle_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowSelector.CycleTime",
        base::Time::Now() - cycle_start_time_);
  }
}

void WindowSelector::Step(WindowSelector::Direction direction) {
  DCHECK(!windows_.empty());
  // Upgrade to CYCLE mode if currently in OVERVIEW mode.
  if (mode_ != CYCLE) {
    event_handler_.reset(new WindowSelectorEventFilter(this));
    DCHECK(window_overview_);
    // Set the initial selection window to animate to the new selection.
    window_overview_->SetSelection(selected_window_);
    window_overview_->MoveToSingleRootWindow(
        windows_[selected_window_]->GetRootWindow());
    mode_ = CYCLE;
  }

  selected_window_ = (selected_window_ + windows_.size() +
      (direction == WindowSelector::FORWARD ? 1 : -1)) % windows_.size();
  if (window_overview_) {
    window_overview_->SetSelection(selected_window_);
  } else {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    showing_window_.reset(new ScopedShowWindow);
    showing_window_->Show(windows_[selected_window_]->SelectionWindow());
    if (timer_enabled_)
      start_overview_timer_.Reset();
  }
}

void WindowSelector::SelectWindow() {
  SelectWindow(windows_[selected_window_]->SelectionWindow());
}

void WindowSelector::SelectWindow(aura::Window* window) {
  ResetFocusRestoreWindow(false);
  if (showing_window_ && showing_window_->window() == window)
    showing_window_->CancelRestore();
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
        !views::corewm::GetTransientParent(new_window)) {
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

void WindowSelector::OnWindowBoundsChanged(aura::Window* window,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  if (!window_overview_)
    return;

  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorItemTargetComparator(window));
  DCHECK(window == restore_focus_window_ || iter != windows_.end());
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

  aura::Window* overview_root = NULL;
  if (mode_ == CYCLE) {
    overview_root = GetOverviewDelayOnCycleMilliseconds() <=
                        kOverviewDelayInitialMonitorThreshold ?
                    windows_.front()->GetRootWindow() :
                    windows_[selected_window_]->GetRootWindow();
  }
  window_overview_.reset(new WindowOverview(this, &windows_, overview_root));
  if (mode_ == CYCLE)
    window_overview_->SetSelection(selected_window_);
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
