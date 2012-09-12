// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
#define ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/window_move_client.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ui {
class LocatedEvent;
}

namespace ash {

class WindowResizer;

class ASH_EXPORT ToplevelWindowEventHandler
    : public ui::EventHandler,
      public aura::client::WindowMoveClient,
      public DisplayController::Observer {
 public:
  explicit ToplevelWindowEventHandler(aura::Window* owner);
  virtual ~ToplevelWindowEventHandler();

  // Overridden from ui::EventHandler:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual ui::TouchStatus OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden form aura::client::WindowMoveClient:
  virtual aura::client::WindowMoveResult RunMoveLoop(
      aura::Window* source,
      const gfx::Point& drag_offset) OVERRIDE;
  virtual void EndMoveLoop() OVERRIDE;

  // Overridden form ash::DisplayController::Observer:
  virtual void OnDisplayConfigurationChanging() OVERRIDE;

 protected:
  // Creates a new WindowResizer.
  virtual WindowResizer* CreateWindowResizer(aura::Window* window,
                                             const gfx::Point& point_in_parent,
                                             int window_component);

 private:
  class ScopedWindowResizer;

  enum DragCompletionStatus {
    DRAG_COMPLETE,
    DRAG_REVERT
  };

  void CreateScopedWindowResizer(aura::Window* window,
                                 const gfx::Point& point_in_parent,
                                 int window_component);

  // Finishes the drag.
  void CompleteDrag(DragCompletionStatus status, int event_flags);

  ui::EventResult HandleMousePressed(aura::Window* target,
                                     ui::MouseEvent* event);
  ui::EventResult HandleMouseReleased(aura::Window* target,
                                      ui::MouseEvent* event);

  // Called during a drag to resize/position the window.
  // The return value is returned by OnMouseEvent() above.
  ui::EventResult HandleDrag(aura::Window* target, ui::LocatedEvent* event);

  // Called during mouse moves to update window resize shadows.
  // Return value is returned by OnMouseEvent() above.
  ui::EventResult HandleMouseMoved(aura::Window* target,
                                   ui::LocatedEvent* event);

  // Called for mouse exits to hide window resize shadows.
  // Return value is returned by OnMouseEvent() above.
  ui::EventResult HandleMouseExited(aura::Window* target,
                                    ui::LocatedEvent* event);

  // Invoked from ScopedWindowResizer if the window is destroyed.
  void ResizerWindowDestroyed();

  // Are we running a nested message loop from RunMoveLoop().
  bool in_move_loop_;

  // Was the move operation cancelled? Used only when the nested loop
  // is used to move a window.
  bool move_cancelled_;

  // Is a gesture-resize in progress?
  bool in_gesture_resize_;

  scoped_ptr<ScopedWindowResizer> window_resizer_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventHandler);
};

}  // namespace aura

#endif  // ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
