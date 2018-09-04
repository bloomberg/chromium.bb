// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/home_launcher_gesture_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_view_state.h"
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
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The animation speed at which the window moves when the gesture is released.
constexpr base::TimeDelta kAnimationDurationMs =
    base::TimeDelta::FromMilliseconds(250);

// The width of the target of screen bounds will be the work area width times
// this ratio.
constexpr float kWidthRatio = 0.8f;

// Checks if |window| can be hidden or shown with a gesture.
bool CanProcessWindow(aura::Window* window,
                      HomeLauncherGestureHandler::Mode mode) {
  DCHECK(window);

  // Window should not be fullscreen as we do not allow swiping up when auto
  // hide for shelf.
  DCHECK(!wm::GetWindowState(window)->IsFullscreen());

  if (!window->IsVisible() &&
      mode == HomeLauncherGestureHandler::Mode::kSwipeUpToShow) {
    return false;
  }

  if (window->IsVisible() &&
      mode == HomeLauncherGestureHandler::Mode::kSwipeDownToHide) {
    return false;
  }

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

void UpdateSettings(ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(kAnimationDurationMs);
  settings->SetTweenType(gfx::Tween::LINEAR);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
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

HomeLauncherGestureHandler::HomeLauncherGestureHandler(
    AppListControllerImpl* app_list_controller)
    : app_list_controller_(app_list_controller) {
  tablet_mode_observer_.Add(Shell::Get()->tablet_mode_controller());
}

HomeLauncherGestureHandler::~HomeLauncherGestureHandler() = default;

bool HomeLauncherGestureHandler::OnPressEvent(Mode mode) {
  // Do not start a new session if a window is currently being processed.
  if (last_event_location_)
    return false;

  // We want the first window in the mru list, if it exists and is usable.
  auto windows = Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
  if (windows.empty() || !CanProcessWindow(windows[0], mode)) {
    window_ = nullptr;
    return false;
  }

  DCHECK_NE(Mode::kNone, mode);
  mode_ = mode;
  window_ = windows[0];
  window_->AddObserver(this);
  windows.erase(windows.begin());

  // Show |window_| if we are swiping down to hide.
  if (mode == Mode::kSwipeDownToHide) {
    ScopedAnimationDisabler disable(window_);
    window_->Show();
    window_->layer()->SetOpacity(1.f);
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

  // Hide all visible windows which are behind our window so that when we
  // scroll, the home launcher will be visible. This is only needed when swiping
  // up.
  hidden_windows_.clear();
  if (mode_ == Mode::kSwipeUpToShow) {
    for (auto* window : windows) {
      if (window->IsVisible()) {
        hidden_windows_.push_back(window);
        window->AddObserver(this);
        ScopedAnimationDisabler disable(window);
        window->Hide();
      }
    }
  }

  UpdateWindows(0.0, /*animate=*/false);
  return true;
}

bool HomeLauncherGestureHandler::OnScrollEvent(const gfx::Point& location) {
  if (!window_)
    return false;

  last_event_location_ = base::make_optional(location);
  double progress = GetHeightInWorkAreaAsRatio(location.y(), window_);
  UpdateWindows(progress, /*animate=*/false);
  return true;
}

bool HomeLauncherGestureHandler::OnReleaseEvent(const gfx::Point& location) {
  if (!window_)
    return false;

  last_event_location_ = base::make_optional(location);
  UpdateWindows(
      GetHeightInWorkAreaAsRatio(location.y(), window_) > 0.5 ? 1.0 : 0.0,
      /*animate=*/true);
  return true;
}

void HomeLauncherGestureHandler::OnWindowDestroying(aura::Window* window) {
  if (window == window_) {
    for (auto* hidden_window : hidden_windows_)
      hidden_window->Show();

    last_event_location_ = base::nullopt;
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

void HomeLauncherGestureHandler::OnTabletModeEnded() {
  if (!last_event_location_)
    return;

  // When leaving tablet mode advance to the end of the in progress scroll
  // session or animation.
  StopObservingImplicitAnimations();
  window_->layer()->GetAnimator()->StopAnimating();
  for (const auto& descendant : transient_descendants_values_)
    descendant.first->layer()->GetAnimator()->StopAnimating();
  UpdateWindows(
      GetHeightInWorkAreaAsRatio(last_event_location_->y(), window_) > 0.5
          ? 1.0
          : 0.0,
      /*animate=*/false);
  OnImplicitAnimationsCompleted();
}

void HomeLauncherGestureHandler::OnImplicitAnimationsCompleted() {
  DCHECK(last_event_location_);
  DCHECK(window_);

  if (GetHeightInWorkAreaAsRatio(last_event_location_->y(), window_) > 0.5) {
    // Minimize the hidden windows so they can be used normally with alt+tab
    // and overview. Minimize in reverse order to preserve mru ordering.
    std::reverse(hidden_windows_.begin(), hidden_windows_.end());
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      wm::GetWindowState(window)->Minimize();
    }

    // Minimize |window_| without animation. Windows in |hidden_windows_| will
    // rename hidden so we can see the home launcher.
    ScopedAnimationDisabler disable(window_);
    wm::GetWindowState(window_)->Minimize();
    window_->SetTransform(window_values_.initial_transform);
  } else {
    // Reshow all windows previously hidden.
    for (auto* window : hidden_windows_) {
      ScopedAnimationDisabler disable(window);
      window->Show();
    }
  }

  // Return the app list to its original opacity and transform without
  // animation.
  app_list_controller_->presenter()->UpdateYPositionAndOpacityForHomeLauncher(
      screen_util::GetDisplayWorkAreaBoundsInParent(window_).y(), 1.f,
      base::NullCallback());
  RemoveObserversAndStopTracking();
}

void HomeLauncherGestureHandler::UpdateWindows(double progress, bool animate) {
  // Helper to update a single windows opacity and transform based on by
  // calculating the in between values using |value| and |values|.
  auto update_windows_helper = [this](double progress, bool animate,
                                      aura::Window* window,
                                      const WindowValues& values) {
    float opacity = gfx::Tween::FloatValueBetween(
        progress, values.initial_opacity, values.target_opacity);
    gfx::Transform transform = gfx::Tween::TransformValueBetween(
        progress, values.initial_transform, values.target_transform);

    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (animate) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          window->layer()->GetAnimator());
      UpdateSettings(settings.get());
      if (window == this->window())
        settings->AddObserver(this);
    }
    window->layer()->SetOpacity(opacity);
    window->SetTransform(transform);
  };

  for (const auto& descendant : transient_descendants_values_) {
    update_windows_helper(progress, animate, descendant.first,
                          descendant.second);
  }
  update_windows_helper(progress, animate, window_, window_values_);

  // Update full screen applist. On tests, |window_| may be null because
  // OnImplicitAnimationsCompleted runs right away, which invalidates |window_|,
  // so just skip this section.
  if (!window_)
    return;

  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_);
  const int y_position =
      gfx::Tween::IntValueBetween(progress, work_area.bottom(), work_area.y());
  app_list_controller_->presenter()->UpdateYPositionAndOpacityForHomeLauncher(
      y_position,
      gfx::Tween::FloatValueBetween(progress, window_values_.target_opacity,
                                    window_values_.initial_opacity),
      animate ? base::BindRepeating(&UpdateSettings) : base::NullCallback());
}

void HomeLauncherGestureHandler::RemoveObserversAndStopTracking() {
  last_event_location_ = base::nullopt;
  mode_ = Mode::kNone;

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
