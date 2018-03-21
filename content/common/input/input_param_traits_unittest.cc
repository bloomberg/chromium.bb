// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_param_traits.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "content/common/input/input_event.h"
#include "content/common/input_messages.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebKeyboardEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "third_party/WebKit/public/platform/WebTouchEvent.h"

namespace content {
namespace {

typedef std::vector<std::unique_ptr<InputEvent>> InputEvents;

class InputParamTraitsTest : public testing::Test {
 protected:
  static void Compare(const InputEvent* a, const InputEvent* b) {
    EXPECT_EQ(!!a->web_event, !!b->web_event);
    if (a->web_event && b->web_event) {
      const size_t a_size = a->web_event->size();
      ASSERT_EQ(a_size, b->web_event->size());
      EXPECT_EQ(0, memcmp(a->web_event.get(), b->web_event.get(), a_size));
    }
    EXPECT_EQ(a->latency_info.latency_components().size(),
              b->latency_info.latency_components().size());
  }

  static void Compare(const InputEvents* a, const InputEvents* b) {
    for (size_t i = 0; i < a->size(); ++i)
      Compare((*a)[i].get(), (*b)[i].get());
  }

  static void Verify(const InputEvents& events_in) {
    IPC::Message msg;
    IPC::ParamTraits<InputEvents>::Write(&msg, events_in);

    InputEvents events_out;
    base::PickleIterator iter(msg);
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
  base::PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ReadParam(&msg, &iter, &event_out));
}

TEST_F(InputParamTraitsTest, InitializedEvents) {
  InputEvents events;

  ui::LatencyInfo latency;
  latency.set_trace_id(5);

  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::kRawKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.native_key_code = 5;
  events.push_back(std::make_unique<InputEvent>(key_event, latency));

  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.delta_x = 10;
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1, 1);
  events.push_back(std::make_unique<InputEvent>(wheel_event, latency));

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(10, 0);
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 2, 2);
  events.push_back(std::make_unique<InputEvent>(mouse_event, latency));

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_event.SetPositionInWidget(gfx::PointF(-1, 0));
  events.push_back(std::make_unique<InputEvent>(gesture_event, latency));

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::kTouchStart, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].radius_x = 1;
  events.push_back(std::make_unique<InputEvent>(touch_event, latency));

  Verify(events);
}

}  // namespace
}  // namespace content
