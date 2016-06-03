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

namespace mus {
class Window;
class WindowManagerClient;
}

namespace ui {
class CancelModeEvent;
}

namespace ash {
namespace mus {

class WmWindowMus;

// EventHandler attached to windows that may be dragged and/or resized. This
// forwards to WmToplevelWindowEventHandler to handle the actual drag/resize.
class MoveEventHandler : public ui::EventHandler, public aura::WindowObserver {
 public:
  MoveEventHandler(::mus::Window* mus_window,
                   ::mus::WindowManagerClient* window_manager_client,
                   aura::Window* aura_window);
  ~MoveEventHandler() override;

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
  ::mus::WindowManagerClient* window_manager_client_;
  // The root window of the aura::Window supplied to the constructor.
  // MoveEventHandler is added as a pre-target handler (and observer) of this.
  aura::Window* root_window_;
  wm::WmToplevelWindowEventHandler toplevel_window_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(MoveEventHandler);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_MOVE_EVENT_HANDLER_H_
