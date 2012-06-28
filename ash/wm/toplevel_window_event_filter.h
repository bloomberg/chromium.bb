// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#define ASH_WM_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#pragma once

#include <set>

#include "base/callback.h"
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

class WindowResizer;

class ASH_EXPORT ToplevelWindowEventFilter :
      public aura::EventFilter,
      public aura::client::WindowMoveClient {
 public:
  explicit ToplevelWindowEventFilter(aura::Window* owner);
  virtual ~ToplevelWindowEventFilter();

  // Sets the size of the grid. If non-zero all resizes and moves are forced to
  // fall on a grid of the specified size. The default is 0, meaning the x,y and
  // width,height are not restricted in anyway.
  void set_grid_size(int size) { grid_size_ = size; }
  int grid_size() const { return grid_size_; }

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
  // Creates a new WindowResizer.
  virtual WindowResizer* CreateWindowResizer(aura::Window* window,
                                             const gfx::Point& point,
                                             int window_component);

 private:
  enum DragCompletionStatus {
    DRAG_COMPLETE,
    DRAG_REVERT
  };

  // Finishes the drag.
  void CompleteDrag(DragCompletionStatus status, int event_flags);

  // Called during a drag to resize/position the window.
  // The return value is returned by OnMouseEvent() above.
  bool HandleDrag(aura::Window* target, aura::LocatedEvent* event);

  // Called during mouse moves to update window resize shadows.
  // Return value is returned by OnMouseEvent() above.
  bool HandleMouseMoved(aura::Window* target, aura::LocatedEvent* event);

  // Called for mouse exits to hide window resize shadows.
  // Return value is returned by OnMouseEvent() above.
  bool HandleMouseExited(aura::Window* target, aura::LocatedEvent* event);

  // Are we running a nested message loop from RunMoveLoop().
  bool in_move_loop_;

  // Is a gesture-resize in progress?
  bool in_gesture_resize_;

  // See description above setter.
  int grid_size_;

  scoped_ptr<WindowResizer> window_resizer_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventFilter);
};

}  // namespace aura

#endif  // ASH_WM_TOPLEVEL_WINDOW_EVENT_FILTER_H_
