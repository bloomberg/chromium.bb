// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

#include <set>

#include "cc/surfaces/surface_hittest.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "components/mus/ws/window_finder.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace mus {
namespace ws {
namespace {

bool IsOnlyOneMouseButtonDown(mojom::EventFlags flags) {
  const uint32_t mouse_only_flags =
      flags & (mojom::EVENT_FLAGS_LEFT_MOUSE_BUTTON |
               mojom::EVENT_FLAGS_MIDDLE_MOUSE_BUTTON |
               mojom::EVENT_FLAGS_RIGHT_MOUSE_BUTTON);
  return mouse_only_flags == mojom::EVENT_FLAGS_LEFT_MOUSE_BUTTON ||
         mouse_only_flags == mojom::EVENT_FLAGS_MIDDLE_MOUSE_BUTTON ||
         mouse_only_flags == mojom::EVENT_FLAGS_RIGHT_MOUSE_BUTTON;
}

bool IsLocationInNonclientArea(const ServerWindow* target,
                               const gfx::Point& location) {
  if (!target->parent())
    return false;

  gfx::Rect client_area(target->bounds().size());
  client_area.Inset(target->client_area());
  if (client_area.Contains(location))
    return false;

  for (const auto& rect : target->additional_client_areas()) {
    if (rect.Contains(location))
      return false;
  }

  return true;
}

gfx::Point EventLocationToPoint(const mojom::Event& event) {
  return gfx::ToFlooredPoint(gfx::PointF(event.pointer_data->location->x,
                                         event.pointer_data->location->y));
}

}  // namespace

class EventMatcher {
 public:
  explicit EventMatcher(const mojom::EventMatcher& matcher)
      : fields_to_match_(NONE),
        event_type_(mojom::EVENT_TYPE_UNKNOWN),
        event_flags_(mojom::EVENT_FLAGS_NONE),
        ignore_event_flags_(mojom::EVENT_FLAGS_NONE),
        keyboard_code_(mojom::KEYBOARD_CODE_UNKNOWN),
        pointer_kind_(mojom::POINTER_KIND_MOUSE) {
    if (matcher.type_matcher) {
      fields_to_match_ |= TYPE;
      event_type_ = matcher.type_matcher->type;
    }
    if (matcher.flags_matcher) {
      fields_to_match_ |= FLAGS;
      event_flags_ = matcher.flags_matcher->flags;
      if (matcher.ignore_flags_matcher)
        ignore_event_flags_ = matcher.ignore_flags_matcher->flags;
    }
    if (matcher.key_matcher) {
      fields_to_match_ |= KEYBOARD_CODE;
      keyboard_code_ = matcher.key_matcher->keyboard_code;
    }
    if (matcher.pointer_kind_matcher) {
      fields_to_match_ |= POINTER_KIND;
      pointer_kind_ = matcher.pointer_kind_matcher->pointer_kind;
    }
    if (matcher.pointer_location_matcher) {
      fields_to_match_ |= POINTER_LOCATION;
      pointer_region_ =
          matcher.pointer_location_matcher->region.To<gfx::RectF>();
    }
  }

  ~EventMatcher() {}

  bool MatchesEvent(const mojom::Event& event) const {
    if ((fields_to_match_ & TYPE) && event.action != event_type_)
      return false;
    mojom::EventFlags flags =
        static_cast<mojom::EventFlags>(event.flags & ~ignore_event_flags_);
    if ((fields_to_match_ & FLAGS) && flags != event_flags_)
      return false;
    if (fields_to_match_ & KEYBOARD_CODE) {
      if (!event.key_data)
        return false;
      if (keyboard_code_ != event.key_data->key_code)
        return false;
    }
    if (fields_to_match_ & POINTER_KIND) {
      if (!event.pointer_data)
        return false;
      if (pointer_kind_ != event.pointer_data->kind)
        return false;
    }
    if (fields_to_match_ & POINTER_LOCATION) {
      // TODO(sad): The tricky part here is to make sure the same coord-space is
      // used for the location-region and the event-location.
      NOTIMPLEMENTED();
      return false;
    }

    return true;
  }

