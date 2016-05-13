// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/frame/move_event_handler.h"

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/interfaces/cursor.mojom.h"
#include "mash/wm/frame/move_loop.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"

namespace mash {
namespace wm {
namespace {

mus::mojom::Cursor CursorForWindowComponent(int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return mus::mojom::Cursor::SOUTH_RESIZE;
    case HTBOTTOMLEFT:
      return mus::mojom::Cursor::SOUTH_WEST_RESIZE;
    case HTBOTTOMRIGHT:
      return mus::mojom::Cursor::SOUTH_EAST_RESIZE;
    case HTLEFT:
      return mus::mojom::Cursor::WEST_RESIZE;
    case HTRIGHT:
      return mus::mojom::Cursor::EAST_RESIZE;
    case HTTOP:
      return mus::mojom::Cursor::NORTH_RESIZE;
    case HTTOPLEFT:
      return mus::mojom::Cursor::NORTH_WEST_RESIZE;
    case HTTOPRIGHT:
      return mus::mojom::Cursor::NORTH_EAST_RESIZE;
    default:
      return mus::mojom::Cursor::CURSOR_NULL;
  }
}

}  // namespace

MoveEventHandler::MoveEventHandler(
    mus::Window* mus_window,
    mus::WindowManagerClient* window_manager_client,
    aura::Window* aura_window)
    : mus_window_(mus_window),
      window_manager_client_(window_manager_client),
      aura_window_(aura_window),
      root_window_(aura_window->GetRootWindow()) {
  root_window_->AddObserver(this);
  root_window_->AddPreTargetHandler(this);
}

MoveEventHandler::~MoveEventHandler() {
  Detach();
}

void MoveEventHandler::ProcessLocatedEvent(ui::LocatedEvent* event) {
  const bool had_move_loop = move_loop_.get() != nullptr;
  DCHECK(event->IsMouseEvent() || event->IsTouchEvent());

  // This event handler can receive mouse events like ET_MOUSE_CAPTURE_CHANGED
  // that cannot be converted to PointerEvents. Ignore them because they aren't
  // needed for move handling.
  if (!ui::PointerEvent::CanConvertFrom(*event))
    return;

  // TODO(moshayedi): no need for this once MoveEventHandler directly receives
  // pointer events.
  std::unique_ptr<ui::PointerEvent> pointer_event;
  if (event->IsMouseEvent())
    pointer_event.reset(new ui::PointerEvent(*event->AsMouseEvent()));
  else
    pointer_event.reset(new ui::PointerEvent(*event->AsTouchEvent()));

  if (move_loop_) {
    if (move_loop_->Move(*pointer_event.get()) == MoveLoop::DONE)
      move_loop_.reset();
  } else if (pointer_event->type() == ui::ET_POINTER_DOWN) {
    const int ht_location = GetNonClientComponentForEvent(pointer_event.get());
    if (ht_location != HTNOWHERE) {
      move_loop_ =
          MoveLoop::Create(mus_window_, ht_location, *pointer_event.get());
    }
  } else if (pointer_event->type() == ui::ET_POINTER_MOVED) {
    const int ht_location = GetNonClientComponentForEvent(pointer_event.get());
    window_manager_client_->SetNonClientCursor(
        mus_window_, CursorForWindowComponent(ht_location));
  }
  if (had_move_loop || move_loop_)
    event->SetHandled();
}

int MoveEventHandler::GetNonClientComponentForEvent(
    const ui::LocatedEvent* event) {
  return aura_window_->delegate()->GetNonClientComponent(event->location());
}

void MoveEventHandler::Detach() {
  if (!root_window_)
    return;

  root_window_->RemoveObserver(this);
  root_window_->RemovePreTargetHandler(this);
  root_window_ = nullptr;
}

void MoveEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  ProcessLocatedEvent(event);
}

void MoveEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  ProcessLocatedEvent(event);
}

void MoveEventHandler::OnCancelMode(ui::CancelModeEvent* event) {
  if (!move_loop_)
    return;

  move_loop_->Revert();
  move_loop_.reset();
  event->SetHandled();
}

void MoveEventHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(root_window_, window);
  Detach();
}

}  // namespace wm
}  // namespace mash
