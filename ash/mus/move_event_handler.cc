// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/move_event_handler.h"

#include "ash/mus/bridge/workspace_event_handler_mus.h"
#include "ash/wm_window.h"
#include "services/ui/public/interfaces/cursor/cursor.mojom.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(ash::mus::MoveEventHandler*);

namespace {

// Key used for storing identifier sent to clients for windows.
DEFINE_UI_CLASS_PROPERTY_KEY(ash::mus::MoveEventHandler*,
                             kWmMoveEventHandler,
                             nullptr);

}  // namespace

namespace ash {
namespace mus {
namespace {

ui::CursorType CursorForWindowComponent(int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return ui::CursorType::kSouthResize;
    case HTBOTTOMLEFT:
      return ui::CursorType::kSouthWestResize;
    case HTBOTTOMRIGHT:
      return ui::CursorType::kSouthEastResize;
    case HTLEFT:
      return ui::CursorType::kWestResize;
    case HTRIGHT:
      return ui::CursorType::kEastResize;
    case HTTOP:
      return ui::CursorType::kNorthResize;
    case HTTOPLEFT:
      return ui::CursorType::kNorthWestResize;
    case HTTOPRIGHT:
      return ui::CursorType::kNorthEastResize;
    default:
      return ui::CursorType::kNull;
  }
}

void OnMoveLoopCompleted(const base::Callback<void(bool success)>& end_closure,
                         wm::WmToplevelWindowEventHandler::DragResult result) {
  end_closure.Run(result ==
                  wm::WmToplevelWindowEventHandler::DragResult::SUCCESS);
}

}  // namespace

MoveEventHandler::MoveEventHandler(
    aura::WindowManagerClient* window_manager_client,
    aura::Window* window)
    : wm_window_(WmWindow::Get(window)),
      window_manager_client_(window_manager_client) {
  window->AddObserver(this);
  window->AddPreTargetHandler(this);

  window->SetProperty(kWmMoveEventHandler, this);
}

MoveEventHandler::~MoveEventHandler() {
  Detach();
}

// static
MoveEventHandler* MoveEventHandler::GetForWindow(WmWindow* wm_window) {
  return WmWindow::GetAuraWindow(wm_window)->GetProperty(kWmMoveEventHandler);
}

void MoveEventHandler::AttemptToStartDrag(
    const gfx::Point& point_in_parent,
    int window_component,
    aura::client::WindowMoveSource source,
    const base::Callback<void(bool success)>& end_closure) {
  toplevel_window_event_handler_.AttemptToStartDrag(
      wm_window_, point_in_parent, window_component, source,
      base::Bind(&OnMoveLoopCompleted, end_closure));
}

bool MoveEventHandler::IsDragInProgress() {
  return toplevel_window_event_handler_.is_drag_in_progress();
}

void MoveEventHandler::RevertDrag() {
  toplevel_window_event_handler_.RevertDrag();
}

void MoveEventHandler::Detach() {
  if (!wm_window_)
    return;

  wm_window_->aura_window()->RemoveObserver(this);
  wm_window_->aura_window()->RemovePreTargetHandler(this);
  wm_window_->aura_window()->ClearProperty(kWmMoveEventHandler);
  wm_window_ = nullptr;
}

WorkspaceEventHandlerMus* MoveEventHandler::GetWorkspaceEventHandlerMus() {
  if (!wm_window_->GetParent())
    return nullptr;

  return WorkspaceEventHandlerMus::Get(wm_window_->aura_window()->parent());
}

void MoveEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  toplevel_window_event_handler_.OnMouseEvent(event, wm_window_);
  if (!toplevel_window_event_handler_.is_drag_in_progress() &&
      (event->type() == ui::ET_POINTER_MOVED ||
       event->type() == ui::ET_MOUSE_MOVED)) {
    const int hit_test_location =
        wm_window_->GetNonClientComponent(event->location());
    window_manager_client_->SetNonClientCursor(
        wm_window_->aura_window(),
        ui::CursorData(CursorForWindowComponent(hit_test_location)));
  }

  WorkspaceEventHandlerMus* workspace_event_handler =
      GetWorkspaceEventHandlerMus();
  if (workspace_event_handler)
    workspace_event_handler->OnMouseEvent(event, wm_window_);
}

void MoveEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  toplevel_window_event_handler_.OnGestureEvent(event, wm_window_);

  WorkspaceEventHandlerMus* workspace_event_handler =
      GetWorkspaceEventHandlerMus();
  if (workspace_event_handler)
    workspace_event_handler->OnGestureEvent(event, wm_window_);
}

void MoveEventHandler::OnCancelMode(ui::CancelModeEvent* event) {
  toplevel_window_event_handler_.RevertDrag();
}

void MoveEventHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(wm_window_->aura_window(), window);
  Detach();
}

}  // namespace mus
}  // namespace ash
