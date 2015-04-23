// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_input_events_type_converters.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/mojo/events/input_event_constants.mojom.h"

namespace mojo {
namespace {

// Used for scrolling. This matches Firefox behavior.
const int kPixelsPerTick = 53;

double EventTimeToWebEventTime(const EventPtr& event) {
  return base::TimeDelta::FromInternalValue(event->time_stamp).InSecondsF();
}

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;

  if (flags & mojo::EVENT_FLAGS_SHIFT_DOWN)
    modifiers |= blink::WebInputEvent::ShiftKey;
  if (flags & mojo::EVENT_FLAGS_CONTROL_DOWN)
    modifiers |= blink::WebInputEvent::ControlKey;
  if (flags & mojo::EVENT_FLAGS_ALT_DOWN)
    modifiers |= blink::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (flags & mojo::EVENT_FLAGS_LEFT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (flags & mojo::EVENT_FLAGS_MIDDLE_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (flags & mojo::EVENT_FLAGS_RIGHT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::RightButtonDown;
  if (flags & mojo::EVENT_FLAGS_CAPS_LOCK_DOWN)
    modifiers |= blink::WebInputEvent::CapsLockOn;
  return modifiers;
}

int EventFlagsToWebInputEventModifiers(int flags) {
  return
      (flags & mojo::EVENT_FLAGS_SHIFT_DOWN ?
       blink::WebInputEvent::ShiftKey : 0) |
      (flags & mojo::EVENT_FLAGS_CONTROL_DOWN ?
       blink::WebInputEvent::ControlKey : 0) |
      (flags & mojo::EVENT_FLAGS_CAPS_LOCK_DOWN ?
       blink::WebInputEvent::CapsLockOn : 0) |
      (flags & mojo::EVENT_FLAGS_ALT_DOWN ?
       blink::WebInputEvent::AltKey : 0);
}

int GetClickCount(int flags) {
  if (flags & mojo::MOUSE_EVENT_FLAGS_IS_TRIPLE_CLICK)
    return 3;
  else if (flags & mojo::MOUSE_EVENT_FLAGS_IS_DOUBLE_CLICK)
    return 2;

  return 1;
}

void SetWebMouseEventLocation(const mojo::PointerData& pointer_data,
                              blink::WebMouseEvent* web_event) {
  web_event->x = static_cast<int>(pointer_data.x);
  web_event->y = static_cast<int>(pointer_data.y);
  web_event->globalX = static_cast<int>(pointer_data.screen_x);
  web_event->globalY = static_cast<int>(pointer_data.screen_y);
}

scoped_ptr<blink::WebInputEvent> BuildWebMouseEventFrom(const EventPtr& event) {
  scoped_ptr<blink::WebMouseEvent> web_event(new blink::WebMouseEvent);

  SetWebMouseEventLocation(*(event->pointer_data), web_event.get());

  web_event->modifiers = EventFlagsToWebEventModifiers(event->flags);
  web_event->timeStampSeconds = EventTimeToWebEventTime(event);

  web_event->button = blink::WebMouseEvent::ButtonNone;
  if (event->flags & mojo::EVENT_FLAGS_LEFT_MOUSE_BUTTON)
    web_event->button = blink::WebMouseEvent::ButtonLeft;
  if (event->flags & mojo::EVENT_FLAGS_MIDDLE_MOUSE_BUTTON)
    web_event->button = blink::WebMouseEvent::ButtonMiddle;
  if (event->flags & mojo::EVENT_FLAGS_RIGHT_MOUSE_BUTTON)
    web_event->button = blink::WebMouseEvent::ButtonRight;

  switch (event->action) {
    case mojo::EVENT_TYPE_POINTER_DOWN:
      web_event->type = blink::WebInputEvent::MouseDown;
      break;
    case mojo::EVENT_TYPE_POINTER_UP:
      web_event->type = blink::WebInputEvent::MouseUp;
      break;
    case mojo::EVENT_TYPE_POINTER_MOVE:
      web_event->type = blink::WebInputEvent::MouseMove;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event->action;
      break;
  }

  web_event->clickCount = GetClickCount(event->flags);

  return web_event.Pass();
}

scoped_ptr<blink::WebInputEvent> BuildWebKeyboardEvent(
    const EventPtr& event) {
  scoped_ptr<blink::WebKeyboardEvent> web_event(new blink::WebKeyboardEvent);

  web_event->modifiers = EventFlagsToWebInputEventModifiers(event->flags);
  web_event->timeStampSeconds = EventTimeToWebEventTime(event);

  switch (event->action) {
    case EVENT_TYPE_KEY_PRESSED:
      web_event->type = event->key_data->is_char ? blink::WebInputEvent::Char :
          blink::WebInputEvent::RawKeyDown;
      break;
    case EVENT_TYPE_KEY_RELEASED:
      web_event->type = blink::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (web_event->modifiers & blink::WebInputEvent::AltKey)
    web_event->isSystemKey = true;

  web_event->windowsKeyCode = event->key_data->windows_key_code;
  web_event->nativeKeyCode = event->key_data->native_key_code;
  web_event->text[0] = event->key_data->text;
  web_event->unmodifiedText[0] = event->key_data->unmodified_text;

  web_event->setKeyIdentifierFromWindowsKeyCode();
  return web_event.Pass();
}

scoped_ptr<blink::WebInputEvent> BuildWebMouseWheelEventFrom(
    const EventPtr& event) {
  scoped_ptr<blink::WebMouseWheelEvent> web_event(
      new blink::WebMouseWheelEvent);
  web_event->type = blink::WebInputEvent::MouseWheel;
  web_event->button = blink::WebMouseEvent::ButtonNone;
  web_event->modifiers = EventFlagsToWebEventModifiers(event->flags);
  web_event->timeStampSeconds = EventTimeToWebEventTime(event);

  SetWebMouseEventLocation(*(event->pointer_data), web_event.get());

  if ((event->flags & mojo::EVENT_FLAGS_SHIFT_DOWN) != 0 &&
      event->pointer_data->horizontal_wheel == 0) {
    web_event->deltaX = event->pointer_data->horizontal_wheel;
    web_event->deltaY = 0;
  } else {
    web_event->deltaX = event->pointer_data->horizontal_wheel;
    web_event->deltaY = event->pointer_data->vertical_wheel;
  }

  // TODO(sky): resole this, doesn't work for desktop.
  web_event->wheelTicksX = web_event->deltaX / kPixelsPerTick;
  web_event->wheelTicksY = web_event->deltaY / kPixelsPerTick;

  return web_event.Pass();
}

}  // namespace

// static
scoped_ptr<blink::WebInputEvent>
TypeConverter<scoped_ptr<blink::WebInputEvent>, EventPtr>::Convert(
    const EventPtr& event) {
  if (event->action == mojo::EVENT_TYPE_POINTER_DOWN ||
      event->action == mojo::EVENT_TYPE_POINTER_UP ||
      event->action == mojo::EVENT_TYPE_POINTER_CANCEL ||
      event->action == mojo::EVENT_TYPE_POINTER_MOVE) {
    if (event->pointer_data->horizontal_wheel != 0 ||
        event->pointer_data->vertical_wheel != 0) {
      return BuildWebMouseWheelEventFrom(event);
    }
    if (event->pointer_data->kind == mojo::POINTER_KIND_MOUSE)
      return BuildWebMouseEventFrom(event);
  } else if ((event->action == mojo::EVENT_TYPE_KEY_PRESSED ||
              event->action == mojo::EVENT_TYPE_KEY_RELEASED) &&
             event->key_data) {
    return BuildWebKeyboardEvent(event);
  }
  return nullptr;
}

}  // namespace mojo
