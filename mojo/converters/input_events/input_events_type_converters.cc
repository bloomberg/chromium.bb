// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/input_events/input_events_type_converters.h"

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#endif

#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/mojo_extended_key_event_data.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace mojo {
namespace {

ui::EventType MojoMouseEventTypeToUIEvent(const EventPtr& event) {
  DCHECK(event->pointer_data);
  DCHECK_EQ(POINTER_KIND_MOUSE, event->pointer_data->kind);
  switch (event->action) {
    case EVENT_TYPE_POINTER_DOWN:
      return ui::ET_MOUSE_PRESSED;

    case EVENT_TYPE_POINTER_UP:
      return ui::ET_MOUSE_RELEASED;

    case EVENT_TYPE_POINTER_MOVE:
      DCHECK(event->pointer_data);
      if (event->flags &
          (EVENT_FLAGS_LEFT_MOUSE_BUTTON | EVENT_FLAGS_MIDDLE_MOUSE_BUTTON |
           EVENT_FLAGS_RIGHT_MOUSE_BUTTON)) {
        return ui::ET_MOUSE_DRAGGED;
      }
      return ui::ET_MOUSE_MOVED;

    default:
      NOTREACHED();
  }

  return ui::ET_MOUSE_RELEASED;
}

ui::EventType MojoTouchEventTypeToUIEvent(const EventPtr& event) {
  DCHECK(event->pointer_data);
  DCHECK_EQ(POINTER_KIND_TOUCH, event->pointer_data->kind);
  switch (event->action) {
    case EVENT_TYPE_POINTER_DOWN:
      return ui::ET_TOUCH_PRESSED;

    case EVENT_TYPE_POINTER_UP:
      return ui::ET_TOUCH_RELEASED;

    case EVENT_TYPE_POINTER_MOVE:
      return ui::ET_TOUCH_MOVED;

    case EVENT_TYPE_POINTER_CANCEL:
      return ui::ET_TOUCH_CANCELLED;

    default:
      NOTREACHED();
  }

  return ui::ET_TOUCH_CANCELLED;
}

ui::EventType MojoWheelEventTypeToUIEvent(const EventPtr& event) {
  DCHECK(event->wheel_data);
  return ui::ET_MOUSEWHEEL;
}

void SetPointerDataLocationFromEvent(const ui::LocatedEvent& located_event,
                                     mojo::LocationData* location_data) {
  location_data->x = located_event.location_f().x();
  location_data->y = located_event.location_f().y();
  location_data->screen_x = located_event.root_location_f().x();
  location_data->screen_y = located_event.root_location_f().y();
}

}  // namespace

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
EventType TypeConverter<EventType, ui::EventType>::Convert(ui::EventType type) {
  switch (type) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
      return EVENT_TYPE_POINTER_DOWN;

    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_TOUCH_MOVED:
      return EVENT_TYPE_POINTER_MOVE;

    case ui::ET_MOUSEWHEEL:
      return EVENT_TYPE_WHEEL;

    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
      return EVENT_TYPE_POINTER_UP;

    case ui::ET_TOUCH_CANCELLED:
      return EVENT_TYPE_POINTER_CANCEL;

    case ui::ET_KEY_PRESSED:
      return EVENT_TYPE_KEY_PRESSED;

    case ui::ET_KEY_RELEASED:
      return EVENT_TYPE_KEY_RELEASED;

    default:
      break;
  }
  return EVENT_TYPE_UNKNOWN;
}

