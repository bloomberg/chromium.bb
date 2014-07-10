// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
#define ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ash/wm/wm_types.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/wm/public/window_move_client.h"

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
  ToplevelWindowEventHandler();
  virtual ~ToplevelWindowEventHandler();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden form aura::client::WindowMoveClient:
  virtual aura::client::WindowMoveResult RunMoveLoop(
      aura::Window* source,
      const gfx::Vector2d& drag_offset,
      aura::client::WindowMoveSource move_source) OVERRIDE;
  virtual void EndMoveLoop() OVERRIDE;

  // Overridden form ash::DisplayController::Observer:
  virtual void OnDisplayConfigurationChanging() OVERRIDE;

 private:
  class ScopedWindowResizer;

  enum DragCompletionStatus {
    DRAG_COMPLETE,
    DRAG_REVERT,

    // To be used when WindowResizer::GetTarget() is destroyed. Neither
    // completes nor reverts the drag because both access the WindowResizer's
    // window.
    DRAG_RESIZER_WINDOW_DESTROYED
  };

  // Attempts to start a drag if one is not already in progress. Returns true if
  // successful.
  bool AttemptToStartDrag(aura::Window* window,
                          const gfx::Point& point_in_parent,
                          int window_component,
                          aura::client::WindowMoveSource source);

  // Completes or reverts the drag if one is in progress. Returns true if a
  // drag was completed or reverted.
  bool CompleteDrag(DragCompletionStatus status);

  void HandleMousePressed(aura::Window* target, ui::MouseEvent* event);
  void HandleMouseReleased(aura::Window* target, ui::MouseEvent* event);

  // Called during a drag to resize/position the window.
  void HandleDrag(aura::Window* target, ui::LocatedEvent* event);

  // Called during mouse moves to update window resize shadows.
  void HandleMouseMoved(aura::Window* target, ui::LocatedEvent* event);

  // Called for mouse exits to hide window resize shadows.
  void HandleMouseExited(aura::Window* target, ui::LocatedEvent* event);

  // Sets |window|'s state type to |new_state_type|. Called after the drag has
  // been completed for fling gestures.
  void SetWindowStateTypeFromGesture(aura::Window* window,
                                    wm::WindowStateType new_state_type);

  // Invoked from ScopedWindowResizer if the window is destroyed.
  void ResizerWindowDestroyed();

  // The hittest result for the first finger at the time that it initially
  // touched the screen. |first_finger_hittest_| is one of ui/base/hit_test.h
  int first_finger_hittest_;

  // The window bounds when the drag was started. When a window is minimized,
  // maximized or snapped via a swipe/fling gesture, the restore bounds should
  // be set to the bounds of the window when the drag was started.
  gfx::Rect pre_drag_window_bounds_;

  // Are we running a nested message loop from RunMoveLoop().
  bool in_move_loop_;

  // Is a window move/resize in progress because of gesture events?
  bool in_gesture_drag_;

  // Whether the drag was reverted. Set by CompleteDrag().
  bool drag_reverted_;

  scoped_ptr<ScopedWindowResizer> window_resizer_;

  base::Closure quit_closure_;

  // Used to track if this object is deleted while running a nested message
  // loop. If non-null the destructor sets this to true.
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventHandler);
};

}  // namespace aura

#endif  // ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
