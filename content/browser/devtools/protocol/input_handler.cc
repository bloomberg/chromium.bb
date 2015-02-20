// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/input_handler.h"

#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {
namespace devtools {
namespace input {

typedef DevToolsProtocolClient::Response Response;

InputHandler::InputHandler()
  : host_(NULL) {
}

InputHandler::~InputHandler() {
}

void InputHandler::SetRenderViewHost(RenderViewHostImpl* host) {
  host_ = host;
}

void InputHandler::SetClient(scoped_ptr<DevToolsProtocolClient> client) {
}

Response InputHandler::EmulateTouchFromMouseEvent(const std::string& type,
                                                  int x,
                                                  int y,
                                                  double timestamp,
                                                  const std::string& button,
                                                  double* delta_x,
                                                  double* delta_y,
                                                  int* modifiers,
                                                  int* click_count) {
  blink::WebMouseWheelEvent wheel_event;
  blink::WebMouseEvent mouse_event;
  blink::WebMouseEvent* event = &mouse_event;

  if (type == emulate_touch_from_mouse_event::kTypeMousePressed) {
    event->type = blink::WebInputEvent::MouseDown;
  } else if (type == emulate_touch_from_mouse_event::kTypeMouseReleased) {
    event->type = blink::WebInputEvent::MouseUp;
  } else if (type == emulate_touch_from_mouse_event::kTypeMouseMoved) {
    event->type = blink::WebInputEvent::MouseMove;
  } else if (type == emulate_touch_from_mouse_event::kTypeMouseWheel) {
    if (!delta_x || !delta_y) {
      return Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event");
    }
    wheel_event.deltaX = static_cast<float>(*delta_x);
    wheel_event.deltaY = static_cast<float>(*delta_y);
    event = &wheel_event;
    event->type = blink::WebInputEvent::MouseWheel;
  } else {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }

  if (modifiers) {
    if (*modifiers & 1)
      event->modifiers |= blink::WebInputEvent::AltKey;
    if (*modifiers & 2)
      event->modifiers |= blink::WebInputEvent::ControlKey;
    if (*modifiers & 4)
      event->modifiers |= blink::WebInputEvent::MetaKey;
    if (*modifiers & 8)
      event->modifiers |= blink::WebInputEvent::ShiftKey;
  }

  event->timeStampSeconds = timestamp;
  event->x = x;
  event->y = y;
  event->windowX = x;
  event->windowY = y;
  event->globalX = x;
  event->globalY = y;

  if (click_count)
    event->clickCount = *click_count;

  if (button == emulate_touch_from_mouse_event::kButtonNone) {
    event->button = blink::WebMouseEvent::ButtonNone;
  } else if (button == emulate_touch_from_mouse_event::kButtonLeft) {
    event->button = blink::WebMouseEvent::ButtonLeft;
    event->modifiers |= blink::WebInputEvent::LeftButtonDown;
  } else if (button == emulate_touch_from_mouse_event::kButtonMiddle) {
    event->button = blink::WebMouseEvent::ButtonMiddle;
    event->modifiers |= blink::WebInputEvent::MiddleButtonDown;
  } else if (button == emulate_touch_from_mouse_event::kButtonRight) {
    event->button = blink::WebMouseEvent::ButtonRight;
    event->modifiers |= blink::WebInputEvent::RightButtonDown;
  } else {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected mouse button '%s'", button.c_str()));
  }

  if (!host_)
    return Response::ServerError("Could not connect to view");

  if (event->type == blink::WebInputEvent::MouseWheel)
    host_->ForwardWheelEvent(wheel_event);
  else
    host_->ForwardMouseEvent(mouse_event);
  return Response::OK();
}

Response InputHandler::SynthesizePinchGesture(
    DevToolsCommandId command_id,
    int x,
    int y,
    double scale_factor,
    const int* relative_speed,
    const std::string* gesture_source_type) {
  return Response::InternalError("Not yet implemented");
}

Response InputHandler::SynthesizeScrollGesture(
    DevToolsCommandId command_id,
    int x,
    int y,
    const int* x_distance,
    const int* y_distance,
    const int* x_overscroll,
    const int* y_overscroll,
    const bool* prevent_fling,
    const int* speed,
    const std::string* gesture_source_type) {
  return Response::InternalError("Not yet implemented");
}

Response InputHandler::SynthesizeTapGesture(
    DevToolsCommandId command_id,
    int x,
    int y,
    const int* duration,
    const int* tap_count,
    const std::string* gesture_source_type) {
  return Response::InternalError("Not yet implemented");
}

}  // namespace input
}  // namespace devtools
}  // namespace content
