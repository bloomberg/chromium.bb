// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_delegate.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_session_windows_hider.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The following distances are copied from tablet_mode_window_drag_delegate.cc.
// TODO(https://crbug.com/1069869): share these constants.

// Items dragged to within |kDistanceFromEdgeDp| of the screen will get snapped
// even if they have not moved by |kMinimumDragToSnapDistanceDp|.
constexpr float kDistanceFromEdgeDp = 16.f;
// The minimum distance that an item must be moved before it is snapped. This
// prevents accidental snaps.
constexpr float kMinimumDragToSnapDistanceDp = 96.f;

// The scale factor that the source window should scale if the source window is
// not the dragged window && is not in splitscreen when drag starts && the user
// has dragged the window to pass the |kIndicatorThresholdRatio| vertical
// threshold.
constexpr float kSourceWindowScale = 0.85;

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsSourceWindowForDrag, false)

}  // namespace

// static
bool TabDragDropDelegate::IsChromeTabDrag(const ui::OSExchangeData& drag_data) {
  if (!features::IsWebUITabStripTabDragIntegrationEnabled())
    return false;

  return Shell::Get()->shell_delegate()->IsTabDrag(drag_data);
}

// static
bool TabDragDropDelegate::IsSourceWindowForDrag(const aura::Window* window) {
  return window->GetProperty(kIsSourceWindowForDrag);
}

TabDragDropDelegate::TabDragDropDelegate(
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& start_location_in_screen)
    : root_window_(root_window),
      source_window_(source_window->GetToplevelWindow()),
      start_location_in_screen_(start_location_in_screen) {
  DCHECK(root_window_);
  DCHECK(source_window_);
  source_window_->SetProperty(kIsSourceWindowForDrag, true);
  split_view_drag_indicators_ =
      std::make_unique<SplitViewDragIndicators>(root_window_);
}

TabDragDropDelegate::~TabDragDropDelegate() {
  if (!source_window_->GetProperty(kIsSourceWindowForDrag))
    return;

  // If we didn't drop to a new window, we must restore the original window.
  RestoreSourceWindowBounds();
  source_window_->ClearProperty(kIsSourceWindowForDrag);
}

void TabDragDropDelegate::DragUpdate(const gfx::Point& location_in_screen) {
  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  const SplitViewController::SnapPosition snap_position =
      ::ash::GetSnapPositionForLocation(
          Shell::GetPrimaryRootWindow(), location_in_screen,
          start_location_in_screen_,
          /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
          /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
          /*horizontal_edge_inset=*/area.width() *
                  kHighlightScreenPrimaryAxisRatio +
              kHighlightScreenEdgePaddingDp,
          /*vertical_edge_inset=*/area.height() *
                  kHighlightScreenPrimaryAxisRatio +
              kHighlightScreenEdgePaddingDp);
  split_view_drag_indicators_->SetWindowDraggingState(
      SplitViewDragIndicators::ComputeWindowDraggingState(
          true, SplitViewDragIndicators::WindowDraggingState::kFromTop,
          snap_position));

  UpdateSourceWindowBoundsIfNecessary(snap_position);
}

void TabDragDropDelegate::Drop(const gfx::Point& location_in_screen,
                               const ui::OSExchangeData& drop_data) {
  aura::Window* const new_window =
      Shell::Get()->shell_delegate()->CreateBrowserForTabDrop(source_window_,
                                                              drop_data);
  DCHECK(new_window);

  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  SplitViewController::SnapPosition snap_position = ash::GetSnapPosition(
      root_window_, new_window, location_in_screen, start_location_in_screen_,
      /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
      /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
      /*horizontal_edge_inset=*/area.width() *
              kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp,
      /*vertical_edge_inset=*/area.height() * kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp);

  if (snap_position == SplitViewController::SnapPosition::NONE)
    RestoreSourceWindowBounds();

  // This must be done after restoring the source window's bounds since
  // otherwise the SetBounds() call may have no effect.
  source_window_->ClearProperty(kIsSourceWindowForDrag);

  if (snap_position == SplitViewController::SnapPosition::NONE)
    return;

  SplitViewController* const split_view_controller =
      SplitViewController::Get(new_window);
  split_view_controller->SnapWindow(new_window, snap_position);

  // The tab drag source window is the last window the user was
  // interacting with. When dropping into split view, it makes the most
  // sense to snap this window to the opposite side. Do this.
  SplitViewController::SnapPosition opposite_position =
      (snap_position == SplitViewController::SnapPosition::LEFT)
          ? SplitViewController::SnapPosition::RIGHT
          : SplitViewController::SnapPosition::LEFT;

  // |source_window_| is itself a child window of the browser since it
  // hosts web content (specifically, the tab strip WebUI). Snap its
  // toplevel window which is the browser window.
  split_view_controller->SnapWindow(source_window_, opposite_position);
}

void TabDragDropDelegate::UpdateSourceWindowBoundsIfNecessary(
    SplitViewController::SnapPosition candidate_snap_position) {
  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window_);

  if (split_view_controller->IsWindowInSplitView(source_window_))
    return;

  if (!windows_hider_) {
    windows_hider_ =
        std::make_unique<TabletModeBrowserWindowDragSessionWindowsHider>(
            source_window_, nullptr);
  }

  gfx::Rect new_source_window_bounds;
  if (candidate_snap_position == SplitViewController::SnapPosition::NONE) {
    const gfx::Rect area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            root_window_);
    new_source_window_bounds = area;
    new_source_window_bounds.ClampToCenteredSize(gfx::Size(
        area.width() * kSourceWindowScale, area.height() * kSourceWindowScale));
  } else {
    const SplitViewController::SnapPosition opposite_position =
        (candidate_snap_position == SplitViewController::SnapPosition::LEFT)
            ? SplitViewController::SnapPosition::RIGHT
            : SplitViewController::SnapPosition::LEFT;
    new_source_window_bounds =
        SplitViewController::Get(source_window_)
            ->GetSnappedWindowBoundsInScreen(opposite_position, source_window_);
  }
  wm::ConvertRectFromScreen(source_window_->parent(),
                            &new_source_window_bounds);

  if (new_source_window_bounds != source_window_->GetTargetBounds()) {
    ui::ScopedLayerAnimationSettings settings(
        source_window_->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    source_window_->SetBounds(new_source_window_bounds);
  }
}

void TabDragDropDelegate::RestoreSourceWindowBounds() {
  if (SplitViewController::Get(source_window_)
          ->IsWindowInSplitView(source_window_))
    return;

  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  source_window_->SetBounds(area);
}

}  // namespace ash
