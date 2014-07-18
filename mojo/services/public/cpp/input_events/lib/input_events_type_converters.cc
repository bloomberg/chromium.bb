// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"

#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace mojo {

// static
EventPtr TypeConverter<EventPtr, ui::Event>::ConvertFrom(
    const ui::Event& input) {
  EventPtr event(Event::New());
  event->action = input.type();
  event->flags = input.flags();
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
  switch (input->action) {
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      ui_event.reset(new ui::KeyEvent(
                         static_cast<ui::EventType>(input->action),
                         static_cast<ui::KeyboardCode>(
                             input->key_data->key_code),
                         input->flags,
                         input->key_data->is_char));
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED: {
      const gfx::PointF location(TypeConverter<PointPtr, gfx::Point>::ConvertTo(
                                     input->location));
      // TODO: last flags isn't right. Need to send changed_flags.
      ui_event.reset(new ui::MouseEvent(
                         static_cast<ui::EventType>(input->action),
                         location,
                         location,
                         input->flags,
                         input->flags));
      break;
    }
    case ui::ET_MOUSEWHEEL: {
      const gfx::PointF location(TypeConverter<PointPtr, gfx::Point>::ConvertTo(
                                     input->location));
      const gfx::Vector2d offset(input->wheel_data->x_offset,
                                 input->wheel_data->y_offset);
      ui_event.reset(new ui::MouseWheelEvent(offset,
                                             location,
                                             location,
                                             input->flags,
                                             input->flags));
      break;
    }
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_TOUCH_RELEASED: {
      gfx::Point location(input->location->x, input->location->y);
      ui_event.reset(new ui::TouchEvent(
                         static_cast<ui::EventType>(input->action),
                         location,
                         input->flags,
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
