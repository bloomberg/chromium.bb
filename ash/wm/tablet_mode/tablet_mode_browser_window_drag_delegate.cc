// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The scale factor that the source window should scale if the source window is
// not the dragged window && is not in splitscreen when drag starts && the user
// has dragged the window to pass the |kIndicatorThresholdRatio| vertical
// threshold.
constexpr float kSourceWindowScale = 0.85;

// Values of the blurred scrim.
constexpr SkColor kScrimBackgroundColor = SK_ColorGRAY;
constexpr float kScrimOpacity = 0.8;
constexpr float kScrimBlur = 5.f;
constexpr int kScrimTransitionInMs = 250;
constexpr int kScrimRoundRectRadiusDp = 4;

// The threshold to compute the vertical distance to create/reset the scrim. The
// scrim is placed below the current dragged window. This value is smaller than
// kIndicatorsThreshouldRatio to prevent the dragged window to merge back to
// its source window during its source window's animation.
constexpr float kScrimResetThresholdRatio = 0.05;

// Returns the window selector if overview mode is active, otherwise returns
// nullptr.
WindowSelector* GetWindowSelector() {
  return Shell::Get()->window_selector_controller()->IsSelecting()
             ? Shell::Get()->window_selector_controller()->window_selector()
             : nullptr;
}

// Creates a transparent scrim which is placed below |dragged_window|.
std::unique_ptr<views::Widget> CreateScrim(aura::Window* dragged_window,
                                           const gfx::Rect& bounds) {
  std::unique_ptr<views::Widget> widget = CreateBackgroundWidget(
      /*root_window=*/dragged_window->GetRootWindow(),
      /*layer_type=*/ui::LAYER_TEXTURED,
      /*background_color=*/kScrimBackgroundColor,
      /*border_thickness=*/0,
      /*border_radius=*/kScrimRoundRectRadiusDp,
      /*border_color=*/SK_ColorTRANSPARENT,
      /*initial_opacity=*/0.f,
      /*parent=*/dragged_window->parent(),
      /*stack_on_top=*/false);
  widget->SetBounds(bounds);
  return widget;
}

// When the dragged window is dragged past this value, a transparent scrim will
// be created to place below the current dragged window to prevent the dragged
// window to merge into any browser window beneath it and when it's dragged back
// toward the top of the screen, the scrim will be destroyed.
int GetScrimVerticalThreshold(const gfx::Rect& work_area_bounds) {
  return work_area_bounds.y() +
         work_area_bounds.height() * kScrimResetThresholdRatio;
}

// The class to observe the source window's bounds change animation. When the
// bounds animation is running, set the dragged window not be able to attach to
// any window to prevent it accidently attach into the animating source window.
class SourceWindowAnimationObserver : public ui::ImplicitAnimationObserver,
                                      public ui::LayerObserver,
                                      public aura::WindowObserver {
 public:
  SourceWindowAnimationObserver(ui::Layer* source_window_layer,
                                aura::Window* dragged_window)
      : source_window_layer_(source_window_layer),
        dragged_window_(dragged_window) {
    source_window_layer_->AddObserver(this);
    dragged_window_->AddObserver(this);
  }

  ~SourceWindowAnimationObserver() override {
    if (source_window_layer_)
      source_window_layer_->RemoveObserver(this);

    if (dragged_window_) {
      dragged_window_->RemoveObserver(this);
      dragged_window_->ClearProperty(ash::kCanAttachToAnotherWindowKey);
    }
  }

  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {
    DCHECK(dragged_window_);
    dragged_window_->SetProperty(ash::kCanAttachToAnotherWindowKey, false);
  }

  void OnImplicitAnimationsCompleted() override { delete this; }

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override {
    DCHECK_EQ(source_window_layer_, layer);
    delete this;
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(dragged_window_, window);
    delete this;
  }

 private:
  ui::Layer* source_window_layer_;
  aura::Window* dragged_window_;

  DISALLOW_COPY_AND_ASSIGN(SourceWindowAnimationObserver);
};

}  // namespace

TabletModeBrowserWindowDragDelegate::TabletModeBrowserWindowDragDelegate() =
    default;

TabletModeBrowserWindowDragDelegate::~TabletModeBrowserWindowDragDelegate() =
    default;

void TabletModeBrowserWindowDragDelegate::PrepareForDraggedWindow(
    const gfx::Point& location_in_screen) {
  DCHECK(dragged_window_);

  wm::WindowState* window_state = wm::GetWindowState(dragged_window_);
  window_state->OnDragStarted(window_state->drag_details()->window_component);
}

void TabletModeBrowserWindowDragDelegate::UpdateForDraggedWindow(
    const gfx::Point& location_in_screen) {
  DCHECK(dragged_window_);

  // Update the source window if necessary.
  UpdateSourceWindow(location_in_screen);

  // Update the scrim that beneath the dragged window if necessary.
  UpdateScrim(location_in_screen);
}

void TabletModeBrowserWindowDragDelegate::EndingForDraggedWindow(
    wm::WmToplevelWindowEventHandler::DragResult result,
    const gfx::Point& location_in_screen) {
  if (result == wm::WmToplevelWindowEventHandler::DragResult::SUCCESS)
    wm::GetWindowState(dragged_window_)->OnCompleteDrag(location_in_screen);
  else
    wm::GetWindowState(dragged_window_)->OnRevertDrag(location_in_screen);

  // The source window might have been scaled during dragging, update its bounds
  // to ensure it has the right bounds after the drag ends.
  aura::Window* source_window =
      dragged_window_->GetProperty(ash::kTabDraggingSourceWindowKey);
  if (source_window && !wm::GetWindowState(source_window)->IsSnapped()) {
    TabletModeWindowState::UpdateWindowPosition(
        wm::GetWindowState(source_window));
  }
  scrim_.reset();
}

