// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/home_launcher_gesture_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "base/numerics/ranges.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr float kWidthRatio = 0.8f;

// Find the transform that will convert |src| to |dst|.
gfx::Transform CalculateTransform(const gfx::RectF& src,
                                  const gfx::RectF& dst) {
  return gfx::Transform(dst.width() / src.width(), 0, 0,
                        dst.height() / src.height(), dst.x() - src.x(),
                        dst.y() - src.y());
}

// Get the target offscreen bounds of |window|. These bounds will be used to
// calculate the transform which will move |window| out and offscreen. The width
// will be a ratio of the work area, and the height will maintain the aspect
// ratio.
gfx::RectF GetOffscreenBounds(aura::Window* window) {
  gfx::RectF bounds = gfx::RectF(window->GetTargetBounds());
  gfx::RectF work_area =
      gfx::RectF(screen_util::GetDisplayWorkAreaBoundsInParent(window));
  float aspect_ratio = bounds.width() / bounds.height();
  work_area.set_x(((1.f - kWidthRatio) / 2.f) * work_area.width() +
                  work_area.x());
  work_area.set_width(kWidthRatio * work_area.width());
  work_area.set_height(work_area.width() / aspect_ratio);
  work_area.set_y(work_area.y() - work_area.height());
  return work_area;
}

// Get the target bounds of a transient descendant of the window we are hiding.
// It should maintain the same ratios to the main window.
gfx::RectF GetTransientDescendantBounds(aura::Window* transient_descendant,
                                        const gfx::RectF& src,
                                        const gfx::RectF& dst) {
  gfx::RectF bounds = gfx::RectF(transient_descendant->GetTargetBounds());
  float ratio = dst.width() / src.width();

  gfx::RectF dst_bounds;
  dst_bounds.set_x(bounds.x() * ratio + dst.x());
  dst_bounds.set_y(bounds.y() * ratio + dst.y());
  dst_bounds.set_width(bounds.width() * ratio);
  dst_bounds.set_height(bounds.height() * ratio);
  return dst_bounds;
}

// Given a y screen coordinate |y|, find out where it lies as a ratio in the
// work area, where the top of the work area is 0.f and the bottom is 1.f.
double GetHeightInWorkAreaAsRatio(int y, aura::Window* window) {
  gfx::Rect work_area = screen_util::GetDisplayWorkAreaBoundsInParent(window);
  int clamped_y = base::ClampToRange(y, work_area.y(), work_area.bottom());
  double ratio =
      static_cast<double>(clamped_y) / static_cast<double>(work_area.height());
  return 1.0 - ratio;
}

// Helper class to perform window state changes without animations. Used to hide
// and minimize windows without having their animation interfere with the ones
// this class is in charge of.
class ScopedAnimationDisabler {
 public:
  explicit ScopedAnimationDisabler(aura::Window* window) : window_(window) {
    needs_disable_ =
        !window_->GetProperty(aura::client::kAnimationsDisabledKey);
    if (needs_disable_)
      window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }
  ~ScopedAnimationDisabler() {
    if (needs_disable_)
      window_->SetProperty(aura::client::kAnimationsDisabledKey, false);
  }

 private:
  aura::Window* window_;
  bool needs_disable_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedAnimationDisabler);
};

}  // namespace

HomeLauncherGestureHandler::HomeLauncherGestureHandler() = default;

HomeLauncherGestureHandler::~HomeLauncherGestureHandler() = default;

void HomeLauncherGestureHandler::OnPressEvent() {
  // We want the first window in the mru list, if it exists and is usable.
  auto windows = Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
  if (windows.empty() || !CanHideWindow(windows[0])) {
    window_ = nullptr;
    return;
  }

  window_ = windows[0];
  window_->AddObserver(this);
  windows.erase(windows.begin());
  // Hide all visible windows which are behind our window so that when we
  // scroll, the home launcher will be visible.
  hidden_windows_.clear();
  for (auto* window : windows) {
    if (window->IsVisible()) {
      hidden_windows_.push_back(window);
      window->AddObserver(this);
      ScopedAnimationDisabler disable(window);
      window->Hide();
    }
  }

  const gfx::RectF& current_bounds = gfx::RectF(window_->GetTargetBounds());
  const gfx::RectF& target_bounds = GetOffscreenBounds(window_);
  // Calculate the values for the main window.
  window_values_.initial_opacity = window_->layer()->opacity();
  window_values_.initial_transform = window_->transform();
  window_values_.target_opacity = 0.f;
  window_values_.target_transform =
      CalculateTransform(current_bounds, target_bounds);

  // Calculate the values for any transient descendants.
  transient_descendants_values_.clear();
  for (auto* window : wm::GetTransientTreeIterator(window_)) {
    if (window == window_)
      continue;

    WindowValues values;
    values.initial_opacity = window->layer()->opacity();
    values.initial_transform = window->transform();
    values.target_opacity = 0.f;
    values.target_transform = CalculateTransform(
        gfx::RectF(window->GetTargetBounds()),
        GetTransientDescendantBounds(window, current_bounds, target_bounds));
    window->AddObserver(this);
    transient_descendants_values_[window] = values;
  }
}

