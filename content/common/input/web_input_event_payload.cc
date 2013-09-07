// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_input_event_payload.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {
namespace {
static WebInputEventPayload::ScopedWebInputEvent CloneWebInputEvent(
    const WebKit::WebInputEvent& event) {
  WebInputEventPayload::ScopedWebInputEvent clone;
  if (WebKit::WebInputEvent::isMouseEventType(event.type))
    clone.reset(new WebKit::WebMouseEvent());
  else if (event.type == WebKit::WebInputEvent::MouseWheel)
    clone.reset(new WebKit::WebMouseWheelEvent());
  else if (WebKit::WebInputEvent::isKeyboardEventType(event.type))
    clone.reset(new WebKit::WebKeyboardEvent());
  else if (WebKit::WebInputEvent::isTouchEventType(event.type))
    clone.reset(new WebKit::WebTouchEvent());
  else if (WebKit::WebInputEvent::isGestureEventType(event.type))
    clone.reset(new WebKit::WebGestureEvent());
  else
    NOTREACHED() << "Unknown webkit event type " << event.type;

  if (!clone)
    return clone.Pass();

  DCHECK_EQ(event.size, clone->size);
  memcpy(clone.get(), &event, event.size);
  return clone.Pass();
}
}  // namespace

WebInputEventPayload::WebInputEventDeleter::WebInputEventDeleter() {}

void WebInputEventPayload::WebInputEventDeleter::operator()(
    WebKit::WebInputEvent* event) const {
  if (!event)
    return;

  if (WebKit::WebInputEvent::isMouseEventType(event->type))
    delete static_cast<WebKit::WebMouseEvent*>(event);
  else if (event->type == WebKit::WebInputEvent::MouseWheel)
    delete static_cast<WebKit::WebMouseWheelEvent*>(event);
  else if (WebKit::WebInputEvent::isKeyboardEventType(event->type))
    delete static_cast<WebKit::WebKeyboardEvent*>(event);
  else if (WebKit::WebInputEvent::isTouchEventType(event->type))
    delete static_cast<WebKit::WebTouchEvent*>(event);
  else if (WebKit::WebInputEvent::isGestureEventType(event->type))
    delete static_cast<WebKit::WebGestureEvent*>(event);
  else
    NOTREACHED() << "Unknown webkit event type " << event->type;
}

WebInputEventPayload::WebInputEventPayload() : is_keyboard_shortcut_(false) {}

WebInputEventPayload::~WebInputEventPayload() {}

scoped_ptr<WebInputEventPayload> WebInputEventPayload::Create() {
  return make_scoped_ptr(new WebInputEventPayload());
}

scoped_ptr<WebInputEventPayload> WebInputEventPayload::Create(
    const WebKit::WebInputEvent& event,
    const ui::LatencyInfo& latency_info,
    bool is_keyboard_shortcut) {
  scoped_ptr<WebInputEventPayload> payload = Create();
  payload->Initialize(event, latency_info, is_keyboard_shortcut);
  return payload.Pass();
}

const WebInputEventPayload* WebInputEventPayload::Cast(const Payload* payload) {
  DCHECK(payload);
  DCHECK_EQ(WEB_INPUT_EVENT, payload->GetType());
  return static_cast<const WebInputEventPayload*>(payload);
}

InputEvent::Payload::Type WebInputEventPayload::GetType() const {
  return WEB_INPUT_EVENT;
}

void WebInputEventPayload::Initialize(const WebKit::WebInputEvent& web_event,
                                      const ui::LatencyInfo& latency_info,
                                      bool is_keyboard_shortcut) {
  DCHECK(!web_event_) << "WebInputEventPayload already initialized";
  web_event_ = CloneWebInputEvent(web_event);
  latency_info_ = latency_info;
  is_keyboard_shortcut_ = is_keyboard_shortcut;
}

bool WebInputEventPayload::CanCreateFollowupEvents() const {
  return WebKit::WebInputEvent::isTouchEventType(web_event()->type);
}

}  // namespace content
