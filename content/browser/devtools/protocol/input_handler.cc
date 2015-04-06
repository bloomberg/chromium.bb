// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/input_handler.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"

namespace content {
namespace devtools {
namespace input {

typedef DevToolsProtocolClient::Response Response;

namespace {

void SetEventModifiers(blink::WebInputEvent* event, const int* modifiers) {
  if (!modifiers)
    return;
  if (*modifiers & 1)
    event->modifiers |= blink::WebInputEvent::AltKey;
  if (*modifiers & 2)
    event->modifiers |= blink::WebInputEvent::ControlKey;
  if (*modifiers & 4)
    event->modifiers |= blink::WebInputEvent::MetaKey;
  if (*modifiers & 8)
    event->modifiers |= blink::WebInputEvent::ShiftKey;
}

void SetEventTimestamp(blink::WebInputEvent* event, const double* timestamp) {
  event->timeStampSeconds =
      timestamp ? *timestamp : base::Time::Now().ToDoubleT();
}

bool SetKeyboardEventText(blink::WebUChar* to, const std::string* from) {
  if (!from)
    return true;

  base::string16 text16 = base::UTF8ToUTF16(*from);
  if (text16.size() > blink::WebKeyboardEvent::textLengthCap)
    return false;

  for (size_t i = 0; i < text16.size(); ++i)
    to[i] = text16[i];
  return true;
}

bool SetMouseEventButton(blink::WebMouseEvent* event,
                         const std::string* button) {
  if (!button)
    return true;

  if (*button == dispatch_mouse_event::kButtonNone) {
    event->button = blink::WebMouseEvent::ButtonNone;
  } else if (*button == dispatch_mouse_event::kButtonLeft) {
    event->button = blink::WebMouseEvent::ButtonLeft;
    event->modifiers |= blink::WebInputEvent::LeftButtonDown;
  } else if (*button == dispatch_mouse_event::kButtonMiddle) {
    event->button = blink::WebMouseEvent::ButtonMiddle;
    event->modifiers |= blink::WebInputEvent::MiddleButtonDown;
  } else if (*button == dispatch_mouse_event::kButtonRight) {
    event->button = blink::WebMouseEvent::ButtonRight;
    event->modifiers |= blink::WebInputEvent::RightButtonDown;
  } else {
    return false;
  }
  return true;
}

bool SetMouseEventType(blink::WebMouseEvent* event, const std::string& type) {
  if (type == dispatch_mouse_event::kTypeMousePressed) {
    event->type = blink::WebInputEvent::MouseDown;
  } else if (type == dispatch_mouse_event::kTypeMouseReleased) {
    event->type = blink::WebInputEvent::MouseUp;
  } else if (type == dispatch_mouse_event::kTypeMouseMoved) {
    event->type = blink::WebInputEvent::MouseMove;
  } else {
    return false;
  }
  return true;
}

}  // namespace

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

Response InputHandler::DispatchKeyEvent(
    const std::string& type,
    const int* modifiers,
    const double* timestamp,
    const std::string* text,
    const std::string* unmodified_text,
    const std::string* key_identifier,
    const std::string* code,
    const int* windows_virtual_key_code,
    const int* native_virtual_key_code,
    const bool* auto_repeat,
    const bool* is_keypad,
    const bool* is_system_key) {
  NativeWebKeyboardEvent event;

  if (type == dispatch_key_event::kTypeKeyDown) {
    event.type = blink::WebInputEvent::KeyDown;
  } else if (type == dispatch_key_event::kTypeKeyUp) {
    event.type = blink::WebInputEvent::KeyUp;
  } else if (type == dispatch_key_event::kTypeChar) {
    event.type = blink::WebInputEvent::Char;
  } else if (type == dispatch_key_event::kTypeRawKeyDown) {
    event.type = blink::WebInputEvent::RawKeyDown;
  } else {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }

  SetEventModifiers(&event, modifiers);
  SetEventTimestamp(&event, timestamp);
  if (!SetKeyboardEventText(event.text, text))
    return Response::InvalidParams("Invalid 'text' parameter");
  if (!SetKeyboardEventText(event.unmodifiedText, unmodified_text))
    return Response::InvalidParams("Invalid 'unmodifiedText' parameter");

  if (key_identifier) {
    if (key_identifier->size() >
        blink::WebKeyboardEvent::keyIdentifierLengthCap) {
      return Response::InvalidParams("Invalid 'keyIdentifier' parameter");
    }
    for (size_t i = 0; i < key_identifier->size(); ++i)
      event.keyIdentifier[i] = (*key_identifier)[i];
  }

  if (code) {
    event.domCode = static_cast<int>(
        ui::KeycodeConverter::CodeStringToDomCode(code->c_str()));
  }

  if (windows_virtual_key_code)
    event.windowsKeyCode = *windows_virtual_key_code;
  if (native_virtual_key_code)
    event.nativeKeyCode = *native_virtual_key_code;
  if (auto_repeat && *auto_repeat)
    event.modifiers |= blink::WebInputEvent::IsAutoRepeat;
  if (is_keypad && *is_keypad)
    event.modifiers |= blink::WebInputEvent::IsKeyPad;
  if (is_system_key)
    event.isSystemKey = *is_system_key;

  if (!host_)
    return Response::ServerError("Could not connect to view");

  if (!host_->is_focused())
    host_->Focus();
  host_->ForwardKeyboardEvent(event);
  return Response::OK();
}

Response InputHandler::DispatchMouseEvent(
    const std::string& type,
    int x,
    int y,
    const int* modifiers,
    const double* timestamp,
    const std::string* button,
    const int* click_count) {
  blink::WebMouseEvent event;

  if (!SetMouseEventType(&event, type)) {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }
  SetEventModifiers(&event, modifiers);
  SetEventTimestamp(&event, timestamp);
  if (!SetMouseEventButton(&event, button))
    return Response::InvalidParams("Invalid mouse button");

  event.x = x;
  event.y = y;
  event.windowX = x;
  event.windowY = y;
  event.globalX = x;
  event.globalY = y;
  event.clickCount = click_count ? *click_count : 0;

  if (!host_)
    return Response::ServerError("Could not connect to view");

  if (!host_->is_focused())
    host_->Focus();
  host_->ForwardMouseEvent(event);
  return Response::OK();
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

  if (type == emulate_touch_from_mouse_event::kTypeMouseWheel) {
    if (!delta_x || !delta_y) {
      return Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event");
    }
    wheel_event.deltaX = static_cast<float>(*delta_x);
    wheel_event.deltaY = static_cast<float>(*delta_y);
    event = &wheel_event;
    event->type = blink::WebInputEvent::MouseWheel;
  } else if (!SetMouseEventType(event, type)) {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }

  SetEventModifiers(event, modifiers);
  SetEventTimestamp(event, &timestamp);
  if (!SetMouseEventButton(event, &button))
    return Response::InvalidParams("Invalid mouse button");

  event->x = x;
  event->y = y;
  event->windowX = x;
  event->windowY = y;
  event->globalX = x;
  event->globalY = y;
  event->clickCount = click_count ? *click_count : 0;

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
