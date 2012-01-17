// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#define ASH_WM_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/window_move_client.h"
#include "ui/aura/event_filter.h"
#include "ash/ash_export.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace aura {
class LocatedEvent;
class MouseEvent;
class Window;
}

namespace ash {

class ASH_EXPORT ToplevelWindowEventFilter :
      public aura::EventFilter,
      public aura::client::WindowMoveClient {
 public:
  explicit ToplevelWindowEventFilter(aura::Window* owner);
  virtual ~ToplevelWindowEventFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  // Overridden form aura::client::WindowMoveClient:
  virtual void RunMoveLoop(aura::Window* source) OVERRIDE;
  virtual void EndMoveLoop() OVERRIDE;

 protected:
  // Returns the |window_component_|. See the variable definition below for
  // more details.
  int window_component() const { return window_component_; }

 private:
  // Moves the target window and all of its parents to the front of their
  // respective z-orders.
  // NOTE: this does NOT activate the window.
  void MoveWindowToFront(aura::Window* target);

  // Called during a drag to resize/position the window.
  // The return value is returned by OnMouseEvent() above.
  bool HandleDrag(aura::Window* target, aura::LocatedEvent* event);

  // Updates |mouse_down_offset_in_parent_| and |mouse_down_bounds_| from
  // |location|.
  void UpdateMouseDownLocation(aura::Window* target,
                               const gfx::Point& location);

  // Updates the |window_component_| using the |event|'s location.
  void UpdateWindowComponentForEvent(aura::Window* window,
                                     aura::LocatedEvent* event);

  // Calculates the new origin of the window during a drag.
  gfx::Point GetOriginForDrag(int bounds_change,
                              int delta_x,
                              int delta_y) const;

  // Calculates the new size of the |target| window during a drag.
  // If the size is constrained, |delta_x| and |delta_y| may be clamped.
  gfx::Size GetSizeForDrag(int bounds_change,
                           aura::Window* target,
                           int* delta_x,
                           int* delta_y) const;

  // Calculates new width of a window during a drag where the mouse
  // position changed by |delta_x|.  |delta_x| may be clamped if the window
  // size is constrained by |min_width|.
  int GetWidthForDrag(aura::Window* target,
                      int size_change_direction,
                      int min_width,
                      int* delta_x) const;

  // Calculates new height of a window during a drag where the mouse
  // position changed by |delta_y|.  |delta_y| may be clamped if the window
  // size is constrained by |min_height|.
  int GetHeightForDrag(aura::Window* target,
                       int size_change_direction,
                       int min_height,
                       int* delta_y) const;

  // The mouse position in the target window when the mouse was pressed, in
  // the target window's parent's coordinates.
  gfx::Point mouse_down_offset_in_parent_;

  // The bounds of the target window when the mouse was pressed.
  gfx::Rect mouse_down_bounds_;

  // Are we running a nested message loop from RunMoveLoop().
  bool in_move_loop_;

  // The window component (hit-test code) the mouse is currently over.
  int window_component_;

  // Set of touch ids currently pressed.
  std::set<int> pressed_touch_ids_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventFilter);
};

}  // namespace aura

#endif  // ASH_WM_TOPLEVEL_WINDOW_EVENT_FILTER_H_
