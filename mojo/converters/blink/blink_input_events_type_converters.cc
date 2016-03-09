// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/blink/blink_input_events_type_converters.h"

#include <utility>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/mus/public/interfaces/input_event_constants.mojom.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event.h"

namespace mojo {
namespace {

double EventTimeToWebEventTime(const mus::mojom::EventPtr& event) {
  return base::TimeDelta::FromInternalValue(event->time_stamp).InSecondsF();
}

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;

  if (flags & mus::mojom::kEventFlagShiftDown)
    modifiers |= blink::WebInputEvent::ShiftKey;
  if (flags & mus::mojom::kEventFlagControlDown)
    modifiers |= blink::WebInputEvent::ControlKey;
  if (flags & mus::mojom::kEventFlagAltDown)
    modifiers |= blink::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (flags & mus::mojom::kEventFlagLeftMouseButton)
    modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (flags & mus::mojom::kEventFlagMiddleMouseButton)
    modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (flags & mus::mojom::kEventFlagRightMouseButton)
    modifiers |= blink::WebInputEvent::RightButtonDown;
  if (flags & mus::mojom::kEventFlagCapsLockOn)
    modifiers |= blink::WebInputEvent::CapsLockOn;
  return modifiers;
}

int EventFlagsToWebInputEventModifiers(int flags) {
  return (flags & mus::mojom::kEventFlagShiftDown
              ? blink::WebInputEvent::ShiftKey
              : 0) |
         (flags & mus::mojom::kEventFlagControlDown
              ? blink::WebInputEvent::ControlKey
              : 0) |
         (flags & mus::mojom::kEventFlagCapsLockOn
              ? blink::WebInputEvent::CapsLockOn
              : 0) |
         (flags & mus::mojom::kEventFlagAltDown ? blink::WebInputEvent::AltKey
                                                : 0);
}

int GetClickCount(int flags) {
  if (flags & mus::mojom::kMouseEventFlagIsTripleClick)
    return 3;
  else if (flags & mus::mojom::kMouseEventFlagIsDoubleClick)
    return 2;

  return 1;
}

void SetWebMouseEventLocation(const mus::mojom::LocationData& location_data,
                              blink::WebMouseEvent* web_event) {
  web_event->x = static_cast<int>(location_data.x);
  web_event->y = static_cast<int>(location_data.y);
  web_event->globalX = static_cast<int>(location_data.screen_x);
  web_event->globalY = static_cast<int>(location_data.screen_y);
}

scoped_ptr<blink::WebInputEvent> BuildWebMouseEventFrom(
    const mus::mojom::EventPtr& event) {
  scoped_ptr<blink::WebMouseEvent> web_event(new blink::WebMouseEvent);
  // TODO(crbug.com/593375): Set pointerType from event->pointer_data->kind

  if (event->pointer_data && event->pointer_data->location)
    SetWebMouseEventLocation(*(event->pointer_data->location), web_event.get());

  web_event->modifiers = EventFlagsToWebEventModifiers(event->flags);
  web_event->timeStampSeconds = EventTimeToWebEventTime(event);

  web_event->button = blink::WebMouseEvent::ButtonNone;
  if (event->flags & mus::mojom::kEventFlagLeftMouseButton)
    web_event->button = blink::WebMouseEvent::ButtonLeft;
  if (event->flags & mus::mojom::kEventFlagMiddleMouseButton)
    web_event->button = blink::WebMouseEvent::ButtonMiddle;
  if (event->flags & mus::mojom::kEventFlagRightMouseButton)
    web_event->button = blink::WebMouseEvent::ButtonRight;

  switch (event->action) {
    case mus::mojom::EventType::POINTER_DOWN:
      web_event->type = blink::WebInputEvent::MouseDown;
      break;
    case mus::mojom::EventType::POINTER_UP:
      web_event->type = blink::WebInputEvent::MouseUp;
      break;
    case mus::mojom::EventType::POINTER_MOVE:
      web_event->type = blink::WebInputEvent::MouseMove;
      break;
    case mus::mojom::EventType::MOUSE_EXIT:
      web_event->type = blink::WebInputEvent::MouseLeave;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event->action;
      break;
  }

  web_event->clickCount = GetClickCount(event->flags);

  return std::move(web_event);
}

scoped_ptr<blink::WebInputEvent> BuildWebKeyboardEvent(
    const mus::mojom::EventPtr& event) {
  scoped_ptr<blink::WebKeyboardEvent> web_event(new blink::WebKeyboardEvent);

  web_event->modifiers = EventFlagsToWebInputEventModifiers(event->flags);
  web_event->timeStampSeconds = EventTimeToWebEventTime(event);

  switch (event->action) {
    case mus::mojom::EventType::KEY_PRESSED:
      web_event->type = event->key_data->is_char
                            ? blink::WebInputEvent::Char
                            : blink::WebInputEvent::RawKeyDown;
      break;
    case mus::mojom::EventType::KEY_RELEASED:
      web_event->type = blink::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (web_event->modifiers & blink::WebInputEvent::AltKey)
    web_event->isSystemKey = true;

  web_event->windowsKeyCode =
      static_cast<int>(event->key_data->windows_key_code);
  web_event->nativeKeyCode = event->key_data->native_key_code;
  web_event->text[0] = event->key_data->text;
  web_event->unmodifiedText[0] = event->key_data->unmodified_text;

  web_event->setKeyIdentifierFromWindowsKeyCode();
  return std::move(web_event);
}

scoped_ptr<blink::WebInputEvent> BuildWebMouseWheelEventFrom(
    const mus::mojom::EventPtr& event) {
  DCHECK(event->pointer_data && event->pointer_data->wheel_data);
  const mus::mojom::WheelData& wheel_data = *event->pointer_data->wheel_data;
  scoped_ptr<blink::WebMouseWheelEvent> web_event(
      new blink::WebMouseWheelEvent);
  web_event->type = blink::WebInputEvent::MouseWheel;
  web_event->button = blink::WebMouseEvent::ButtonNone;
  web_event->modifiers = EventFlagsToWebEventModifiers(event->flags);
  web_event->timeStampSeconds = EventTimeToWebEventTime(event);

  SetWebMouseEventLocation(*(event->pointer_data->location), web_event.get());

  // TODO(rjkroege): Update the following code once Blink supports
  // DOM Level 3 wheel events
  // (http://www.w3.org/TR/DOM-Level-3-Events/#events-wheelevents)
  web_event->deltaX = wheel_data.delta_x;
  web_event->deltaY = wheel_data.delta_y;

  web_event->wheelTicksX = web_event->deltaX / ui::MouseWheelEvent::kWheelDelta;
  web_event->wheelTicksY = web_event->deltaY / ui::MouseWheelEvent::kWheelDelta;

  // TODO(rjkroege): Mandoline currently only generates WHEEL_MODE_LINE
  // wheel events so the other modes are not yet tested. Verify that
  // the implementation is correct.
  switch (wheel_data.mode) {
    case mus::mojom::WheelMode::PIXEL:
      web_event->hasPreciseScrollingDeltas = true;
      web_event->scrollByPage = false;
      web_event->canScroll = true;
      break;
    case mus::mojom::WheelMode::LINE:
      web_event->hasPreciseScrollingDeltas = false;
      web_event->scrollByPage = false;
      web_event->canScroll = true;
      break;
    case mus::mojom::WheelMode::PAGE:
      web_event->hasPreciseScrollingDeltas = false;
      web_event->scrollByPage = true;
      web_event->canScroll = true;
      break;
    case mus::mojom::WheelMode::SCALING:
      web_event->hasPreciseScrollingDeltas = false;
      web_event->scrollByPage = false;
      web_event->canScroll = false;
      break;
  }

  return std::move(web_event);
}

}  // namespace

// static
scoped_ptr<blink::WebInputEvent>
TypeConverter<scoped_ptr<blink::WebInputEvent>, mus::mojom::EventPtr>::Convert(
    const mus::mojom::EventPtr& event) {
  switch (event->action) {
    case mus::mojom::EventType::POINTER_DOWN:
    case mus::mojom::EventType::POINTER_UP:
    case mus::mojom::EventType::POINTER_CANCEL:
    case mus::mojom::EventType::POINTER_MOVE:
    case mus::mojom::EventType::MOUSE_EXIT:
      if (event->pointer_data &&
          event->pointer_data->kind == mus::mojom::PointerKind::MOUSE) {
        return BuildWebMouseEventFrom(event);
      }
      return nullptr;
    case mus::mojom::EventType::WHEEL:
      return BuildWebMouseWheelEventFrom(event);
    case mus::mojom::EventType::KEY_PRESSED:
    case mus::mojom::EventType::KEY_RELEASED:
      return BuildWebKeyboardEvent(event);
    case mus::mojom::EventType::UNKNOWN:
      return nullptr;
  }
  return nullptr;
}

}  // namespace mojo
