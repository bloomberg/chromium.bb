// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H
#define ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ash {
class DisplayEdgeController;

// An event filter that controls mouse location in extended desktop
// environment.
class ASH_EXPORT MouseCursorEventFilter
    : public ui::EventHandler,
      public WindowTreeHostManager::Observer {
 public:
  MouseCursorEventFilter();
  ~MouseCursorEventFilter() override;

  void set_mouse_warp_enabled(bool enabled) { mouse_warp_enabled_ = enabled; }

  // Shows/Hide the indicator for window dragging. The |from|
  // is the window where the dragging started.
  void ShowSharedEdgeIndicator(aura::Window* from);
  void HideSharedEdgeIndicator();

  // WindowTreeHostManager::Observer:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanged() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  friend class test::DisplayManagerTestApi;
  friend class ExtendedMouseWarpControllerTest;
  friend class MouseCursorEventFilterTest;
  friend class UnifiedMouseWarpControllerTest;
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, DoNotWarpTwice);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, SetMouseWarpModeFlag);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           WarpMouseDifferentScaleDisplaysInNative);
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, WarpMousePointer);

  MouseWarpController* mouse_warp_controller_for_test() {
    return mouse_warp_controller_.get();
  }

  bool mouse_warp_enabled_;

  scoped_ptr<MouseWarpController> mouse_warp_controller_;

  DISALLOW_COPY_AND_ASSIGN(MouseCursorEventFilter);
};

}  // namespace ash

#endif  // ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H
