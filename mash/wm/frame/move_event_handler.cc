// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/frame/move_event_handler.h"

#include "components/mus/public/cpp/window.h"
#include "mash/wm/frame/move_loop.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"

namespace mash {
namespace wm {

MoveEventHandler::MoveEventHandler(mus::Window* mus_window,
                                   aura::Window* aura_window)
    : mus_window_(mus_window), aura_window_(aura_window) {}

MoveEventHandler::~MoveEventHandler() {}

void MoveEventHandler::ProcessLocatedEvent(ui::LocatedEvent* event) {
  const bool had_move_loop = move_loop_.get() != nullptr;
  ui::Event* ui_event = static_cast<ui::Event*>(event);
  if (move_loop_) {
    if (move_loop_->Move(*mus::mojom::Event::From(*ui_event)) == MoveLoop::DONE)
      move_loop_.reset();
  } else if (event->type() == ui::ET_MOUSE_PRESSED ||
             event->type() == ui::ET_TOUCH_PRESSED) {
    const int ht_location = ShouldStartMoveLoop(event);
    if (ht_location != HTNOWHERE) {
      // TODO(sky): convert MoveLoop to take ui::Event.
      move_loop_ = MoveLoop::Create(mus_window_, ht_location,
                                    *mus::mojom::Event::From(*ui_event));
    }
  }
  if (had_move_loop || move_loop_)
    event->SetHandled();
}

int MoveEventHandler::ShouldStartMoveLoop(const ui::LocatedEvent* event) {
  return aura_window_->delegate()->GetNonClientComponent(event->location());
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

}  // namespace wm
}  // namespace mash
