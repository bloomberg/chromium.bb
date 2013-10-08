// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_input_event_traits.h"

#include "base/logging.h"

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;

namespace content {
namespace {

struct WebInputEventSize {
  template <class EventType>
  void Execute(WebInputEvent::Type /* type */, size_t* type_size) const {
    *type_size = sizeof(EventType);
  }
};

struct WebInputEventClone {
  template <class EventType>
  void Execute(const WebInputEvent& event,
               ScopedWebInputEvent* scoped_event) const {
    DCHECK_EQ(sizeof(EventType), event.size);
    *scoped_event = ScopedWebInputEvent(
        new EventType(static_cast<const EventType&>(event)));
  }
};

struct WebInputEventDelete {
  template <class EventType>
  void Execute(WebInputEvent* event, bool* /* dummy_var */) const {
    if (!event)
      return;
    DCHECK_EQ(sizeof(EventType), event->size);
    delete static_cast<EventType*>(event);
  }
};

template <typename Operator, typename ArgIn, typename ArgOut>
void Apply(Operator op,
           WebInputEvent::Type type,
           const ArgIn& arg_in,
           ArgOut* arg_out) {
  if (WebInputEvent::isMouseEventType(type))
    op.template Execute<WebMouseEvent>(arg_in, arg_out);
  else if (type == WebInputEvent::MouseWheel)
    op.template Execute<WebMouseWheelEvent>(arg_in, arg_out);
  else if (WebInputEvent::isKeyboardEventType(type))
    op.template Execute<WebKeyboardEvent>(arg_in, arg_out);
  else if (WebInputEvent::isTouchEventType(type))
    op.template Execute<WebTouchEvent>(arg_in, arg_out);
  else if (WebInputEvent::isGestureEventType(type))
    op.template Execute<WebGestureEvent>(arg_in, arg_out);
  else
    NOTREACHED() << "Unknown webkit event type " << type;
}

}  // namespace

size_t WebInputEventTraits::GetSize(WebInputEvent::Type type) {
  size_t size = 0;
  Apply(WebInputEventSize(), type, type, &size);
  return size;
}

ScopedWebInputEvent WebInputEventTraits::Clone(const WebInputEvent& event) {
  ScopedWebInputEvent scoped_event;
  Apply(WebInputEventClone(), event.type, event, &scoped_event);
  return scoped_event.Pass();
}

void WebInputEventTraits::Delete(WebInputEvent* event) {
  if (!event)
    return;
  bool dummy_var = false;
  Apply(WebInputEventDelete(), event->type, event, &dummy_var);
}

}  // namespace content
