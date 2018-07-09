// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/wm_toplevel_window_event_handler.h"

namespace ash {

enum class IndicatorState;
class SplitViewDragIndicators;

// This class includes the common logic that can be used by both
// TabletModeBrowserWindowDragController and TabletModeAppWindowDragController.
class ASH_EXPORT TabletModeWindowDragDelegate {
 public:
  TabletModeWindowDragDelegate();
  virtual ~TabletModeWindowDragDelegate();

  SplitViewDragIndicators* split_view_drag_indicators_for_testing() {
    return split_view_drag_indicators_.get();
  }

  // When the dragged window is dragged past this value, the drag indicators
  // will show up.
  static int GetIndicatorsVerticalThreshold(const gfx::Rect& work_area_bounds);

  // When the dragged window is dragged past this value, a blured scrim will
  // show up, indicating the dragged window will be maximized after releasing.
  static int GetMaximizeVerticalThreshold(const gfx::Rect& work_area_bounds);

  // Called when a browser/app window start/end being dragging around.
  void OnWindowDragStarted(aura::Window* window);
  void OnWindowDragEnded(wm::WmToplevelWindowEventHandler::DragResult result,
                         const gfx::Point& location_in_screen);

  // Updates the split view drag indicators according to |location_in_screen|.
  void UpdateIndicatorState(const gfx::Point& location_in_screen);

  // Gets the desired snap position for |location_in_screen|.
  SplitViewController::SnapPosition GetSnapPosition(
      const gfx::Point& location_in_screen) const;

  SplitViewController* split_view_controller() {
    return split_view_controller_;
  }
  aura::Window* dragged_window() { return dragged_window_; }

 private:
  // Returns the IndicatorState according to |location_in_screen|.
  IndicatorState GetIndicatorState(const gfx::Point& location_in_screen);

  SplitViewController* const split_view_controller_;

  // A widget to display the drag indicators and preview window.
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  aura::Window* dragged_window_ = nullptr;  // not owned.

  // The backdrop should be disabled during dragging and resumed after dragging.
  BackdropWindowMode original_backdrop_mode_ = BackdropWindowMode::kAuto;

  DISALLOW_COPY_AND_ASSIGN(TabletModeWindowDragDelegate);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_
