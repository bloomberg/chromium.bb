// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/move_loop.h"

#include "base/auto_reset.h"
#include "components/mus/example/wm/property_util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/input_event_constants.mojom.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"

namespace {

gfx::Point EventLocationToPoint(const mus::mojom::Event& event) {
  return gfx::ToFlooredPoint(gfx::PointF(event.pointer_data->location->x,
                                         event.pointer_data->location->y));
}

gfx::Point EventScreenLocationToPoint(const mus::mojom::Event& event) {
  return gfx::ToFlooredPoint(
      gfx::PointF(event.pointer_data->location->screen_x,
                  event.pointer_data->location->screen_y));
}

mus::mojom::EventFlags MouseOnlyEventFlags(mus::mojom::EventFlags flags) {
  return static_cast<mus::mojom::EventFlags>(
      flags & (mus::mojom::EVENT_FLAGS_LEFT_MOUSE_BUTTON |
               mus::mojom::EVENT_FLAGS_MIDDLE_MOUSE_BUTTON |
               mus::mojom::EVENT_FLAGS_RIGHT_MOUSE_BUTTON));
}

gfx::Rect ClientAreaBounds(const mus::Window* window) {
  gfx::Rect client_area(window->bounds().size());
  client_area.Inset(window->client_area());
  return client_area;
}

}  // namespace

MoveLoop::~MoveLoop() {
  if (target_)
    target_->RemoveObserver(this);
}

// static
scoped_ptr<MoveLoop> MoveLoop::Create(mus::Window* target,
                                      const mus::mojom::Event& event) {
  DCHECK_EQ(event.action, mus::mojom::EVENT_TYPE_POINTER_DOWN);
  const gfx::Point location(EventLocationToPoint(event));
  if (!gfx::Rect(target->bounds().size()).Contains(location) ||
      ClientAreaBounds(target).Contains(location)) {
    return nullptr;
  }

  // Start a move on left mouse, or any other type of pointer.
  if (event.pointer_data->kind == mus::mojom::POINTER_KIND_MOUSE &&
      MouseOnlyEventFlags(event.flags) !=
          mus::mojom::EVENT_FLAGS_LEFT_MOUSE_BUTTON) {
    return nullptr;
  }

  Type type;
  HorizontalLocation h_loc;
  VerticalLocation v_loc;
  DetermineType(target, location, &type, &h_loc, &v_loc);

  return make_scoped_ptr(new MoveLoop(target, event, type, h_loc, v_loc));
}

MoveLoop::MoveResult MoveLoop::Move(const mus::mojom::Event& event) {
  switch (event.action) {
    case mus::mojom::EVENT_TYPE_POINTER_CANCEL:
      if (event.pointer_data->pointer_id == pointer_id_) {
        if (target_)
          Revert();
        return MoveResult::DONE;
      }
      return MoveResult::CONTINUE;

    case mus::mojom::EVENT_TYPE_POINTER_MOVE:
      if (target_ && event.pointer_data->pointer_id == pointer_id_)
        MoveImpl(event);
      return MoveResult::CONTINUE;

    case mus::mojom::EVENT_TYPE_POINTER_UP:
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

MoveLoop::MoveLoop(mus::Window* target,
                   const mus::mojom::Event& event,
                   Type type,
                   HorizontalLocation h_loc,
                   VerticalLocation v_loc)
    : target_(target),
      type_(type),
      h_loc_(h_loc),
      v_loc_(v_loc),
      pointer_id_(event.pointer_data->pointer_id),
      initial_event_screen_location_(EventScreenLocationToPoint(event)),
      initial_window_bounds_(target->bounds()),
      initial_user_set_bounds_(GetWindowUserSetBounds(target)),
      changing_bounds_(false) {
  target->AddObserver(this);
}

// static
void MoveLoop::DetermineType(mus::Window* target,
                             const gfx::Point& location,
                             Type* type,
                             HorizontalLocation* h_loc,
                             VerticalLocation* v_loc) {
  *h_loc = HorizontalLocation::OTHER;
  *v_loc = VerticalLocation::OTHER;
  const int resize_size = static_cast<int>(
      kResizeSize *
      std::max(1.f, target->viewport_metrics().device_pixel_ratio));

  const gfx::Rect client_area(ClientAreaBounds(target));
  if (location.x() < client_area.x())
    *h_loc = HorizontalLocation::LEFT;
  else if (location.x() >= client_area.right())
    *h_loc = HorizontalLocation::RIGHT;
  else
    *h_loc = HorizontalLocation::OTHER;

  if (location.y() < resize_size)
    *v_loc = VerticalLocation::TOP;
  else if (location.y() >= client_area.bottom())
    *v_loc = VerticalLocation::BOTTOM;
  else
    *v_loc = VerticalLocation::OTHER;

  if (*v_loc == VerticalLocation::OTHER && location.y() >= resize_size &&
      *h_loc == HorizontalLocation::OTHER) {
    *type = Type::MOVE;
    return;
  }
  *type = Type::RESIZE;
  DCHECK(*h_loc != HorizontalLocation::OTHER ||
         *v_loc != VerticalLocation::OTHER);
}

void MoveLoop::MoveImpl(const mus::mojom::Event& event) {
  const gfx::Vector2d delta =
      EventScreenLocationToPoint(event) - initial_event_screen_location_;
  const gfx::Rect new_bounds(DetermineBoundsFromDelta(delta));
  base::AutoReset<bool> resetter(&changing_bounds_, true);
  target_->SetBounds(new_bounds);
  SetWindowUserSetBounds(target_, new_bounds);
}

void MoveLoop::Cancel() {
  target_->RemoveObserver(this);
  target_ = nullptr;
}

void MoveLoop::Revert() {
  base::AutoReset<bool> resetter(&changing_bounds_, true);
  target_->SetBounds(initial_window_bounds_);
  SetWindowUserSetBounds(target_, initial_user_set_bounds_);
}

gfx::Rect MoveLoop::DetermineBoundsFromDelta(const gfx::Vector2d& delta) {
  if (type_ == Type::MOVE) {
    return gfx::Rect(initial_window_bounds_.origin() + delta,
                     initial_window_bounds_.size());
  }

  // TODO(sky): support better min sizes, make sure doesn't get bigger than
  // screen and max. Also make sure keep some portion on screen.
  gfx::Rect bounds(initial_window_bounds_);
  if (h_loc_ == HorizontalLocation::LEFT) {
    const int x = std::min(bounds.right() - 1, bounds.x() + delta.x());
    const int width = bounds.right() - x;
    bounds.set_x(x);
    bounds.set_width(width);
  } else if (h_loc_ == HorizontalLocation::RIGHT) {
    bounds.set_width(std::max(1, bounds.width() + delta.x()));
  }

  if (v_loc_ == VerticalLocation::TOP) {
    const int y = std::min(bounds.bottom() - 1, bounds.y() + delta.y());
    const int height = bounds.bottom() - y;
    bounds.set_y(y);
    bounds.set_height(height);
  } else if (v_loc_ == VerticalLocation::BOTTOM) {
    bounds.set_height(std::max(1, bounds.height() + delta.y()));
  }

  return bounds;
}

void MoveLoop::OnTreeChanged(const TreeChangeParams& params) {
  if (params.target == target_)
    Cancel();
}

void MoveLoop::OnWindowBoundsChanged(mus::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) {
  DCHECK_EQ(window, target_);
  if (!changing_bounds_)
    Cancel();
}

void MoveLoop::OnWindowVisibilityChanged(mus::Window* window) {
  DCHECK_EQ(window, target_);
  Cancel();
}