bool TabletModeBrowserWindowDragDelegate::ShouldOpenOverviewWhenDragStarts() {
  DCHECK(dragged_window_);
  aura::Window* source_window =
      dragged_window_->GetProperty(ash::kTabDraggingSourceWindowKey);
  return !source_window;
}

void TabletModeBrowserWindowDragDelegate::UpdateSourceWindow(
    const gfx::Point& location_in_screen) {
  // Only do the scale if the source window is not the dragged window && the
  // source window is not in splitscreen && the source window is not in
  // overview.
  aura::Window* source_window =
      dragged_window_->GetProperty(ash::kTabDraggingSourceWindowKey);
  if (!source_window || source_window == dragged_window_ ||
      source_window == split_view_controller_->left_window() ||
      source_window == split_view_controller_->right_window() ||
      (GetWindowSelector() &&
       GetWindowSelector()->IsWindowInOverview(source_window))) {
    return;
  }

  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  gfx::Rect expected_bounds(work_area_bounds);
  if (location_in_screen.y() >=
      GetIndicatorsVerticalThreshold(work_area_bounds)) {
    SplitViewController::SnapPosition snap_position =
        GetSnapPosition(location_in_screen);

    if (snap_position == SplitViewController::NONE) {
      // Scale down the source window if the event location passes the vertical
      // |kIndicatorThresholdRatio| threshold.
      expected_bounds.ClampToCenteredSize(
          gfx::Size(work_area_bounds.width() * kSourceWindowScale,
                    work_area_bounds.height() * kSourceWindowScale));
    } else {
      // Put the source window on the other side of the split screen.
      expected_bounds = split_view_controller_->GetSnappedWindowBoundsInScreen(
          source_window, snap_position == SplitViewController::LEFT
                             ? SplitViewController::RIGHT
                             : SplitViewController::LEFT);
    }
  }
  ::wm::ConvertRectFromScreen(source_window->parent(), &expected_bounds);

  if (expected_bounds != source_window->GetTargetBounds()) {
    ui::ScopedLayerAnimationSettings settings(
        source_window->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(new SourceWindowAnimationObserver(
        source_window->layer(), dragged_window_));
    source_window->SetBounds(expected_bounds);
  }
}

void TabletModeBrowserWindowDragDelegate::UpdateScrim(
    const gfx::Point& location_in_screen) {
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  if (location_in_screen.y() < GetScrimVerticalThreshold(work_area_bounds)) {
    // Remove |scrim_| entirely so that the dragged window can be merged back
    // to the source window when the dragged window is dragged back toward the
    // top area of the screen.
    scrim_.reset();
    return;
  }

  // If overview mode is active, do not show the scrim on the overview side of
  // the screen.
  if (Shell::Get()->window_selector_controller()->IsSelecting()) {
    WindowGrid* window_grid = GetWindowSelector()->GetGridWithRootWindow(
        dragged_window_->GetRootWindow());
    if (window_grid && window_grid->bounds().Contains(location_in_screen)) {
      scrim_.reset();
      return;
    }
  }

  SplitViewController::SnapPosition snap_position =
      GetSnapPosition(location_in_screen);
  gfx::Rect expected_bounds(work_area_bounds);
  if (split_view_controller_->IsSplitViewModeActive()) {
    expected_bounds = split_view_controller_->GetSnappedWindowBoundsInScreen(
        dragged_window_, snap_position);
  } else {
    expected_bounds.Inset(kHighlightScreenEdgePaddingDp,
                          kHighlightScreenEdgePaddingDp);
  }

  bool should_show_blurred_scrim = false;
  if (location_in_screen.y() >=
      GetMaximizeVerticalThreshold(work_area_bounds)) {
    if (split_view_controller_->IsSplitViewModeActive() !=
        (snap_position == SplitViewController::NONE)) {
      should_show_blurred_scrim = true;
    }
  }
  // When the event is between |indicators_vertical_threshold| and
  // |maximize_vertical_threshold|, the scrim is still shown but is invisible
  // to the user (transparent). It's needed to prevent the dragged window to
  // merge into the scaled down source window.
  ShowScrim(should_show_blurred_scrim ? kScrimOpacity : 0.f,
            should_show_blurred_scrim ? kScrimBlur : 0.f, expected_bounds);
}

void TabletModeBrowserWindowDragDelegate::ShowScrim(
    float opacity,
    float blur,
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds(bounds_in_screen);
  ::wm::ConvertRectFromScreen(dragged_window_->parent(), &bounds);

  if (scrim_ && scrim_->GetLayer()->GetTargetOpacity() == opacity &&
      scrim_->GetNativeWindow()->bounds() == bounds) {
    return;
  }

  if (!scrim_)
    scrim_ = CreateScrim(dragged_window_, bounds);
  dragged_window_->parent()->StackChildBelow(scrim_->GetNativeWindow(),
                                             dragged_window_);
  scrim_->GetLayer()->SetBackgroundBlur(blur);

  if (scrim_->GetNativeWindow()->bounds() != bounds) {
    scrim_->SetOpacity(0.f);
    scrim_->SetBounds(bounds);
  }
  ui::ScopedLayerAnimationSettings animation(scrim_->GetLayer()->GetAnimator());
  animation.SetTweenType(gfx::Tween::EASE_IN_OUT);
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kScrimTransitionInMs));
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  scrim_->SetOpacity(opacity);
}

}  // namespace ash
