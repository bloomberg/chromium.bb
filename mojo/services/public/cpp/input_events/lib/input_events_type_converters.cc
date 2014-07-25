// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"

#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace mojo {

COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_NONE) ==
               static_cast<int32>(ui::EF_NONE),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_CAPS_LOCK_DOWN) ==
               static_cast<int32>(ui::EF_CAPS_LOCK_DOWN),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_SHIFT_DOWN) ==
               static_cast<int32>(ui::EF_SHIFT_DOWN),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_CONTROL_DOWN) ==
               static_cast<int32>(ui::EF_CONTROL_DOWN),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_ALT_DOWN) ==
               static_cast<int32>(ui::EF_ALT_DOWN),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_LEFT_MOUSE_BUTTON) ==
               static_cast<int32>(ui::EF_LEFT_MOUSE_BUTTON),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_MIDDLE_MOUSE_BUTTON) ==
               static_cast<int32>(ui::EF_MIDDLE_MOUSE_BUTTON),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_RIGHT_MOUSE_BUTTON) ==
               static_cast<int32>(ui::EF_RIGHT_MOUSE_BUTTON),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_COMMAND_DOWN) ==
               static_cast<int32>(ui::EF_COMMAND_DOWN),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_EXTENDED) ==
               static_cast<int32>(ui::EF_EXTENDED),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_IS_SYNTHESIZED) ==
               static_cast<int32>(ui::EF_IS_SYNTHESIZED),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_ALTGR_DOWN) ==
               static_cast<int32>(ui::EF_ALTGR_DOWN),
               event_flags_should_match);
COMPILE_ASSERT(static_cast<int32>(EVENT_FLAGS_MOD3_DOWN) ==
               static_cast<int32>(ui::EF_MOD3_DOWN),
               event_flags_should_match);


// static
EventType TypeConverter<EventType, ui::EventType>::ConvertFrom(
    ui::EventType type) {

#define MOJO_INPUT_EVENT_NAME(name) case ui::ET_##name: return EVENT_TYPE_##name

  switch (type) {
#include "mojo/services/public/cpp/input_events/lib/input_event_names.h"
    case ui::ET_LAST:
      NOTREACHED();
      break;
  }

#undef MOJO_INPUT_EVENT_NAME

  NOTREACHED();
  return EVENT_TYPE_UNKNOWN;
}

// static
ui::EventType TypeConverter<EventType, ui::EventType>::ConvertTo(
  EventType type) {

#define MOJO_INPUT_EVENT_NAME(name) case EVENT_TYPE_##name: return ui::ET_##name

  switch (type) {
#include "mojo/services/public/cpp/input_events/lib/input_event_names.h"
  }

#undef MOJO_INPUT_EVENT_NAME

  NOTREACHED();
  return ui::ET_UNKNOWN;
}


// static
EventPtr TypeConverter<EventPtr, ui::Event>::ConvertFrom(
    const ui::Event& input) {
  EventPtr event(Event::New());
  event->action = TypeConverter<EventType, ui::EventType>::ConvertFrom(
      input.type());
  event->flags = EventFlags(input.flags());
  event->time_stamp = input.time_stamp().ToInternalValue();

  if (input.IsMouseEvent() || input.IsTouchEvent()) {
    const ui::LocatedEvent* located_event =
        static_cast<const ui::LocatedEvent*>(&input);
    event->location =
        TypeConverter<PointPtr, gfx::Point>::ConvertFrom(
            located_event->location());
  }

  if (input.IsTouchEvent()) {
    const ui::TouchEvent* touch_event =
        static_cast<const ui::TouchEvent*>(&input);
    TouchDataPtr touch_data(TouchData::New());
    touch_data->pointer_id = touch_event->touch_id();
    event->touch_data = touch_data.Pass();
  } else if (input.IsKeyEvent()) {
    const ui::KeyEvent* key_event = static_cast<const ui::KeyEvent*>(&input);
    KeyDataPtr key_data(KeyData::New());
    key_data->key_code = key_event->key_code();
    key_data->is_char = key_event->is_char();
    event->key_data = key_data.Pass();
  } else if (input.IsMouseWheelEvent()) {
    const ui::MouseWheelEvent* wheel_event =
        static_cast<const ui::MouseWheelEvent*>(&input);
    MouseWheelDataPtr wheel_data(MouseWheelData::New());
    wheel_data->x_offset = wheel_event->x_offset();
    wheel_data->y_offset = wheel_event->y_offset();
    event->wheel_data = wheel_data.Pass();
  }
  return event.Pass();
}

// static
EventPtr TypeConverter<EventPtr, ui::KeyEvent>::ConvertFrom(
    const ui::KeyEvent& input) {
  return Event::From(static_cast<const ui::Event&>(input));
}

// static
scoped_ptr<ui::Event>
TypeConverter<EventPtr, scoped_ptr<ui::Event> >::ConvertTo(
    const EventPtr& input) {
  scoped_ptr<ui::Event> ui_event;
  ui::EventType ui_event_type =
      TypeConverter<EventType, ui::EventType>::ConvertTo(input->action);
  switch (input->action) {
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      if (input->key_data->is_char) {
        ui_event.reset(new ui::KeyEvent(
                           static_cast<base::char16>(input->key_data->key_code),
                           static_cast<ui::KeyboardCode>(
                               input->key_data->key_code),
                           input->flags));
      } else {
        ui_event.reset(new ui::KeyEvent(
                           ui_event_type,
                           static_cast<ui::KeyboardCode>(
                               input->key_data->key_code),
                           input->flags));
      }
      break;
    case EVENT_TYPE_MOUSE_PRESSED:
    case EVENT_TYPE_MOUSE_DRAGGED:
    case EVENT_TYPE_MOUSE_RELEASED:
    case EVENT_TYPE_MOUSE_MOVED:
    case EVENT_TYPE_MOUSE_ENTERED:
    case EVENT_TYPE_MOUSE_EXITED: {
      const gfx::PointF location(TypeConverter<PointPtr, gfx::Point>::ConvertTo(
                                     input->location));
      // TODO: last flags isn't right. Need to send changed_flags.
      ui_event.reset(new ui::MouseEvent(
                         ui_event_type,
                         location,
                         location,
                         ui::EventFlags(input->flags),
                         ui::EventFlags(input->flags)));
      break;
    }
    case EVENT_TYPE_MOUSEWHEEL: {
      const gfx::PointF location(TypeConverter<PointPtr, gfx::Point>::ConvertTo(
                                     input->location));
      const gfx::Vector2d offset(input->wheel_data->x_offset,
                                 input->wheel_data->y_offset);
      ui_event.reset(new ui::MouseWheelEvent(offset,
                                             location,
                                             location,
                                             ui::EventFlags(input->flags),
                                             ui::EventFlags(input->flags)));
      break;
    }
    case EVENT_TYPE_TOUCH_MOVED:
    case EVENT_TYPE_TOUCH_PRESSED:
    case EVENT_TYPE_TOUCH_CANCELLED:
    case EVENT_TYPE_TOUCH_RELEASED: {
      gfx::Point location(input->location->x, input->location->y);
      ui_event.reset(new ui::TouchEvent(
                         ui_event_type,
                         location,
                         ui::EventFlags(input->flags),
                         input->touch_data->pointer_id,
                         base::TimeDelta::FromInternalValue(input->time_stamp),
                         0.f, 0.f, 0.f, 0.f));
      break;
    }
    default:
      // TODO: support other types.
      // NOTIMPLEMENTED();
      ;
  }
  // TODO: need to support time_stamp.
  return ui_event.Pass();
}

}  // namespace mojo
