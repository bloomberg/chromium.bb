// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
#define ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/common/wm/wm_toplevel_window_event_handler.h"
#include "ash/common/wm/wm_types.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/window_move_client.h"

namespace aura {
class Window;
}

namespace base {
class RunLoop;
}

namespace ash {
namespace wm {
}

class ASH_EXPORT ToplevelWindowEventHandler
    : public ui::EventHandler,
      public aura::client::WindowMoveClient {
 public:
  explicit ToplevelWindowEventHandler(WmShell* shell);
  ~ToplevelWindowEventHandler() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden form aura::client::WindowMoveClient:
  aura::client::WindowMoveResult RunMoveLoop(
      aura::Window* source,
      const gfx::Vector2d& drag_offset,
      aura::client::WindowMoveSource move_source) override;
  void EndMoveLoop() override;

 private:
  // Callback from WmToplevelWindowEventHandler once the drag completes.
  void OnDragCompleted(
      wm::WmToplevelWindowEventHandler::DragResult* result_return_value,
      base::RunLoop* run_loop,
      wm::WmToplevelWindowEventHandler::DragResult result);

  wm::WmToplevelWindowEventHandler wm_toplevel_window_event_handler_;

  // Are we running a nested message loop from RunMoveLoop().
  bool in_move_loop_ = false;

  base::WeakPtrFactory<ToplevelWindowEventHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventHandler);
};

}  // namespace ash

#endif  // ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
