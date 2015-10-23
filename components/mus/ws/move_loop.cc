// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/move_loop.h"

#include "base/auto_reset.h"
#include "components/mus/ws/server_window.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/mojo/events/input_event_constants.mojom.h"

namespace mus {
namespace ws {
namespace {

gfx::Point EventLocationToPoint(const mojo::Event& event) {
  return gfx::ToFlooredPoint(gfx::PointF(event.pointer_data->location->x,
                                         event.pointer_data->location->y));
}

gfx::Point EventScreenLocationToPoint(const mojo::Event& event) {
  return gfx::ToFlooredPoint(
      gfx::PointF(event.pointer_data->location->screen_x,
                  event.pointer_data->location->screen_y));
}

}  // namespace

MoveLoop::~MoveLoop() {
  if (target_)
    target_->RemoveObserver(this);
}

// static
scoped_ptr<MoveLoop> MoveLoop::Create(ServerWindow* target,
                                      const mojo::Event& event) {
  DCHECK(event.action == mojo::EVENT_TYPE_POINTER_DOWN);
  const gfx::Point location(EventLocationToPoint(event));
  if (!target->parent() || !target->parent()->is_draggable_window_container() ||
      target->client_area().Contains(location)) {
    return nullptr;
  }

  // Start a move on left mouse, or any other type of pointer.
  if (event.pointer_data->kind == mojo::POINTER_KIND_MOUSE &&
      event.flags != mojo::EVENT_FLAGS_LEFT_MOUSE_BUTTON) {
    return nullptr;
  }

  return make_scoped_ptr(new MoveLoop(target, event));
}

MoveLoop::MoveResult MoveLoop::Move(const mojo::Event& event) {
  switch (event.action) {
    case mojo::EVENT_TYPE_POINTER_CANCEL:
      if (event.pointer_data->pointer_id == pointer_id_) {
        if (target_)
          Revert();
        return MoveResult::DONE;
      }
      return MoveResult::CONTINUE;

    case mojo::EVENT_TYPE_POINTER_MOVE:
      if (target_ && event.pointer_data->pointer_id == pointer_id_)
        MoveImpl(event);
      return MoveResult::CONTINUE;

    case mojo::EVENT_TYPE_POINTER_UP:
      if (event.pointer_data->pointer_id == pointer_id_) {
        // TODO(sky): need to support changed_flags.
        if (target_)
          MoveImpl(event);
        return MoveResult::DONE;
      }
      return MoveResult::CONTINUE;

    default:
      break;
  }

  return MoveResult::CONTINUE;
}

MoveLoop::MoveLoop(ServerWindow* target, const mojo::Event& event)
    : target_(target),
      pointer_id_(event.pointer_data->pointer_id),
      initial_event_screen_location_(EventScreenLocationToPoint(event)),
      initial_window_bounds_(target->bounds()),
      changing_bounds_(false) {
  target->AddObserver(this);
}

void MoveLoop::MoveImpl(const mojo::Event& event) {
  const gfx::Vector2d delta =
      EventScreenLocationToPoint(event) - initial_event_screen_location_;
  const gfx::Rect new_bounds(initial_window_bounds_.origin() + delta,
                             initial_window_bounds_.size());
  base::AutoReset<bool> resetter(&changing_bounds_, true);
  target_->SetBounds(new_bounds);
}

void MoveLoop::Cancel() {
  target_->RemoveObserver(this);
  target_ = nullptr;
}

void MoveLoop::Revert() {
  base::AutoReset<bool> resetter(&changing_bounds_, true);
  target_->SetBounds(initial_window_bounds_);
}

void MoveLoop::OnWindowHierarchyChanged(ServerWindow* window,
                                        ServerWindow* new_parent,
                                        ServerWindow* old_parent) {
  DCHECK_EQ(window, target_);
  Cancel();
}

void MoveLoop::OnWindowBoundsChanged(ServerWindow* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) {
  DCHECK_EQ(window, target_);
  if (!changing_bounds_)
    Cancel();
}

void MoveLoop::OnWindowVisibilityChanged(ServerWindow* window) {
  DCHECK_EQ(window, target_);
  Cancel();
}

}  // namespace ws
}  // namespace mus
