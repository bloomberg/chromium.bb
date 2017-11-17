// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/macros.h"
#include "base/optional.h"
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
  // The minimum offset that will be considered as a drag event.
  static constexpr int kMinimumDragOffset = 5;
  // The minimum offset that an item must be moved before it is considered a
  // drag event, if the drag starts in one of the snap regions.
  static constexpr int kMinimumDragOffsetAlreadyInSnapRegionDp = 48;

  explicit OverviewWindowDragController(WindowSelector* window_selector);
  ~OverviewWindowDragController();

  void InitiateDrag(WindowSelectorItem* item,
                    const gfx::Point& location_in_screen);
  void Drag(const gfx::Point& location_in_screen);
  void CompleteDrag(const gfx::Point& location_in_screen);
  void ActivateDraggedWindow();
  void ResetGesture();

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

  // Dragged items should not attempt to show the phantom window or snap if
  // the drag started in a snap region and has not been dragged pass the
  // threshold.
  bool ShouldUpdatePhantomWindowOrSnap(const gfx::Point& event_location);

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

  // The location of the initial mouse/touch/gesture event in screen. It is
  // nullopt if the initial drag location was not a snap region, or if the it
  // was a snap region but the drag has since moved out.
  base::Optional<gfx::Point> initial_event_location_;

  // Set to true once the bounds of |item_| change.
  bool did_move_ = false;

  SplitViewController::SnapPosition snap_position_ = SplitViewController::NONE;

  DISALLOW_COPY_AND_ASSIGN(OverviewWindowDragController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_
