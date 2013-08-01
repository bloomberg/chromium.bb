// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_selector.h"

#include <algorithm>

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_selector_delegate.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/corewm/window_animations.h"

namespace ash {

namespace {

const float kCardAspectRatio = 4.0f / 3.0f;
const int kWindowMargin = 20;
const int kMinCardsMajor = 3;
const int kOverviewTransitionMilliseconds = 100;

// Applies a transform to |window| to fit within |target_bounds| while
// maintaining its aspect ratio.
void TransformWindowToFitBounds(aura::Window* window,
                                const gfx::Rect& target_bounds) {
  const gfx::Rect bounds = window->bounds();
  float scale = std::min(1.0f,
      std::min(static_cast<float>(target_bounds.width()) / bounds.width(),
               static_cast<float>(target_bounds.height()) / bounds.height()));
  gfx::Transform transform;
  gfx::Vector2d offset(
      0.5 * (target_bounds.width() - scale * bounds.width()),
      0.5 * (target_bounds.height() - scale * bounds.height()));
  transform.Translate(target_bounds.x() - bounds.x() + offset.x(),
                      target_bounds.y() - bounds.y() + offset.y());
  transform.Scale(scale, scale);
  // TODO(flackr): The window bounds or transform could change during overview
  // mode. WindowSelector should create a copy of the window so that the
  // displayed windows are not affected by changes happening in the background.
  // This will be necessary for alt-tab cycling as well, as some windows will
  // be coming from other displays: http://crbug.com/263481.
  window->SetTransform(transform);
}

}  // namespace

WindowSelector::WindowSelector(const WindowList& windows,
                               WindowSelectorDelegate* delegate)
      : delegate_(delegate) {
  DCHECK(delegate_);
  for (size_t i = 0; i < windows.size(); ++i) {
    windows[i]->AddObserver(this);
    WindowDetails details;
    details.window = windows[i];
    details.minimized = windows[i]->GetProperty(aura::client::kShowStateKey) ==
                        ui::SHOW_STATE_MINIMIZED;
    details.original_transform = windows[i]->layer()->transform();
    if (details.minimized) {
      windows[i]->Show();
    }
    windows_.push_back(details);
  }
  PositionWindows();
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowSelector::~WindowSelector() {
  for (size_t i = 0; i < windows_.size(); i++) {
    ui::ScopedLayerAnimationSettings animation_settings(
        windows_[i].window->layer()->GetAnimator());
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kOverviewTransitionMilliseconds));
    windows_[i].window->RemoveObserver(this);
    gfx::Transform transform;
    windows_[i].window->SetTransform(windows_[i].original_transform);
    if (windows_[i].minimized) {
      // Setting opacity 0 and visible false ensures that the property change
      // to SHOW_STATE_MINIMIZED will not animate the window from its original
      // bounds to the minimized position.
      windows_[i].window->layer()->SetOpacity(0);
      windows_[i].window->layer()->SetVisible(false);
      windows_[i].window->SetProperty(aura::client::kShowStateKey,
                                      ui::SHOW_STATE_MINIMIZED);
    }
  }
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowSelector::OnEvent(ui::Event* event) {
  // TODO(flackr): This will prevent anything else from working while overview
  // mode is active. This should only stop events from being sent to the windows
  // in the overview but still allow interaction with the launcher / tray and
  // hotkeys http://crbug.com/264289.
  EventHandler::OnEvent(event);
  event->StopPropagation();
}

void WindowSelector::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_RELEASED)
    return;
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target->HitTest(event->location()))
    return;

  HandleSelectionEvent(event);
}

void WindowSelector::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;
  HandleSelectionEvent(event);
}

void WindowSelector::OnWindowDestroyed(aura::Window* window) {
  std::vector<WindowDetails>::iterator iter =
      std::find(windows_.begin(), windows_.end(), window);
  DCHECK(iter != windows_.end());
  windows_.erase(iter);
  if (windows_.empty()) {
    delegate_->OnSelectionCanceled();
    return;
  }

  PositionWindows();
}

void WindowSelector::HandleSelectionEvent(ui::Event* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());

  for (size_t i = 0; i < windows_.size(); i++) {
    if (windows_[i].window->Contains(target)) {
      // The selected window should not be minimized when window selection is
      // ended.
      windows_[i].minimized = false;
      windows_[i].original_transform = gfx::Transform();

      // The delegate may delete the WindowSelector, assume the object may no
      // longer valid after calling this.
      delegate_->OnWindowSelected(windows_[i].window);
      return;
    }
  }
}

void WindowSelector::PositionWindows() {
  Shell::RootWindowList root_window_list = Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_window_list.size(); ++i) {
    PositionWindowsOnRoot(root_window_list[i]);
  }
}

void WindowSelector::PositionWindowsOnRoot(aura::RootWindow* root_window) {
  gfx::Size window_size;
  gfx::Rect total_bounds = ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      Shell::GetContainer(root_window,
                          internal::kShellWindowId_DefaultContainer));

  std::vector<WindowDetails> windows;
  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i].window->GetRootWindow() == root_window)
      windows.push_back(windows_[i]);
  }
  if (windows.empty())
    return;

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
    ui::ScopedLayerAnimationSettings animation_settings(
        windows[i].window->layer()->GetAnimator());
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kOverviewTransitionMilliseconds));
    gfx::Transform transform;
    int column = i % columns;
    int row = i / columns;
    gfx::Rect target_bounds(window_size.width() * column + x_offset,
                            window_size.height() * row + y_offset,
                            window_size.width(),
                            window_size.height());
    target_bounds.Inset(kWindowMargin, kWindowMargin);
    TransformWindowToFitBounds(windows[i].window, target_bounds);
  }
}

}  // namespace ash
