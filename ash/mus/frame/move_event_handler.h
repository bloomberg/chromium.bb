// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_FRAME_MOVE_EVENT_HANDLER_H_
#define ASH_MUS_FRAME_MOVE_EVENT_HANDLER_H_

#include <memory>

#include "ash/common/wm/wm_toplevel_window_event_handler.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace ui {
class CancelModeEvent;
class Window;
class WindowManagerClient;
}

namespace ash {
namespace mus {

class WmWindowMus;

// EventHandler attached to windows that may be dragged and/or resized. This
// forwards to WmToplevelWindowEventHandler to handle the actual drag/resize.
class MoveEventHandler : public ui::EventHandler, public aura::WindowObserver {
 public:
  MoveEventHandler(::ui::Window* mus_window,
                   ::ui::WindowManagerClient* window_manager_client,
                   aura::Window* aura_window);
  ~MoveEventHandler() override;

  // Retrieves the MoveEventHandler for an existing WmWindow.
  static MoveEventHandler* GetForWindow(WmWindow* wm_window);

  // Attempts to start a drag if one is not already in progress. This passes
  // the call to the underlying WmToplevelWindowEventHandler. After the drag
  // completes, |end_closure| will be called to return whether the drag was
  // successfully completed.
  void AttemptToStartDrag(
      const gfx::Point& point_in_parent,
      int window_component,
      aura::client::WindowMoveSource source,
      const base::Callback<void(bool success)>& end_closure);

  // Returns whether we're in a drag.
  bool IsDragInProgress();

  // Reverts a manually started drag started with AttemptToStartDrag().
  void RevertDrag();

 private:
  // Removes observer and EventHandler installed on |root_window_|.
  void Detach();

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnCancelMode(ui::CancelModeEvent* event) override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  WmWindowMus* wm_window_;
  ::ui::WindowManagerClient* window_manager_client_;
  // The root window of the aura::Window supplied to the constructor.
  // MoveEventHandler is added as a pre-target handler (and observer) of this.
  aura::Window* root_window_;
  wm::WmToplevelWindowEventHandler toplevel_window_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(MoveEventHandler);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_MOVE_EVENT_HANDLER_H_
