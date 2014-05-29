// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/switchable_windows.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/overview/window_selector_panels.h"
#include "ash/wm/overview/window_selector_window.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Conceptually the window overview is a table or grid of cells having this
// fixed aspect ratio. The number of columns is determined by maximizing the
// area of them based on the number of windows.
const float kCardAspectRatio = 4.0f / 3.0f;

// The minimum number of cards along the major axis (i.e. horizontally on a
// landscape orientation).
const int kMinCardsMajor = 3;

// A comparator for locating a given target window.
struct WindowSelectorItemComparator
    : public std::unary_function<WindowSelectorItem*, bool> {
  explicit WindowSelectorItemComparator(const aura::Window* target_window)
      : target(target_window) {
  }

  bool operator()(WindowSelectorItem* window) const {
    return window->HasSelectableWindow(target);
  }

  const aura::Window* target;
};

// An observer which holds onto the passed widget until the animation is
// complete.
class CleanupWidgetAfterAnimationObserver : public ui::LayerAnimationObserver {
 public:
  explicit CleanupWidgetAfterAnimationObserver(
      scoped_ptr<views::Widget> widget);

  // ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE;

 private:
  virtual ~CleanupWidgetAfterAnimationObserver();

  scoped_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(CleanupWidgetAfterAnimationObserver);
};

CleanupWidgetAfterAnimationObserver::CleanupWidgetAfterAnimationObserver(
    scoped_ptr<views::Widget> widget)
    : widget_(widget.Pass()) {
  widget_->GetNativeWindow()->layer()->GetAnimator()->AddObserver(this);
}

CleanupWidgetAfterAnimationObserver::~CleanupWidgetAfterAnimationObserver() {
  widget_->GetNativeWindow()->layer()->GetAnimator()->RemoveObserver(this);
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
}

// A comparator for locating a selectable window given a targeted window.
struct WindowSelectorItemTargetComparator
    : public std::unary_function<WindowSelectorItem*, bool> {
  explicit WindowSelectorItemTargetComparator(const aura::Window* target_window)
      : target(target_window) {
  }

  bool operator()(WindowSelectorItem* window) const {
    return window->Contains(target);
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
    CHECK(item->Contains(windows[i]));
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
  ash::Shell* shell = ash::Shell::GetInstance();

  ResetFocusRestoreWindow(true);
  for (std::set<aura::Window*>::iterator iter = observed_windows_.begin();
       iter != observed_windows_.end(); ++iter) {
    (*iter)->RemoveObserver(this);
  }
  shell->activation_client()->RemoveObserver(this);
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  const aura::WindowTracker::Windows hidden_windows(hidden_windows_.windows());
  for (aura::WindowTracker::Windows::const_iterator iter =
       hidden_windows.begin(); iter != hidden_windows.end(); ++iter) {
    ui::ScopedLayerAnimationSettings settings(
        (*iter)->layer()->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        ScopedTransformOverviewWindow::kTransitionMilliseconds));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    (*iter)->layer()->SetOpacity(1);
    (*iter)->Show();
  }

  shell->RemovePreTargetHandler(this);
  shell->GetScreen()->RemoveObserver(this);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Ash.WindowSelector.TimeInOverview",
      base::Time::Now() - overview_start_time_);

  // TODO(nsatragno): Change this to OnOverviewModeEnded and move it to when
  // everything is done.
  shell->OnOverviewModeEnding();

  // Clearing the window list resets the ignored_by_shelf flag on the windows.
  windows_.clear();
  UpdateShelfVisibility();
}

void WindowSelector::CancelSelection() {
  delegate_->OnSelectionEnded();
}

void WindowSelector::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  if (event->key_code() == ui::VKEY_ESCAPE) {
    CancelSelection();
    event->SetHandled();
  }
}

void WindowSelector::OnDisplayAdded(const gfx::Display& display) {
}

void WindowSelector::OnDisplayRemoved(const gfx::Display& display) {
}

void WindowSelector::OnDisplayMetricsChanged(const gfx::Display& display,
                                             uint32_t metrics) {
  PositionWindows(/* animate */ false);
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
  PositionWindows(true);
}

void WindowSelector::OnWindowBoundsChanged(aura::Window* window,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
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

  ScopedVector<WindowSelectorItem>::iterator iter = std::find_if(
      windows_.begin(), windows_.end(),
      WindowSelectorItemComparator(gained_active));

  if (iter != windows_.end())
    (*iter)->RestoreWindowOnExit(gained_active);

  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  CancelSelection();
}

void WindowSelector::OnAttemptToReactivateWindow(aura::Window* request_active,
                                                 aura::Window* actual_active) {
  OnWindowActivated(request_active, actual_active);
}

