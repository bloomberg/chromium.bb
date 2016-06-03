// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/frame/move_event_handler.h"

#include "ash/mus/bridge/wm_window_mus.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/interfaces/cursor.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"

namespace ash {
namespace mus {
namespace {

::mus::mojom::Cursor CursorForWindowComponent(int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return ::mus::mojom::Cursor::SOUTH_RESIZE;
    case HTBOTTOMLEFT:
      return ::mus::mojom::Cursor::SOUTH_WEST_RESIZE;
    case HTBOTTOMRIGHT:
      return ::mus::mojom::Cursor::SOUTH_EAST_RESIZE;
    case HTLEFT:
      return ::mus::mojom::Cursor::WEST_RESIZE;
    case HTRIGHT:
      return ::mus::mojom::Cursor::EAST_RESIZE;
    case HTTOP:
      return ::mus::mojom::Cursor::NORTH_RESIZE;
    case HTTOPLEFT:
      return ::mus::mojom::Cursor::NORTH_WEST_RESIZE;
    case HTTOPRIGHT:
      return ::mus::mojom::Cursor::NORTH_EAST_RESIZE;
    default:
      return ::mus::mojom::Cursor::CURSOR_NULL;
  }
}

}  // namespace

MoveEventHandler::MoveEventHandler(
    ::mus::Window* mus_window,
    ::mus::WindowManagerClient* window_manager_client,
    aura::Window* aura_window)
    : wm_window_(WmWindowMus::Get(mus_window)),
      window_manager_client_(window_manager_client),
      root_window_(aura_window->GetRootWindow()),
      toplevel_window_event_handler_(wm_window_->GetShell()) {
  root_window_->AddObserver(this);
  root_window_->AddPreTargetHandler(this);
}

MoveEventHandler::~MoveEventHandler() {
  Detach();
}

void MoveEventHandler::Detach() {
  if (!root_window_)
    return;

  root_window_->RemoveObserver(this);
  root_window_->RemovePreTargetHandler(this);
  root_window_ = nullptr;
}

void MoveEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  toplevel_window_event_handler_.OnMouseEvent(event, wm_window_);
  if (!toplevel_window_event_handler_.is_drag_in_progress() &&
      (event->type() == ui::ET_POINTER_MOVED ||
       event->type() == ui::ET_MOUSE_MOVED)) {
    const int hit_test_location =
        wm_window_->GetNonClientComponent(event->location());
    window_manager_client_->SetNonClientCursor(
        wm_window_->mus_window(), CursorForWindowComponent(hit_test_location));
  }
}

void MoveEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  toplevel_window_event_handler_.OnGestureEvent(event, wm_window_);
}

void MoveEventHandler::OnCancelMode(ui::CancelModeEvent* event) {
  toplevel_window_event_handler_.RevertDrag();
}

void MoveEventHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(root_window_, window);
  Detach();
}

}  // namespace mus
}  // namespace ash