EventPtr TypeConverter<EventPtr, ui::Event>::Convert(const ui::Event& input) {
  const EventType type = ConvertTo<EventType>(input.type());
  if (type == EVENT_TYPE_UNKNOWN)
    return nullptr;

  EventPtr event = Event::New();
  event->action = type;
  event->flags = EventFlags(input.flags());
  event->time_stamp = input.time_stamp().ToInternalValue();

  if (input.IsMouseWheelEvent()) {
    const ui::MouseWheelEvent* wheel_event =
        static_cast<const ui::MouseWheelEvent*>(&input);

    WheelDataPtr wheel_data(WheelData::New());
    LocationDataPtr location_data(LocationData::New());
    const ui::LocatedEvent* located_event =
        static_cast<const ui::LocatedEvent*>(&input);
    SetPointerDataLocationFromEvent(*located_event, location_data.get());
    wheel_data->location = location_data.Pass();

    // TODO(rjkroege): Support page scrolling on windows by directly
    // cracking into a mojo event when the native event is available.
    wheel_data->mode = WHEEL_MODE_LINE;
    // TODO(rjkroege): Support precise scrolling deltas.

    if ((input.flags() & ui::EF_SHIFT_DOWN) != 0 &&
        wheel_event->x_offset() == 0) {
      wheel_data->delta_x = wheel_event->y_offset();
      wheel_data->delta_y = 0;
      wheel_data->delta_z = 0;
    } else {
      // TODO(rjkroege): support z in ui::Events.
      wheel_data->delta_x = wheel_event->x_offset();
      wheel_data->delta_y = wheel_event->y_offset();
      wheel_data->delta_z = 0;
    }
    event->wheel_data = wheel_data.Pass();
  } else if (input.IsMouseEvent()) {
    const ui::LocatedEvent* located_event =
        static_cast<const ui::LocatedEvent*>(&input);
    PointerDataPtr pointer_data(PointerData::New());
    // TODO(sky): come up with a better way to handle this.
    pointer_data->pointer_id = std::numeric_limits<int32>::max();
    pointer_data->kind = POINTER_KIND_MOUSE;
    LocationDataPtr location_data(LocationData::New());
    SetPointerDataLocationFromEvent(*located_event, location_data.get());
    pointer_data->location = location_data.Pass();

    event->pointer_data = pointer_data.Pass();
  } else if (input.IsTouchEvent()) {
    const ui::TouchEvent* touch_event =
        static_cast<const ui::TouchEvent*>(&input);

    PointerDataPtr pointer_data(PointerData::New());
    pointer_data->pointer_id = touch_event->touch_id();
    pointer_data->kind = POINTER_KIND_TOUCH;
    LocationDataPtr location_data(LocationData::New());
    SetPointerDataLocationFromEvent(*touch_event, location_data.get());
    pointer_data->location = location_data.Pass();

    BrushDataPtr brush_data(BrushData::New());

    // TODO(rjk): this is in the wrong coordinate system
    brush_data->width = touch_event->pointer_details().radius_x();
    brush_data->height = touch_event->pointer_details().radius_y();
    // TODO(rjk): update for touch_event->rotation_angle();
    brush_data->pressure = touch_event->pointer_details().force();
    brush_data->tilt_y = 0;
    brush_data->tilt_z = 0;
    pointer_data->brush_data = brush_data.Pass();
    event->pointer_data = pointer_data.Pass();

    // TODO(rjkroege): Plumb raw pointer events on windows.
    // TODO(rjkroege): Handle force-touch on MacOS
    // TODO(rjkroege): Adjust brush data appropriately for Android.
  } else if (input.IsKeyEvent()) {
    const ui::KeyEvent* key_event = static_cast<const ui::KeyEvent*>(&input);
    KeyDataPtr key_data(KeyData::New());
    key_data->key_code = key_event->GetConflatedWindowsKeyCode();
    key_data->native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(key_event->code());
    key_data->is_char = key_event->is_char();
    key_data->character = key_event->GetCharacter();

    if (key_event->extended_key_event_data()) {
      const MojoExtendedKeyEventData* data =
          static_cast<const MojoExtendedKeyEventData*>(
              key_event->extended_key_event_data());
      key_data->windows_key_code = static_cast<mojo::KeyboardCode>(
          data->windows_key_code());
      key_data->text = data->text();
      key_data->unmodified_text = data->unmodified_text();
    } else {
      key_data->windows_key_code = static_cast<mojo::KeyboardCode>(
          key_event->GetLocatedWindowsKeyboardCode());
      key_data->text = key_event->GetText();
      key_data->unmodified_text = key_event->GetUnmodifiedText();
    }
    event->key_data = key_data.Pass();
  }
  return event.Pass();
}

