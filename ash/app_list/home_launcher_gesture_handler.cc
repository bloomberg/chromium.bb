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
#include "base/numerics/ranges.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/wm/core/transient_window_manager.h"

namespace ash {

namespace {

constexpr float kWidthInsetRatio = 0.1f;

// Given a |window|, find the transform which will move its bounds to a region
// offscreen to the top. Its destination width will be inset by a percentage,
// and its width-height ratio will remain the same.
gfx::Transform CalculateOffscreenTransform(aura::Window* window) {
  gfx::RectF bounds = gfx::RectF(window->GetTargetBounds());
  gfx::RectF work_area =
      gfx::RectF(screen_util::GetDisplayWorkAreaBoundsInParent(window));
  float inverse_aspect_ratio = work_area.height() / work_area.width();
  work_area.Inset(kWidthInsetRatio * work_area.width(), 0);
  work_area.set_height(work_area.width() * inverse_aspect_ratio);
  work_area.set_y(-work_area.height());
  gfx::Transform transform(work_area.width() / bounds.width(), 0, 0,
                           work_area.height() / bounds.height(),
                           work_area.x() - bounds.x(),
                           work_area.y() - bounds.y());
  return transform;
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
  auto windows = Shell::Get()->mru_window_tracker()->BuildMruWindowList();
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

  original_opacity_ = window_->layer()->opacity();
  original_transform_ = window_->transform();
  target_transform_ = CalculateOffscreenTransform(window_);
}

void HomeLauncherGestureHandler::OnScrollEvent(const gfx::Point& location) {
  double value = GetHeightInWorkAreaAsRatio(location.y(), window_);
  float opacity = gfx::Tween::FloatValueBetween(value, original_opacity_, 0.f);
  window_->layer()->SetOpacity(opacity);
  gfx::Transform transform = gfx::Tween::TransformValueBetween(
      value, original_transform_, target_transform_);
  window_->SetTransform(transform);
}

void HomeLauncherGestureHandler::OnReleaseEvent(const gfx::Point& location) {
  // TODO(sammiequon): Animate the window to its final destination.
  if (GetHeightInWorkAreaAsRatio(location.y(), window_) > 0.5) {
    window_->layer()->SetOpacity(1.f);
    window_->SetTransform(target_transform_);

    // Minimize |window_| without animation. Windows in |hidden_windows_| will
    // rename hidden so we can see the home launcher.
    // TODO(sammiequon): Investigate if we should minimize the windows in
    // |hidden_windows_|.
    ScopedAnimationDisabler disable(window_);
    wm::GetWindowState(window_)->Minimize();
  } else {
    // Reshow all windows previously hidden.
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      window->Show();
    }

    window_->layer()->SetOpacity(original_opacity_);
    window_->SetTransform(original_transform_);
  }

  for (auto* window : hidden_windows_)
    window->RemoveObserver(this);
  hidden_windows_.clear();

  window_->RemoveObserver(this);
  window_ = nullptr;
}

void HomeLauncherGestureHandler::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  if (window == window_) {
    for (auto* hidden_window : hidden_windows_)
      hidden_window->Show();
    hidden_windows_.clear();
    window_ = nullptr;
    return;
  }

  DCHECK(base::ContainsValue(hidden_windows_, window));
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

  // TODO(sammiequon): Handle transient windows.
  const ::wm::TransientWindowManager* manager =
      ::wm::TransientWindowManager::GetIfExists(window);
  if (manager &&
      (manager->transient_parent() || !manager->transient_children().empty())) {
    return false;
  }

  wm::WindowState* state = wm::GetWindowState(window);
  return state->CanMaximize() && state->CanResize();
}

}  // namespace ash
