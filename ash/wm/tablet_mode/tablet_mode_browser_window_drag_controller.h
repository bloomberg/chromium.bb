// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_CONTROLLER_H_

#include <memory>

#include "ash/wm/window_resizer.h"
#include "ash/wm/wm_toplevel_window_event_handler.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {
class TabletModeWindowDragDelegate;

namespace wm {
class WindowState;
}  // namespace wm

// WindowResizer implementation for browser windows in tablet mode. Currently we
// don't allow any resizing and any dragging happening on the area other than
// the caption tabs area in tablet mode. Only browser windows with tabs are
// allowed to be dragged. Depending on the event position, the dragged window
// may be 1) maximized, or 2) snapped in splitscreen, or 3) merged to an
// existing window.
class TabletModeBrowserWindowDragController : public WindowResizer {
 public:
  explicit TabletModeBrowserWindowDragController(wm::WindowState* window_state);
  ~TabletModeBrowserWindowDragController() override;

  // WindowResizer:
  void Drag(const gfx::Point& location_in_parent, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;

  TabletModeWindowDragDelegate* drag_delegate_for_testing() {
    return drag_delegate_.get();
  }

 private:
  void EndDragImpl(wm::WmToplevelWindowEventHandler::DragResult result);

  // Scales down the source window if the dragged window is dragged past the
  // |kIndicatorThresholdRatio| threshold and restores it if the dragged window
  // is dragged back toward the top of the screen. |location_in_screen| is the
  // current drag location for the dragged window.
  void UpdateSourceWindow(const gfx::Point& location_in_screen);

  // Shows/Hides/Destroies the scrim widget |scrim_| based on the current
  // location |location_in_screen|.
  void UpdateScrim(const gfx::Point& location_in_screen);

  // Shows the scrim with the specified opacity, blur and expected bounds.
  void ShowScrim(float opacity, float blur, const gfx::Rect& bounds_in_screen);

  // A widget placed below the current dragged window to show the blurred or
  // transparent background and to prevent the dragged window merge into any
  // browser window beneath it during dragging.
  std::unique_ptr<views::Widget> scrim_;

  gfx::Point previous_location_in_parent_;

  bool did_lock_cursor_ = false;

  std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate_;

  // Used to determine if this has been deleted during a drag such as when a tab
  // gets dragged into another browser window.
  base::WeakPtrFactory<TabletModeBrowserWindowDragController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeBrowserWindowDragController);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_CONTROLLER_H_