// static
EventPtr TypeConverter<EventPtr, ui::KeyEvent>::Convert(
    const ui::KeyEvent& input) {
  return Event::From(static_cast<const ui::Event&>(input));
}

// static
scoped_ptr<ui::Event> TypeConverter<scoped_ptr<ui::Event>, EventPtr>::Convert(
    const EventPtr& input) {
  gfx::PointF location;
  gfx::PointF screen_location;
  if (input->pointer_data && input->pointer_data->location) {
    location.SetPoint(input->pointer_data->location->x,
                      input->pointer_data->location->y);
    screen_location.SetPoint(input->pointer_data->location->screen_x,
                             input->pointer_data->location->screen_y);
  }

  switch (input->action) {
    case EVENT_TYPE_KEY_PRESSED:
    case EVENT_TYPE_KEY_RELEASED: {
      scoped_ptr<ui::KeyEvent> key_event;
      if (input->key_data->is_char) {
        key_event.reset(new ui::KeyEvent(
            static_cast<base::char16>(input->key_data->character),
            static_cast<ui::KeyboardCode>(
                input->key_data->key_code),
            input->flags));
      } else {
        key_event.reset(new ui::KeyEvent(
            input->action == EVENT_TYPE_KEY_PRESSED ? ui::ET_KEY_PRESSED
                                                    : ui::ET_KEY_RELEASED,

            static_cast<ui::KeyboardCode>(input->key_data->key_code),
            input->flags));
      }
      key_event->SetExtendedKeyEventData(scoped_ptr<ui::ExtendedKeyEventData>(
          new MojoExtendedKeyEventData(
              static_cast<int32_t>(input->key_data->windows_key_code),
              input->key_data->text,
              input->key_data->unmodified_text)));
      return key_event.Pass();
    }
    case EVENT_TYPE_POINTER_DOWN:
    case EVENT_TYPE_POINTER_UP:
    case EVENT_TYPE_POINTER_MOVE:
    case EVENT_TYPE_POINTER_CANCEL: {
      switch (input->pointer_data->kind) {
        case POINTER_KIND_MOUSE: {
          // TODO: last flags isn't right. Need to send changed_flags.
          scoped_ptr<ui::MouseEvent> event(new ui::MouseEvent(
              MojoMouseEventTypeToUIEvent(input), location, screen_location,
              ui::EventTimeForNow(), ui::EventFlags(input->flags),
              ui::EventFlags(input->flags)));
          return event.Pass();
        } break;
        case POINTER_KIND_TOUCH: {
          DCHECK(input->pointer_data->brush_data);
          scoped_ptr<ui::TouchEvent> touch_event(new ui::TouchEvent(
              MojoTouchEventTypeToUIEvent(input), location,
              ui::EventFlags(input->flags), input->pointer_data->pointer_id,
              base::TimeDelta::FromInternalValue(input->time_stamp),
              input->pointer_data->brush_data->width,
              input->pointer_data->brush_data->height, 0,
              input->pointer_data->brush_data->pressure));
          touch_event->set_root_location(screen_location);
          return touch_event.Pass();
        } break;
        case POINTER_KIND_PEN:
          NOTIMPLEMENTED();
          break;
      }
    } break;
    case EVENT_TYPE_WHEEL: {
      DCHECK(input->wheel_data);
      scoped_ptr<ui::MouseEvent> pre_wheel_event(new ui::MouseEvent(
          MojoWheelEventTypeToUIEvent(input), location, screen_location,
          ui::EventTimeForNow(), ui::EventFlags(input->flags),
          ui::EventFlags(input->flags)));
      scoped_ptr<ui::MouseEvent> wheel_event(new ui::MouseWheelEvent(
          *pre_wheel_event, static_cast<int>(input->wheel_data->delta_x),
          static_cast<int>(input->wheel_data->delta_y)));
      return wheel_event.Pass();
    } break;

    default:
      NOTIMPLEMENTED();
  }
  // TODO: need to support time_stamp.
  return nullptr;
}

}  // namespace mojo
