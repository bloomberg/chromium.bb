// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/macros.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

class PhantomWindowController;
class WindowSelector;
class WindowSelectorItem;

// The drag controller for an overview window item in overview mode. It updates
// the position of the corresponding window item using transform while dragging
// and shows/hides the phantom window accordingly.
class ASH_EXPORT OverviewWindowDragController {
 public:
  // Snapping distance between the dragged window with the screen edge. It's
  // useful especially for touch events.
  static constexpr int kScreenEdgeInsetForDrag = 200;

  explicit OverviewWindowDragController(WindowSelector* window_selector);
  ~OverviewWindowDragController();

  void InitiateDrag(WindowSelectorItem* item,
                    const gfx::Point& location_in_screen);
  void Drag(const gfx::Point& location_in_screen);
  void CompleteDrag(const gfx::Point& location_in_screen);

  // Resets |window_selector_| to nullptr. It's needed since we defer the
  // deletion of OverviewWindowDragController in WindowSelector destructor and
  // we need to reset |window_selector_| to nullptr to avoid null pointer
  // dereference.
  void ResetWindowSelector();

  WindowSelectorItem* item() { return item_; }

  bool IsPhantomWindowShowing() const {
    return phantom_window_controller_ != nullptr;
  }

 private:
  // Updates visuals for the user while dragging items around.
  void UpdatePhantomWindowAndWindowGrid(const gfx::Point& location_in_screen);

  SplitViewController::SnapPosition GetSnapPosition(
      const gfx::Point& location_in_screen) const;

  // Returns the expected window grid bounds based on |snap_position|.
  gfx::Rect GetGridBounds(SplitViewController::SnapPosition snap_position);

  void SnapWindow(SplitViewController::SnapPosition snap_position);

  WindowSelector* window_selector_;

  SplitViewController* split_view_controller_;

  // Shows a highlight of where the dragged window will end up.
  std::unique_ptr<PhantomWindowController> phantom_window_controller_;

  // The drag target window in the overview mode.
  WindowSelectorItem* item_ = nullptr;

  // The location of the previous mouse/touch/gesture event in screen.
  gfx::Point previous_event_location_;

  // Set to true once the bounds of |item_| change.
  bool did_move_ = false;

  SplitViewController::SnapPosition snap_position_ = SplitViewController::NONE;

  DISALLOW_COPY_AND_ASSIGN(OverviewWindowDragController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_
