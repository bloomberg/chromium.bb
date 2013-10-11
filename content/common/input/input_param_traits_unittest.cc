// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_param_traits.h"

#include "content/common/input/input_event.h"
#include "content/common/input_messages.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {
namespace {

typedef ScopedVector<InputEvent> InputEvents;

void AddTo(InputEvents& events, const WebKit::WebInputEvent& event) {
  events.push_back(new InputEvent(event, ui::LatencyInfo(), false));
}

class InputParamTraitsTest : public testing::Test {
 protected:
  void Compare(const InputEvent* a, const InputEvent* b) {
    EXPECT_EQ(!!a->web_event, !!b->web_event);
    if (a->web_event && b->web_event) {
      const size_t a_size = a->web_event->size;
      ASSERT_EQ(a_size, b->web_event->size);
      EXPECT_EQ(0, memcmp(a->web_event.get(), b->web_event.get(), a_size));
    }
    EXPECT_EQ(a->latency_info.latency_components.size(),
              b->latency_info.latency_components.size());
    EXPECT_EQ(a->is_keyboard_shortcut, b->is_keyboard_shortcut);
  }

  void Compare(const InputEvents* a, const InputEvents* b) {
    for (size_t i = 0; i < a->size(); ++i)
      Compare((*a)[i], (*b)[i]);
  }

  void Verify(const InputEvents& events_in) {
    IPC::Message msg;
    IPC::ParamTraits<InputEvents>::Write(&msg, events_in);

    InputEvents events_out;
    PickleIterator iter(msg);
    EXPECT_TRUE(IPC::ParamTraits<InputEvents>::Read(&msg, &iter, &events_out));

    Compare(&events_in, &events_out);

    // Perform a sanity check that logging doesn't explode.
    std::string events_in_string;
    IPC::ParamTraits<InputEvents>::Log(events_in, &events_in_string);
    std::string events_out_string;
    IPC::ParamTraits<InputEvents>::Log(events_out, &events_out_string);
    ASSERT_FALSE(events_in_string.empty());
    EXPECT_EQ(events_in_string, events_out_string);
  }
};

TEST_F(InputParamTraitsTest, UninitializedEvents) {
  InputEvent event;

  IPC::Message msg;
  IPC::WriteParam(&msg, event);

  InputEvent event_out;
  PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ReadParam(&msg, &iter, &event_out));
}

TEST_F(InputParamTraitsTest, InitializedEvents) {
  InputEvents events;

  ui::LatencyInfo latency;

  WebKit::WebKeyboardEvent key_event;
  key_event.type = WebKit::WebInputEvent::RawKeyDown;
  key_event.nativeKeyCode = 5;
  events.push_back(new InputEvent(key_event, latency, false));

  WebKit::WebMouseWheelEvent wheel_event;
  wheel_event.type = WebKit::WebInputEvent::MouseWheel;
  wheel_event.deltaX = 10;
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1, 1);
  events.push_back(new InputEvent(wheel_event, latency, false));

  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.x = 10;
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 2, 2);
  events.push_back(new InputEvent(mouse_event, latency, false));

  WebKit::WebGestureEvent gesture_event;
  gesture_event.type = WebKit::WebInputEvent::GestureScrollBegin;
  gesture_event.x = -1;
  events.push_back(new InputEvent(gesture_event, latency, false));

  WebKit::WebTouchEvent touch_event;
  touch_event.type = WebKit::WebInputEvent::TouchStart;
  touch_event.touchesLength = 1;
  touch_event.touches[0].radiusX = 1;
  events.push_back(new InputEvent(touch_event, latency, false));

  Verify(events);
}

}  // namespace
}  // namespace content