void HomeLauncherGestureHandler::OnScrollEvent(const gfx::Point& location) {
  double progress = GetHeightInWorkAreaAsRatio(location.y(), window_);
  UpdateWindows(progress);
}

void HomeLauncherGestureHandler::OnReleaseEvent(const gfx::Point& location) {
  // TODO(sammiequon): Animate the window to its final destination.
  if (GetHeightInWorkAreaAsRatio(location.y(), window_) > 0.5) {
    UpdateWindows(1.0);

    // Minimize the hidden windows so they can be used normally with alt+tab
    // and overview.
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      wm::GetWindowState(window)->Minimize();
    }

    // Minimize |window_| without animation. Windows in |hidden_windows_| will
    // rename hidden so we can see the home launcher.
    ScopedAnimationDisabler disable(window_);
    wm::GetWindowState(window_)->Minimize();
  } else {
    // Reshow all windows previously hidden.
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      window->Show();
    }

    UpdateWindows(0.0);
  }

  RemoveObserversAndStopTracking();
}

void HomeLauncherGestureHandler::OnWindowDestroying(aura::Window* window) {
  if (window == window_) {
    for (auto* hidden_window : hidden_windows_)
      hidden_window->Show();

    RemoveObserversAndStopTracking();
    return;
  }

  if (transient_descendants_values_.find(window) !=
      transient_descendants_values_.end()) {
    window->RemoveObserver(this);
    transient_descendants_values_.erase(window);
    return;
  }

  DCHECK(base::ContainsValue(hidden_windows_, window));
  window->RemoveObserver(this);
  hidden_windows_.erase(
      std::find(hidden_windows_.begin(), hidden_windows_.end(), window));
}

bool HomeLauncherGestureHandler::CanHideWindow(aura::Window* window) {
  DCHECK(window);

  if (!window->IsVisible())
    return false;

  if (!Shell::Get()->app_list_controller()->IsHomeLauncherEnabledInTabletMode())
    return false;

  if (Shell::Get()->split_view_controller()->IsSplitViewModeActive())
    return false;

  if (Shell::Get()->window_selector_controller()->IsSelecting())
    return false;

  if (window->type() == aura::client::WINDOW_TYPE_POPUP)
    return false;

  if (::wm::GetTransientParent(window))
    return false;

  wm::WindowState* state = wm::GetWindowState(window);
  return state->CanMaximize() && state->CanResize();
}

void HomeLauncherGestureHandler::UpdateWindows(double progress) {
  // Helper to update a single windows opacity and transform based on by
  // calculating the in between values using |value| and |values|.
  auto update_windows_helper = [](double progress, aura::Window* window,
                                  const WindowValues& values) {
    float opacity = gfx::Tween::FloatValueBetween(
        progress, values.initial_opacity, values.target_opacity);
    window->layer()->SetOpacity(opacity);
    gfx::Transform transform = gfx::Tween::TransformValueBetween(
        progress, values.initial_transform, values.target_transform);
    window->SetTransform(transform);
  };

  update_windows_helper(progress, window_, window_values_);
  for (const auto& descendant : transient_descendants_values_)
    update_windows_helper(progress, descendant.first, descendant.second);
}

void HomeLauncherGestureHandler::RemoveObserversAndStopTracking() {
  for (auto* window : hidden_windows_)
    window->RemoveObserver(this);
  hidden_windows_.clear();

  for (const auto& descendant : transient_descendants_values_)
    descendant.first->RemoveObserver(this);
  transient_descendants_values_.clear();

  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace ash