  bool Equals(const EventMatcher& matcher) const {
    return fields_to_match_ == matcher.fields_to_match_ &&
           event_type_ == matcher.event_type_ &&
           event_flags_ == matcher.event_flags_ &&
           ignore_event_flags_ == matcher.ignore_event_flags_ &&
           keyboard_code_ == matcher.keyboard_code_ &&
           pointer_kind_ == matcher.pointer_kind_ &&
           pointer_region_ == matcher.pointer_region_;
  }

 private:
  enum MatchFields {
    NONE = 0,
    TYPE = 1 << 0,
    FLAGS = 1 << 1,
    KEYBOARD_CODE = 1 << 2,
    POINTER_KIND = 1 << 3,
    POINTER_LOCATION = 1 << 4,
  };

  uint32_t fields_to_match_;
  mojom::EventType event_type_;
  mojom::EventFlags event_flags_;
  mojom::EventFlags ignore_event_flags_;
  mojom::KeyboardCode keyboard_code_;
  mojom::PointerKind pointer_kind_;
  gfx::RectF pointer_region_;
};

////////////////////////////////////////////////////////////////////////////////

EventDispatcher::EventDispatcher(EventDispatcherDelegate* delegate)
    : delegate_(delegate),
      root_(nullptr),
      mouse_button_down_(false),
      mouse_cursor_source_window_(nullptr) {}

EventDispatcher::~EventDispatcher() {
  std::set<ServerWindow*> pointer_targets;
  for (const auto& pair : pointer_targets_) {
    if (pair.second.window &&
        pointer_targets.insert(pair.second.window).second) {
      pair.second.window->RemoveObserver(this);
    }
  }
}

void EventDispatcher::UpdateCursorProviderByLastKnownLocation() {
  if (!mouse_button_down_) {
    gfx::Point location = mouse_pointer_last_location_;
    mouse_cursor_source_window_ =
        FindDeepestVisibleWindowForEvents(root_, surface_id_, &location);
  }
}

bool EventDispatcher::AddAccelerator(uint32_t id,
                                     mojom::EventMatcherPtr event_matcher) {
  EventMatcher matcher(*event_matcher);
  // If an accelerator with the same id or matcher already exists, then abort.
  for (const auto& pair : accelerators_) {
    if (pair.first == id || matcher.Equals(pair.second))
      return false;
  }
  accelerators_.insert(Entry(id, matcher));
  return true;
}

void EventDispatcher::RemoveAccelerator(uint32_t id) {
  auto it = accelerators_.find(id);
  DCHECK(it != accelerators_.end());
  accelerators_.erase(it);
}

void EventDispatcher::OnEvent(mojom::EventPtr event) {
  if (!root_)
    return;

  if (event->action == mojom::EVENT_TYPE_KEY_PRESSED &&
      !event->key_data->is_char) {
    uint32_t accelerator = 0u;
    if (FindAccelerator(*event, &accelerator)) {
      delegate_->OnAccelerator(accelerator, std::move(event));
      return;
    }
  }

  if (event->key_data) {
    ProcessKeyEvent(std::move(event));
    return;
  }

  if (event->pointer_data.get()) {
    ProcessPointerEvent(std::move(event));
    return;
  }

  NOTREACHED();
}

void EventDispatcher::ProcessKeyEvent(mojom::EventPtr event) {
  ServerWindow* focused_window =
      delegate_->GetFocusedWindowForEventDispatcher();
  if (focused_window)
    delegate_->DispatchInputEventToWindow(focused_window, false,
                                          std::move(event));
}

void EventDispatcher::ProcessPointerEvent(mojom::EventPtr event) {
  bool is_mouse_event =
      event->pointer_data &&
      event->pointer_data->kind == mojom::PointerKind::POINTER_KIND_MOUSE;

  if (is_mouse_event)
    mouse_pointer_last_location_ = EventLocationToPoint(*event);

  const int32_t pointer_id = event->pointer_data->pointer_id;
  if (event->action == mojom::EVENT_TYPE_WHEEL ||
      (event->action == mojom::EVENT_TYPE_POINTER_MOVE &&
       pointer_targets_.count(pointer_id) == 0)) {
    PointerTarget pointer_target;
    if (pointer_targets_.count(pointer_id) != 0) {
      pointer_target = pointer_targets_[pointer_id];
    } else {
      gfx::Point location(EventLocationToPoint(*event));
      pointer_target.window =
          FindDeepestVisibleWindowForEvents(root_, surface_id_, &location);
    }
    if (is_mouse_event && !mouse_button_down_)
      mouse_cursor_source_window_ = pointer_target.window;
    DispatchToPointerTarget(pointer_target, std::move(event));
    return;
  }

  // Pointer down implicitly captures.
  if (pointer_targets_.count(pointer_id) == 0) {
    DCHECK(event->action == mojom::EVENT_TYPE_POINTER_DOWN);
    const bool is_first_pointer_down = pointer_targets_.empty();
    gfx::Point location(EventLocationToPoint(*event));
    ServerWindow* target =
        FindDeepestVisibleWindowForEvents(root_, surface_id_, &location);
    DCHECK(target);
    if (!IsObservingWindow(target))
      target->AddObserver(this);

    if (is_mouse_event) {
      mouse_button_down_ = true;
      mouse_cursor_source_window_ = target;
    }

    pointer_targets_[pointer_id].window = target;
    pointer_targets_[pointer_id].in_nonclient_area =
        IsLocationInNonclientArea(target, location);

    if (is_first_pointer_down)
      delegate_->SetFocusedWindowFromEventDispatcher(target);
  }

  // Release capture on pointer up. For mouse we only release if there are
  // no buttons down.
  const bool should_reset_target =
      (event->action == mojom::EVENT_TYPE_POINTER_UP ||
       event->action == mojom::EVENT_TYPE_POINTER_CANCEL) &&
      (event->pointer_data->kind != mojom::POINTER_KIND_MOUSE ||
       IsOnlyOneMouseButtonDown(event->flags));

  if (should_reset_target && is_mouse_event) {
    // When we release the mouse button, we want the cursor to be sourced from
    // the window under the mouse pointer, even though we're sending the button
    // up event to the window that had implicit capture. We have to set this
    // before we perform dispatch because the Delegate is going to read this
    // information from us.
    mouse_button_down_ = false;
    gfx::Point location(EventLocationToPoint(*event));
    mouse_cursor_source_window_ =
        FindDeepestVisibleWindowForEvents(root_, surface_id_, &location);
  }

  DispatchToPointerTarget(pointer_targets_[pointer_id], std::move(event));

  if (should_reset_target) {
    ServerWindow* target = pointer_targets_[pointer_id].window;
    pointer_targets_.erase(pointer_id);
    if (target && !IsObservingWindow(target))
      target->RemoveObserver(this);
  }
}

void EventDispatcher::DispatchToPointerTarget(const PointerTarget& target,
                                              mojom::EventPtr event) {
  if (!target.window)
    return;

  gfx::Point location(EventLocationToPoint(*event));
  gfx::Transform transform(GetTransformToWindow(surface_id_, target.window));
  transform.TransformPoint(&location);
  event->pointer_data->location->x = location.x();
  event->pointer_data->location->y = location.y();
  delegate_->DispatchInputEventToWindow(target.window, target.in_nonclient_area,
                                        std::move(event));
}

void EventDispatcher::CancelPointerEventsToTarget(ServerWindow* window) {
  window->RemoveObserver(this);

  for (auto& pair : pointer_targets_) {
    if (pair.second.window == window)
      pair.second.window = nullptr;
  }
}

bool EventDispatcher::IsObservingWindow(ServerWindow* window) {
  for (const auto& pair : pointer_targets_) {
    if (pair.second.window == window)
      return true;
  }
  return false;
}

bool EventDispatcher::FindAccelerator(const mojom::Event& event,
                                      uint32_t* accelerator_id) {
  DCHECK(event.key_data);
  for (const auto& pair : accelerators_) {
    if (pair.second.MatchesEvent(event)) {
      *accelerator_id = pair.first;
      return true;
    }
  }
  return false;
}

void EventDispatcher::OnWillChangeWindowHierarchy(ServerWindow* window,
                                                  ServerWindow* new_parent,
                                                  ServerWindow* old_parent) {
  CancelPointerEventsToTarget(window);
}

void EventDispatcher::OnWindowVisibilityChanged(ServerWindow* window) {
  CancelPointerEventsToTarget(window);
}

void EventDispatcher::OnWindowDestroyed(ServerWindow* window) {
  CancelPointerEventsToTarget(window);

  if (mouse_cursor_source_window_ == window)
    mouse_cursor_source_window_ = nullptr;
}

}  // namespace ws
}  // namespace mus