void WindowSelector::StartOverview() {
  // Remove focus from active window before entering overview.
  aura::client::GetFocusClient(
      Shell::GetPrimaryRootWindow())->FocusWindow(NULL);

  Shell* shell = Shell::GetInstance();
  shell->OnOverviewModeStarting();

  for (WindowSelectorItemList::iterator iter = windows_.begin();
       iter != windows_.end(); ++iter) {
    (*iter)->PrepareForOverview();
  }
  PositionWindows(/* animate */ true);
  DCHECK(!windows_.empty());
  shell->PrependPreTargetHandler(this);
  shell->GetScreen()->AddObserver(this);
  shell->metrics()->RecordUserMetricsAction(UMA_WINDOW_OVERVIEW);
  HideAndTrackNonOverviewWindows();
  // Send an a11y alert.
  shell->accessibility_delegate()->TriggerAccessibilityAlert(
      A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED);

  UpdateShelfVisibility();
}

bool WindowSelector::Contains(const aura::Window* window) {
  for (WindowSelectorItemList::iterator iter = windows_.begin();
      iter != windows_.end(); ++iter) {
    if ((*iter)->Contains(window))
      return true;
  }
  return false;
}

void WindowSelector::PositionWindows(bool animate) {
  aura::Window::Windows root_window_list = Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_window_list.size(); ++i)
    PositionWindowsFromRoot(root_window_list[i], animate);
}

void WindowSelector::PositionWindowsFromRoot(aura::Window* root_window,
                                             bool animate) {
  std::vector<WindowSelectorItem*> windows;
  for (WindowSelectorItemList::iterator iter = windows_.begin();
       iter != windows_.end(); ++iter) {
    if ((*iter)->GetRootWindow() == root_window)
      windows.push_back(*iter);
  }

  if (windows.empty())
    return;

  gfx::Size window_size;
  gfx::Rect total_bounds = ScreenUtil::ConvertRectToScreen(
      root_window,
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(
          Shell::GetContainer(root_window, kShellWindowId_DefaultContainer)));

  // Find the minimum number of windows per row that will fit all of the
  // windows on screen.
  size_t columns = std::max(
      total_bounds.width() > total_bounds.height() ? kMinCardsMajor : 1,
      static_cast<int>(ceil(sqrt(total_bounds.width() * windows.size() /
                                 (kCardAspectRatio * total_bounds.height())))));
  size_t rows = ((windows.size() + columns - 1) / columns);
  window_size.set_width(std::min(
      static_cast<int>(total_bounds.width() / columns),
      static_cast<int>(total_bounds.height() * kCardAspectRatio / rows)));
  window_size.set_height(window_size.width() / kCardAspectRatio);

  // Calculate the X and Y offsets necessary to center the grid.
  int x_offset = total_bounds.x() + ((windows.size() >= columns ? 0 :
      (columns - windows.size()) * window_size.width()) +
      (total_bounds.width() - columns * window_size.width())) / 2;
  int y_offset = total_bounds.y() + (total_bounds.height() -
      rows * window_size.height()) / 2;
  for (size_t i = 0; i < windows.size(); ++i) {
    gfx::Transform transform;
    int column = i % columns;
    int row = i / columns;
    gfx::Rect target_bounds(window_size.width() * column + x_offset,
                            window_size.height() * row + y_offset,
                            window_size.width(),
                            window_size.height());
    windows[i]->SetBounds(root_window, target_bounds, animate);
  }
}

void WindowSelector::HideAndTrackNonOverviewWindows() {
  // Add the windows to hidden_windows first so that if any are destroyed
  // while hiding them they are tracked.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::const_iterator root_iter = root_windows.begin();
       root_iter != root_windows.end(); ++root_iter) {
    for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
      aura::Window* container = Shell::GetContainer(*root_iter,
          kSwitchableWindowContainerIds[i]);
      for (aura::Window::Windows::const_iterator iter =
           container->children().begin(); iter != container->children().end();
           ++iter) {
        if (Contains(*iter) || !(*iter)->IsVisible())
          continue;
        hidden_windows_.Add(*iter);
      }
    }
  }

  // Copy the window list as it can change during iteration.
  const aura::WindowTracker::Windows hidden_windows(hidden_windows_.windows());
  for (aura::WindowTracker::Windows::const_iterator iter =
       hidden_windows.begin(); iter != hidden_windows.end(); ++iter) {
    if (!hidden_windows_.Contains(*iter))
      continue;
    ui::ScopedLayerAnimationSettings settings(
        (*iter)->layer()->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        ScopedTransformOverviewWindow::kTransitionMilliseconds));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    (*iter)->Hide();
    // Hiding the window can result in it being destroyed.
    if (!hidden_windows_.Contains(*iter))
      continue;
    (*iter)->layer()->SetOpacity(0);
  }
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
