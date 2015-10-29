// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

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

bool IsMouseEventFlag(int32_t event_flags) {
  return !!(event_flags & (mojom::EVENT_FLAGS_LEFT_MOUSE_BUTTON |
                           mojom::EVENT_FLAGS_MIDDLE_MOUSE_BUTTON |
                           mojom::EVENT_FLAGS_RIGHT_MOUSE_BUTTON));
}

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
  return target->parent() &&
         !target->client_area().Contains(location);
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
        keyboard_code_(mojom::KEYBOARD_CODE_UNKNOWN),
        pointer_kind_(mojom::POINTER_KIND_MOUSE) {
    if (matcher.type_matcher) {
      fields_to_match_ |= TYPE;
      event_type_ = matcher.type_matcher->type;
    }
    if (matcher.flags_matcher) {
      fields_to_match_ |= FLAGS;
      event_flags_ = matcher.flags_matcher->flags;
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
    if ((fields_to_match_ & FLAGS) && event.flags != event_flags_)
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

#if !defined(NDEBUG)
  bool Equals(const EventMatcher& matcher) const {
    return fields_to_match_ == matcher.fields_to_match_ &&
           event_type_ == matcher.event_type_ &&
           event_flags_ == matcher.event_flags_ &&
           keyboard_code_ == matcher.keyboard_code_ &&
           pointer_kind_ == matcher.pointer_kind_ &&
           pointer_region_ == matcher.pointer_region_;
  }
#endif

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
  mojom::KeyboardCode keyboard_code_;
  mojom::PointerKind pointer_kind_;
  gfx::RectF pointer_region_;
};

////////////////////////////////////////////////////////////////////////////////

EventDispatcher::EventDispatcher(EventDispatcherDelegate* delegate)
    : delegate_(delegate),
      root_(nullptr),
      capture_window_(nullptr),
      capture_in_nonclient_area_(false) {}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::AddAccelerator(uint32_t id,
                                     mojom::EventMatcherPtr event_matcher) {
  EventMatcher matcher(*event_matcher);
#if !defined(NDEBUG)
  for (const auto& pair : accelerators_) {
    DCHECK_NE(pair.first, id);
    DCHECK(!matcher.Equals(pair.second));
  }
#endif
  accelerators_.insert(Entry(id, matcher));
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
      delegate_->OnAccelerator(accelerator, event.Pass());
      return;
    }
  }

  ServerWindow* target = FindEventTarget(event.get());
  bool in_nonclient_area = false;

  if (IsMouseEventFlag(event->flags)) {
    if (!capture_window_ && target &&
        (event->action == mojom::EVENT_TYPE_POINTER_DOWN)) {
      // TODO(sky): |capture_window_| needs to be reset when window removed
      // from hierarchy.
      capture_window_ = target;
      // TODO(sky): this needs to happen for pointer down events too.
      capture_in_nonclient_area_ =
          IsLocationInNonclientArea(target, EventLocationToPoint(*event));
      in_nonclient_area = capture_in_nonclient_area_;
    } else if (event->action == mojom::EVENT_TYPE_POINTER_UP &&
               IsOnlyOneMouseButtonDown(event->flags)) {
      capture_window_ = nullptr;
    }
    in_nonclient_area = capture_in_nonclient_area_;
  }

  if (target) {
    if (event->action == mojom::EVENT_TYPE_POINTER_DOWN)
      delegate_->SetFocusedWindowFromEventDispatcher(target);

    delegate_->DispatchInputEventToWindow(target, in_nonclient_area,
                                          event.Pass());
  }
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

ServerWindow* EventDispatcher::FindEventTarget(mojom::Event* event) {
  ServerWindow* focused_window =
      delegate_->GetFocusedWindowForEventDispatcher();
  if (event->key_data)
    return focused_window;

  DCHECK(event->pointer_data) << "Unknown event type: " << event->action;
  mojom::LocationData* event_location = event->pointer_data->location.get();
  DCHECK(event_location);
  gfx::Point location(static_cast<int>(event_location->x),
                      static_cast<int>(event_location->y));

  ServerWindow* target = capture_window_;

  if (!target) {
    target = FindDeepestVisibleWindow(root_, surface_id_, &location);
  } else {
    gfx::Transform transform(GetTransformToWindow(surface_id_, target));
    transform.TransformPoint(&location);
  }

  event_location->x = location.x();
  event_location->y = location.y();

  return target;
}

}  // namespace ws
}  // namespace mus
