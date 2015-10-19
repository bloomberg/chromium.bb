// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

#include "cc/surfaces/surface_id.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace mus {

namespace ws {

EventDispatcher::EventDispatcher(WindowTreeHostImpl* window_tree_host)
    : window_tree_host_(window_tree_host) {}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::AddAccelerator(uint32_t id,
                                     mojo::KeyboardCode keyboard_code,
                                     mojo::EventFlags flags) {
#if !defined(NDEBUG)
  for (const auto& pair : accelerators_) {
    DCHECK(pair.first != id);
    DCHECK(pair.second.keyboard_code != keyboard_code ||
           pair.second.flags != flags);
  }
#endif
  accelerators_.insert(Entry(id, Accelerator(keyboard_code, flags)));
}

void EventDispatcher::RemoveAccelerator(uint32_t id) {
  auto it = accelerators_.find(id);
  DCHECK(it != accelerators_.end());
  accelerators_.erase(it);
}

void EventDispatcher::OnEvent(mojo::EventPtr event) {
  if (event->action == mojo::EVENT_TYPE_KEY_PRESSED &&
      !event->key_data->is_char) {
    uint32_t accelerator = 0u;
    if (FindAccelerator(*event, &accelerator)) {
      window_tree_host_->OnAccelerator(accelerator, event.Pass());
      return;
    }
  }

  ServerWindow* target = FindEventTarget(event.get());
  if (target) {
    // Update focus on pointer-down.
    if (event->action == mojo::EVENT_TYPE_POINTER_DOWN)
      window_tree_host_->SetFocusedWindow(target);
    window_tree_host_->DispatchInputEventToWindow(target, event.Pass());
  }
}

bool EventDispatcher::FindAccelerator(const mojo::Event& event,
                                      uint32_t* accelerator_id) {
  DCHECK(event.key_data);
  for (const auto& pair : accelerators_) {
    if (pair.second.keyboard_code == event.key_data->windows_key_code &&
        pair.second.flags == event.flags) {
      *accelerator_id = pair.first;
      return true;
    }
  }
  return false;
}

ServerWindow* EventDispatcher::FindEventTarget(mojo::Event* event) {
  ServerWindow* focused_window = window_tree_host_->GetFocusedWindow();
  if (event->key_data)
    return focused_window;

  DCHECK(event->pointer_data || event->wheel_data) << "Unknown event type: "
                                                   << event->action;

  mojo::LocationData* event_location = nullptr;
  if (event->pointer_data)
    event_location = event->pointer_data->location.get();
  else if (event->wheel_data)
    event_location = event->wheel_data->location.get();
  DCHECK(event_location);
  gfx::Point location(static_cast<int>(event_location->x),
                      static_cast<int>(event_location->y));
  ServerWindow* target = focused_window;
  ServerWindow* root = window_tree_host_->root_window();
  if (event->action == mojo::EVENT_TYPE_POINTER_DOWN || !target ||
      !root->Contains(target)) {
    target = FindDeepestVisibleWindowFromSurface(&location);
    // Surface-based hit-testing will not return a valid target if no
    // compositor-frame have been submitted (e.g. in unit-tests).
    if (!target)
      target = FindDeepestVisibleWindow(root, &location);
    CHECK(target);
  } else {
    gfx::Point old_point = location;
    location = ConvertPointBetweenWindows(root, target, location);
  }

  event_location->x = location.x();
  event_location->y = location.y();
  return target;
}

ServerWindow* EventDispatcher::FindDeepestVisibleWindow(ServerWindow* window,
                                                        gfx::Point* location) {
  for (ServerWindow* child : window->GetChildren()) {
    if (!child->visible())
      continue;

    // TODO(sky): support transform.
    gfx::Point child_location(location->x() - child->bounds().x(),
                              location->y() - child->bounds().y());
    if (child_location.x() >= 0 && child_location.y() >= 0 &&
        child_location.x() < child->bounds().width() &&
        child_location.y() < child->bounds().height()) {
      *location = child_location;
      return FindDeepestVisibleWindow(child, location);
    }
  }
  return window;
}

ServerWindow* EventDispatcher::FindDeepestVisibleWindowFromSurface(
    gfx::Point* location) {
  if (window_tree_host_->surface_id().is_null())
    return nullptr;

  gfx::Transform transform_to_target_surface;
  cc::SurfaceId target_surface =
      window_tree_host_->root_window()
          ->delegate()
          ->GetSurfacesState()
          ->hit_tester()
          ->GetTargetSurfaceAtPoint(window_tree_host_->surface_id(), *location,
                                    &transform_to_target_surface);
  WindowId id = WindowIdFromTransportId(
      cc::SurfaceIdAllocator::NamespaceForId(target_surface));
  ServerWindow* target = window_tree_host_->connection_manager()->GetWindow(id);
  // TODO(fsamuel): This should be a DCHECK but currently we use stale
  // information to decide where to route input events. This should be fixed
  // once we implement a UI scheduler.
  if (target) {
    transform_to_target_surface.TransformPoint(location);
    return target;
  }
  return nullptr;
}

}  // namespace ws

}  // namespace mus
