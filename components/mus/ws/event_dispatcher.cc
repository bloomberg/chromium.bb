// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "components/mus/ws/window_finder.h"
#include "ui/gfx/geometry/point.h"

namespace mus {
namespace ws {

EventDispatcher::EventDispatcher(EventDispatcherDelegate* delegate)
    : delegate_(delegate), root_(nullptr) {}

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
  if (!root_)
    return;

  if (event->action == mojo::EVENT_TYPE_KEY_PRESSED &&
      !event->key_data->is_char) {
    uint32_t accelerator = 0u;
    if (FindAccelerator(*event, &accelerator)) {
      delegate_->OnAccelerator(accelerator, event.Pass());
      return;
    }
  }

  ServerWindow* target = FindEventTarget(event.get());
  if (target) {
    // Update focus on pointer-down.
    if (event->action == mojo::EVENT_TYPE_POINTER_DOWN)
      delegate_->SetFocusedWindowFromEventDispatcher(target);
    delegate_->DispatchInputEventToWindow(target, event.Pass());
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
  ServerWindow* focused_window =
      delegate_->GetFocusedWindowForEventDispatcher();
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
  if (event->action == mojo::EVENT_TYPE_POINTER_DOWN || !target ||
      !root_->Contains(target)) {
    target = FindDeepestVisibleWindowFromSurface(root_, surface_id_, &location);
    // Surface-based hit-testing will not return a valid target if no
    // compositor-frame have been submitted (e.g. in unit-tests).
    if (!target)
      target = FindDeepestVisibleWindow(root_, &location);
    CHECK(target);
  } else {
    gfx::Point old_point = location;
    location = ConvertPointBetweenWindows(root_, target, location);
  }

  event_location->x = location.x();
  event_location->y = location.y();
  return target;
}

}  // namespace ws
}  // namespace mus
