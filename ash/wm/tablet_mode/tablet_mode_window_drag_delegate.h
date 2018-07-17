// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/wm_toplevel_window_event_handler.h"

namespace ash {

enum class IndicatorState;
class SplitViewDragIndicators;

// This class includes the common logic when dragging a window around, either
// it's a browser window, or an app window. It does everything needs to be done
// other than updating the dragged window (which is done by DragController),
// e.g., it decides when/where to show the drag indicators and preview windows,
// shows/hides the backdrop, interacts with overview and splitview, etc.
class TabletModeWindowDragDelegate {
 public:
  TabletModeWindowDragDelegate();
  virtual ~TabletModeWindowDragDelegate();

  // Called when a window starts/continues/ends being dragged around.
  void StartWindowDrag(aura::Window* window,
                       const gfx::Point& location_in_screen);
  void ContinueWindowDrag(const gfx::Point& location_in_screen);
  void EndWindowDrag(wm::WmToplevelWindowEventHandler::DragResult result,
                     const gfx::Point& location_in_screen);

  aura::Window* dragged_window() { return dragged_window_; }

  SplitViewDragIndicators* split_view_drag_indicators_for_testing() {
    return split_view_drag_indicators_.get();
  }

 protected:
  // These three methods are used by its child class to do its special handling
  // before/during/after dragging.
  virtual void PrepareForDraggedWindow(
      const gfx::Point& location_in_screen) = 0;
  virtual void UpdateForDraggedWindow(const gfx::Point& location_in_screen) = 0;
  virtual void EndingForDraggedWindow(
      wm::WmToplevelWindowEventHandler::DragResult result,
      const gfx::Point& location_in_screen) = 0;

  // Returns true if we should open overview behind the dragged window when drag
  // starts.
  virtual bool ShouldOpenOverviewWhenDragStarts();

  // When the dragged window is dragged past this value, the drag indicators
  // will show up.
  int GetIndicatorsVerticalThreshold(const gfx::Rect& work_area_bounds) const;

  // When the dragged window is dragged past this value, a blured scrim will
  // show up, indicating the dragged window will be maximized after releasing.
  int GetMaximizeVerticalThreshold(const gfx::Rect& work_area_bounds) const;

  // Gets the desired snap position for |location_in_screen|.
  SplitViewController::SnapPosition GetSnapPosition(
      const gfx::Point& location_in_screen) const;

  // Returns the IndicatorState according to |location_in_screen|.
  IndicatorState GetIndicatorState(const gfx::Point& location_in_screen) const;

  SplitViewController* const split_view_controller_;

  // A widget to display the drag indicators and preview window.
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  aura::Window* dragged_window_ = nullptr;  // not owned.

  // The backdrop should be disabled during dragging and resumed after dragging.
  BackdropWindowMode original_backdrop_mode_ = BackdropWindowMode::kAuto;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeWindowDragDelegate);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_
