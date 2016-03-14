// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/event_param_traits.h"

#include <limits>

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ui {
namespace {

#define CAST_EVENT(T, e) (*static_cast<const T*>(e.get()))
#define FEQ(a, b) (a == b || (std::isnan(a) && std::isnan(b)))
#define ASSERT_FEQ(a, b) ASSERT_TRUE(FEQ(a, b))
#define MIN(T) std::numeric_limits<T>::min()
#define MAX(T) std::numeric_limits<T>::max()
#define IMIN MIN(int)
#define IMAX MAX(int)
#define FMIN MIN(float)
#define FMAX MAX(float)
#define FNAN std::numeric_limits<float>::quiet_NaN()

class EventParamTraitsTest : public testing::Test {
 protected:
  // Implements event downcasting as performed by param traits. This enables
  // testing the interface that client code actually uses: (de)serialization of
  // scoped Event pointers that contain a concrete type.
  static void CompareEvents(const ScopedEvent& a, const ScopedEvent& b) {
    ASSERT_EQ(!!a, !!b);
    ASSERT_EQ(a->type(), b->type());
    switch (a->type()) {
      case EventType::ET_MOUSE_PRESSED:
      case EventType::ET_MOUSE_DRAGGED:
      case EventType::ET_MOUSE_RELEASED:
      case EventType::ET_MOUSE_MOVED:
      case EventType::ET_MOUSE_ENTERED:
      case EventType::ET_MOUSE_EXITED:
      case EventType::ET_MOUSE_CAPTURE_CHANGED:
        Compare(CAST_EVENT(MouseEvent, a), CAST_EVENT(MouseEvent, b));
        break;
      case EventType::ET_KEY_PRESSED:
      case EventType::ET_KEY_RELEASED:
        Compare(CAST_EVENT(KeyEvent, a), CAST_EVENT(KeyEvent, b));
        break;
      case EventType::ET_MOUSEWHEEL:
        Compare(CAST_EVENT(MouseWheelEvent, a), CAST_EVENT(MouseWheelEvent, b));
        break;
      case EventType::ET_TOUCH_RELEASED:
      case EventType::ET_TOUCH_PRESSED:
      case EventType::ET_TOUCH_MOVED:
      case EventType::ET_TOUCH_CANCELLED:
      case EventType::ET_DROP_TARGET_EVENT:
        Compare(CAST_EVENT(TouchEvent, a), CAST_EVENT(TouchEvent, b));
        break;
      case EventType::ET_POINTER_DOWN:
      case EventType::ET_POINTER_MOVED:
      case EventType::ET_POINTER_UP:
      case EventType::ET_POINTER_CANCELLED:
      case EventType::ET_POINTER_ENTERED:
      case EventType::ET_POINTER_EXITED:
        Compare(CAST_EVENT(PointerEvent, a), CAST_EVENT(PointerEvent, b));
        break;
      case EventType::ET_GESTURE_SCROLL_BEGIN:
      case EventType::ET_GESTURE_SCROLL_END:
      case EventType::ET_GESTURE_SCROLL_UPDATE:
      case EventType::ET_GESTURE_SHOW_PRESS:
      case EventType::ET_GESTURE_WIN8_EDGE_SWIPE:
      case EventType::ET_GESTURE_TAP:
      case EventType::ET_GESTURE_TAP_DOWN:
      case EventType::ET_GESTURE_TAP_CANCEL:
      case EventType::ET_GESTURE_BEGIN:
      case EventType::ET_GESTURE_END:
      case EventType::ET_GESTURE_TWO_FINGER_TAP:
      case EventType::ET_GESTURE_PINCH_BEGIN:
      case EventType::ET_GESTURE_PINCH_END:
      case EventType::ET_GESTURE_PINCH_UPDATE:
      case EventType::ET_GESTURE_LONG_PRESS:
      case EventType::ET_GESTURE_LONG_TAP:
      case EventType::ET_GESTURE_SWIPE:
      case EventType::ET_GESTURE_TAP_UNCONFIRMED:
      case EventType::ET_GESTURE_DOUBLE_TAP:
        Compare(CAST_EVENT(GestureEvent, a), CAST_EVENT(GestureEvent, b));
        break;
      case EventType::ET_SCROLL:
        Compare(CAST_EVENT(ScrollEvent, a), CAST_EVENT(ScrollEvent, b));
        break;
      case EventType::ET_SCROLL_FLING_START:
      case EventType::ET_SCROLL_FLING_CANCEL:
        ASSERT_EQ(a->flags() & MouseEventFlags::EF_FROM_TOUCH,
                  b->flags() & MouseEventFlags::EF_FROM_TOUCH);
        if (a->flags() & MouseEventFlags::EF_FROM_TOUCH) {
          Compare(CAST_EVENT(GestureEvent, a), CAST_EVENT(GestureEvent, b));
        } else {
          Compare(CAST_EVENT(MouseEvent, a), CAST_EVENT(MouseEvent, b));
        }
        break;
      case EventType::ET_CANCEL_MODE:
        Compare(CAST_EVENT(CancelModeEvent, a), CAST_EVENT(CancelModeEvent, b));
        break;
      default:
        NOTREACHED();
    }
  }

#undef CAST_EVENT

#define CAST_EVENT(T, e) static_cast<const T&>(e)
#define COMPARE_BASE(T, a, b) Compare(CAST_EVENT(T, a), CAST_EVENT(T, b))

  static void Compare(const Event& a, const Event& b) {
    ASSERT_EQ(a.type(), b.type());
    ASSERT_EQ(a.name(), b.name());
    ASSERT_EQ(a.time_stamp(), b.time_stamp());
    ASSERT_EQ(a.flags(), b.flags());
    ASSERT_EQ(a.phase(), b.phase());
    ASSERT_EQ(a.result(), b.result());
    ASSERT_EQ(a.cancelable(), b.cancelable());
    ASSERT_EQ(a.IsShiftDown(), b.IsShiftDown());
    ASSERT_EQ(a.IsControlDown(), b.IsControlDown());
    ASSERT_EQ(a.IsAltDown(), b.IsAltDown());
    ASSERT_EQ(a.IsCommandDown(), b.IsCommandDown());
    ASSERT_EQ(a.IsAltGrDown(), b.IsAltGrDown());
    ASSERT_EQ(a.IsCapsLockOn(), b.IsCapsLockOn());
    ASSERT_EQ(a.IsKeyEvent(), b.IsKeyEvent());
    ASSERT_EQ(a.IsMouseEvent(), b.IsMouseEvent());
    ASSERT_EQ(a.IsTouchEvent(), b.IsTouchEvent());
    ASSERT_EQ(a.IsGestureEvent(), b.IsGestureEvent());
    ASSERT_EQ(a.IsEndingEvent(), b.IsEndingEvent());
    ASSERT_EQ(a.IsScrollEvent(), b.IsScrollEvent());
    ASSERT_EQ(a.IsScrollGestureEvent(), b.IsScrollGestureEvent());
    ASSERT_EQ(a.IsFlingScrollEvent(), b.IsFlingScrollEvent());
    ASSERT_EQ(a.IsMouseWheelEvent(), b.IsMouseWheelEvent());
    ASSERT_EQ(a.IsLocatedEvent(), b.IsLocatedEvent());
    ASSERT_EQ(a.handled(), b.handled());
  }

  static void Compare(const CancelModeEvent& a, const CancelModeEvent& b) {
    COMPARE_BASE(Event, a, b);
  }

  static void Compare(const LocatedEvent& a, const LocatedEvent& b) {
    COMPARE_BASE(Event, a, b);

    ASSERT_EQ(a.x(), b.x());
    ASSERT_EQ(a.y(), b.y());
    ASSERT_EQ(a.location(), b.location());
    ASSERT_EQ(a.location_f(), b.location_f());
    ASSERT_EQ(a.root_location(), b.root_location());
    ASSERT_EQ(a.root_location_f(), b.root_location_f());
  }

  static void Compare(const MouseEvent& a, const MouseEvent& b) {
    COMPARE_BASE(LocatedEvent, a, b);

    ASSERT_EQ(a.IsOnlyLeftMouseButton(), b.IsOnlyLeftMouseButton());
    ASSERT_EQ(a.IsLeftMouseButton(), b.IsLeftMouseButton());
    ASSERT_EQ(a.IsOnlyMiddleMouseButton(), b.IsOnlyMiddleMouseButton());
    ASSERT_EQ(a.IsMiddleMouseButton(), b.IsMiddleMouseButton());
    ASSERT_EQ(a.IsOnlyRightMouseButton(), b.IsOnlyRightMouseButton());
    ASSERT_EQ(a.IsRightMouseButton(), b.IsRightMouseButton());
    ASSERT_EQ(a.IsAnyButton(), b.IsAnyButton());
    ASSERT_EQ(a.button_flags(), b.button_flags());
    ASSERT_EQ(a.GetClickCount(), b.GetClickCount());
    ASSERT_EQ(a.changed_button_flags(), b.changed_button_flags());

    Compare(a.pointer_details(), b.pointer_details());
  }

  static void Compare(const MouseWheelEvent& a, const MouseWheelEvent& b) {
    COMPARE_BASE(MouseEvent, a, b);

    ASSERT_EQ(a.x_offset(), b.x_offset());
    ASSERT_EQ(a.y_offset(), b.y_offset());
    ASSERT_EQ(a.offset(), b.offset());
  }

  static void Compare(const TouchEvent& a, const TouchEvent& b) {
    COMPARE_BASE(LocatedEvent, a, b);

    ASSERT_EQ(a.touch_id(), b.touch_id());
    ASSERT_EQ(a.unique_event_id(), b.unique_event_id());
    ASSERT_FEQ(a.rotation_angle(), b.rotation_angle());
    ASSERT_EQ(a.may_cause_scrolling(), b.may_cause_scrolling());
    ASSERT_EQ(a.synchronous_handling_disabled(),
              b.synchronous_handling_disabled());

    Compare(a.pointer_details(), b.pointer_details());
  }

  static void Compare(const KeyEvent& a, const KeyEvent& b) {
    COMPARE_BASE(Event, a, b);

    ASSERT_EQ(a.GetCharacter(), b.GetCharacter());
    ASSERT_EQ(a.GetUnmodifiedText(), b.GetUnmodifiedText());
    ASSERT_EQ(a.GetText(), b.GetText());
    ASSERT_EQ(a.is_char(), b.is_char());
    ASSERT_EQ(a.is_repeat(), b.is_repeat());
    ASSERT_EQ(a.key_code(), b.key_code());
    ASSERT_EQ(a.GetLocatedWindowsKeyboardCode(),
              b.GetLocatedWindowsKeyboardCode());
    ASSERT_EQ(a.GetConflatedWindowsKeyCode(), b.GetConflatedWindowsKeyCode());
    ASSERT_EQ(a.IsUnicodeKeyCode(), b.IsUnicodeKeyCode());
    ASSERT_EQ(a.code(), b.code());
    ASSERT_EQ(a.GetCodeString(), b.GetCodeString());
    ASSERT_EQ(a.GetDomKey(), b.GetDomKey());
  }

  static void Compare(const ScrollEvent& a, const ScrollEvent& b) {
    COMPARE_BASE(MouseEvent, a, b);

    ASSERT_FEQ(a.x_offset(), b.x_offset());
    ASSERT_FEQ(a.y_offset(), b.y_offset());
    ASSERT_FEQ(a.x_offset_ordinal(), b.x_offset_ordinal());
    ASSERT_FEQ(a.y_offset_ordinal(), b.y_offset_ordinal());
    ASSERT_EQ(a.finger_count(), b.finger_count());
  }

  static void Compare(const GestureEvent& a, const GestureEvent& b) {
    COMPARE_BASE(LocatedEvent, a, b);

    ASSERT_EQ(a.details(), b.details());
  }

  static void Compare(const PointerDetails& a, const PointerDetails& b) {
    ASSERT_EQ(a.pointer_type, b.pointer_type);
    ASSERT_FEQ(a.radius_x, b.radius_x);
    ASSERT_FEQ(a.radius_y, b.radius_y);
    ASSERT_FEQ(a.force, b.force);
    ASSERT_FEQ(a.tilt_x, b.tilt_x);
    ASSERT_FEQ(a.tilt_y, b.tilt_y);
  }

  static void Verify(const ScopedEvent& event_in) {
    IPC::Message msg;
    IPC::ParamTraits<ScopedEvent>::Write(&msg, event_in);

    ScopedEvent event_out;
    base::PickleIterator iter(msg);
    EXPECT_TRUE(IPC::ParamTraits<ScopedEvent>::Read(&msg, &iter, &event_out));

    CompareEvents(event_in, event_out);

    // Perform a sanity check that logging doesn't explode.
    std::string event_in_string;
    IPC::ParamTraits<ScopedEvent>::Log(event_in, &event_in_string);
    std::string event_out_string;
    IPC::ParamTraits<ScopedEvent>::Log(event_out, &event_out_string);
    ASSERT_FALSE(event_in_string.empty());
    EXPECT_EQ(event_in_string, event_out_string);
  }

  static GestureEventDetails CreateCornerCaseGestureEventDetails(
      EventType type) {
    GestureEventDetails details;

    // Only some types support |delta_x| and |delta_y| parameters.
    if (type == EventType::ET_GESTURE_SCROLL_BEGIN ||
        type == EventType::ET_GESTURE_SCROLL_UPDATE ||
        type == EventType::ET_SCROLL_FLING_START ||
        type == EventType::ET_GESTURE_TWO_FINGER_TAP ||
        type == EventType::ET_GESTURE_SWIPE) {
      details = GestureEventDetails(type, FMIN, FMAX);
    } else {
      details = GestureEventDetails(type);
    }

    details.set_bounding_box(gfx::RectF(FMIN, FMAX, FNAN, FNAN));

    // Note: Positive values and |type| check dodges DCHECKs that are not being
    // tested here.
    details.set_touch_points(IMAX);
    if (type == EventType::ET_GESTURE_TAP ||
        type == EventType::ET_GESTURE_TAP_UNCONFIRMED ||
        type == EventType::ET_GESTURE_DOUBLE_TAP) {
      details.set_tap_count(IMAX);
    }
    if (type == EventType::ET_GESTURE_PINCH_UPDATE) {
      details.set_scale(FMAX);
    }

    return details;
  }
};

TEST_F(EventParamTraitsTest, GoodCancelModeEvent) {
  ScopedEvent event(new CancelModeEvent());
  Verify(event);
}

TEST_F(EventParamTraitsTest, GoodSimpleMouseEvent) {
  EventType event_types[7] = {
      EventType::ET_MOUSE_PRESSED,         EventType::ET_MOUSE_DRAGGED,
      EventType::ET_MOUSE_RELEASED,        EventType::ET_MOUSE_MOVED,
      EventType::ET_MOUSE_ENTERED,         EventType::ET_MOUSE_EXITED,
      EventType::ET_MOUSE_CAPTURE_CHANGED,
  };
  for (int i = 0; i < 7; i++) {
    ScopedEvent event(new MouseEvent(event_types[i], gfx::Point(), gfx::Point(),
                                     base::TimeDelta(), 0, 0));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodCornerCaseMouseEvent) {
  EventType event_types[7] = {
      EventType::ET_MOUSE_PRESSED,         EventType::ET_MOUSE_DRAGGED,
      EventType::ET_MOUSE_RELEASED,        EventType::ET_MOUSE_MOVED,
      EventType::ET_MOUSE_ENTERED,         EventType::ET_MOUSE_EXITED,
      EventType::ET_MOUSE_CAPTURE_CHANGED,
  };
  for (int i = 0; i < 7; i++) {
    ScopedEvent event(new MouseEvent(event_types[i], gfx::Point(IMIN, IMIN),
                                     gfx::Point(IMAX, IMAX),
                                     base::TimeDelta::Max(), IMIN, IMAX));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodSimpleMouseWheelEvent) {
  ScopedEvent event(new MouseWheelEvent(gfx::Vector2d(), gfx::Point(),
                                        gfx::Point(), base::TimeDelta(), 0, 0));
  Verify(event);
}

TEST_F(EventParamTraitsTest, GoodCornerCaseMouseWheelEvent) {
  ScopedEvent event(new MouseWheelEvent(
      gfx::Vector2d(IMIN, IMAX), gfx::Point(IMIN, IMIN), gfx::Point(IMAX, IMAX),
      base::TimeDelta::Max(), IMIN, IMAX));
  Verify(event);
}

TEST_F(EventParamTraitsTest, GoodSimpleTouchEvent) {
  EventType event_types[5] = {
      EventType::ET_TOUCH_RELEASED,    EventType::ET_TOUCH_PRESSED,
      EventType::ET_TOUCH_MOVED,       EventType::ET_TOUCH_CANCELLED,
      EventType::ET_DROP_TARGET_EVENT,
  };
  for (int i = 0; i < 5; i++) {
    ScopedEvent event(new TouchEvent(event_types[i], gfx::Point(), 0, 0,
                                     base::TimeDelta(), 0.0, 0.0, 0.0, 0.0));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodCornerCaseTouchEvent) {
  EventType event_types[5] = {
      EventType::ET_TOUCH_RELEASED,    EventType::ET_TOUCH_PRESSED,
      EventType::ET_TOUCH_MOVED,       EventType::ET_TOUCH_CANCELLED,
      EventType::ET_DROP_TARGET_EVENT,
  };
  for (int i = 0; i < 5; i++) {
    ScopedEvent event(new TouchEvent(event_types[i], gfx::Point(IMIN, IMAX),
                                     IMIN, IMAX, base::TimeDelta::Max(), FMIN,
                                     FMAX, FNAN, FNAN));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodSimpleKeyEvent) {
  EventType event_types[2] = {
      EventType::ET_KEY_PRESSED, EventType::ET_KEY_RELEASED,
  };
  for (int i = 0; i < 2; i++) {
    ScopedEvent event(new KeyEvent(event_types[i], KeyboardCode::VKEY_UNKNOWN,
                                   static_cast<DomCode>(0), 0, DomKey(0),
                                   base::TimeDelta()));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodCornerCaseKeyEvent) {
  EventType event_types[2] = {
      EventType::ET_KEY_PRESSED, EventType::ET_KEY_RELEASED,
  };
  for (int i = 0; i < 2; i++) {
    ScopedEvent event(new KeyEvent(event_types[i], KeyboardCode::VKEY_OEM_CLEAR,
                                   static_cast<DomCode>(0x0c028c), IMIN,
                                   MIN(DomKey), base::TimeDelta::Max()));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodSimpleScrollEvent) {
  ScopedEvent event(new ScrollEvent(EventType::ET_SCROLL, gfx::Point(),
                                    base::TimeDelta(), 0, 0.0, 0.0, 0.0, 0.0,
                                    0));
  Verify(event);
}

TEST_F(EventParamTraitsTest, GoodCornerCaseScrollEvent) {
  ScopedEvent event(new ScrollEvent(EventType::ET_SCROLL, gfx::Point(),
                                    base::TimeDelta(), IMIN, FMIN, FMAX, FNAN,
                                    FMIN, IMAX));
  Verify(event);
}

TEST_F(EventParamTraitsTest, GoodSimpleGestureEvent) {
  EventType event_types[19] = {
      EventType::ET_GESTURE_SCROLL_BEGIN,
      EventType::ET_GESTURE_SCROLL_END,
      EventType::ET_GESTURE_SCROLL_UPDATE,
      EventType::ET_GESTURE_SHOW_PRESS,
      EventType::ET_GESTURE_WIN8_EDGE_SWIPE,
      EventType::ET_GESTURE_TAP,
      EventType::ET_GESTURE_TAP_DOWN,
      EventType::ET_GESTURE_TAP_CANCEL,
      EventType::ET_GESTURE_BEGIN,
      EventType::ET_GESTURE_END,
      EventType::ET_GESTURE_TWO_FINGER_TAP,
      EventType::ET_GESTURE_PINCH_BEGIN,
      EventType::ET_GESTURE_PINCH_END,
      EventType::ET_GESTURE_PINCH_UPDATE,
      EventType::ET_GESTURE_LONG_PRESS,
      EventType::ET_GESTURE_LONG_TAP,
      EventType::ET_GESTURE_SWIPE,
      EventType::ET_GESTURE_TAP_UNCONFIRMED,
      EventType::ET_GESTURE_DOUBLE_TAP,
  };
  for (int i = 0; i < 19; i++) {
    ScopedEvent event(new GestureEvent(0.0, 0.0, 0, base::TimeDelta(),
                                       GestureEventDetails(event_types[i])));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodCornerCaseGestureEvent) {
  EventType event_types[17] = {
      EventType::ET_GESTURE_SCROLL_UPDATE,
      EventType::ET_GESTURE_SHOW_PRESS,
      EventType::ET_GESTURE_WIN8_EDGE_SWIPE,
      EventType::ET_GESTURE_TAP,
      EventType::ET_GESTURE_TAP_DOWN,
      EventType::ET_GESTURE_TAP_CANCEL,
      EventType::ET_GESTURE_BEGIN,
      EventType::ET_GESTURE_END,
      EventType::ET_GESTURE_TWO_FINGER_TAP,
      EventType::ET_GESTURE_PINCH_BEGIN,
      EventType::ET_GESTURE_PINCH_END,
      EventType::ET_GESTURE_PINCH_UPDATE,
      EventType::ET_GESTURE_LONG_PRESS,
      EventType::ET_GESTURE_LONG_TAP,
      EventType::ET_GESTURE_SWIPE,
      EventType::ET_GESTURE_TAP_UNCONFIRMED,
      EventType::ET_GESTURE_DOUBLE_TAP,
  };
  for (int i = 0; i < 17; i++) {
    ScopedEvent event(
        new GestureEvent(0.0, 0.0, 0, base::TimeDelta(),
                         CreateCornerCaseGestureEventDetails(event_types[i])));
    Verify(event);
  }
}

TEST_F(EventParamTraitsTest, GoodSimplePointerEvent) {
  EventType event_types[] = {
      ET_POINTER_DOWN,      ET_POINTER_MOVED,   ET_POINTER_UP,
      ET_POINTER_CANCELLED, ET_POINTER_ENTERED, ET_POINTER_EXITED,
  };
  EventPointerType pointer_types[] = {
      EventPointerType::POINTER_TYPE_MOUSE,
      EventPointerType::POINTER_TYPE_TOUCH,
  };

  for (size_t i = 0; i < arraysize(event_types); i++) {
    for (size_t j = 0; j < arraysize(pointer_types); j++) {
      ScopedEvent event(new PointerEvent(event_types[i], pointer_types[j],
                                         gfx::Point(), gfx::Point(), 0, 0,
                                         base::TimeDelta()));
      Verify(event);
    }
  }
}

#undef FEQ
#undef ASSERT_FEQ
#undef CAST_EVENT
#undef COMPARE_BASE
#undef MIN
#undef MAX
#undef IMIN
#undef IMAX
#undef FMIN
#undef FMAX
#undef FNAN

}  // namespace
}  // namespace ui
