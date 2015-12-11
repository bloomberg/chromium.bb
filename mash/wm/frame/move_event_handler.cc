// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/frame/move_event_handler.h"

#include "components/mus/public/cpp/window.h"
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
      return mus::mojom::Cursor::CURSOR_SOUTH_RESIZE;
    case HTBOTTOMLEFT:
      return mus::mojom::Cursor::CURSOR_SOUTH_WEST_RESIZE;
    case HTBOTTOMRIGHT:
      return mus::mojom::Cursor::CURSOR_SOUTH_EAST_RESIZE;
    case HTLEFT:
      return mus::mojom::Cursor::CURSOR_WEST_RESIZE;
    case HTRIGHT:
      return mus::mojom::Cursor::CURSOR_EAST_RESIZE;
    case HTTOP:
      return mus::mojom::Cursor::CURSOR_NORTH_RESIZE;
    case HTTOPLEFT:
      return mus::mojom::Cursor::CURSOR_NORTH_WEST_RESIZE;
    case HTTOPRIGHT:
      return mus::mojom::Cursor::CURSOR_NORTH_EAST_RESIZE;
    default:
      return mus::mojom::Cursor::CURSOR_NULL;
  }
}

}  // namespace

MoveEventHandler::MoveEventHandler(mus::Window* mus_window,
                                   aura::Window* aura_window)
    : mus_window_(mus_window), aura_window_(aura_window),
      root_window_(aura_window->GetRootWindow()) {
  root_window_->AddObserver(this);
  root_window_->AddPreTargetHandler(this);
}

MoveEventHandler::~MoveEventHandler() {
  Detach();
}

void MoveEventHandler::ProcessLocatedEvent(ui::LocatedEvent* event) {
  const bool had_move_loop = move_loop_.get() != nullptr;
  ui::Event* ui_event = static_cast<ui::Event*>(event);
  if (move_loop_) {
    if (move_loop_->Move(*mus::mojom::Event::From(*ui_event)) == MoveLoop::DONE)
      move_loop_.reset();
  } else if (event->type() == ui::ET_MOUSE_PRESSED ||
             event->type() == ui::ET_TOUCH_PRESSED) {
    const int ht_location = GetNonClientComponentForEvent(event);
    if (ht_location != HTNOWHERE) {
      // TODO(sky): convert MoveLoop to take ui::Event.
      move_loop_ = MoveLoop::Create(mus_window_, ht_location,
                                    *mus::mojom::Event::From(*ui_event));
    }
  } else if (event->type() == ui::ET_MOUSE_MOVED) {
    const int ht_location = GetNonClientComponentForEvent(event);
    mus_window_->SetPredefinedCursor(CursorForWindowComponent(ht_location));
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
