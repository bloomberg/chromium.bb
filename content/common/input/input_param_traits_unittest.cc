// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_param_traits.h"

#include "content/common/input/event_packet.h"
#include "content/common/input/input_event.h"
#include "content/common/input/ipc_input_event_payload.h"
#include "content/common/input/web_input_event_payload.h"
#include "content/common/input_messages.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {
namespace {

class InputParamTraitsTest : public testing::Test {
 protected:
  void Compare(const WebInputEventPayload* a, const WebInputEventPayload* b) {
    EXPECT_EQ(!!a->web_event(), !!b->web_event());
    if (a->web_event() && b->web_event()) {
      const size_t a_size = a->web_event()->size;
      ASSERT_EQ(a_size, b->web_event()->size);
      EXPECT_EQ(0, memcmp(a->web_event(), b->web_event(), a_size));
    }
    EXPECT_EQ(a->latency_info().latency_components.size(),
              b->latency_info().latency_components.size());
    EXPECT_EQ(a->is_keyboard_shortcut(), b->is_keyboard_shortcut());
  }

  void Compare(const IPCInputEventPayload* a, const IPCInputEventPayload* b) {
    EXPECT_EQ(!!a->message, !!b->message);
    if (a->message && b->message) {
      EXPECT_EQ(a->message->type(), b->message->type());
      EXPECT_EQ(a->message->routing_id(), b->message->routing_id());
    }
  }

  void Compare(const InputEvent::Payload* a, const InputEvent::Payload* b) {
    ASSERT_EQ(!!a, !!b);
    if (!a)
      return;
    switch (a->GetType()) {
      case InputEvent::Payload::IPC_MESSAGE:
        Compare(IPCInputEventPayload::Cast(a), IPCInputEventPayload::Cast(b));
        break;
      case InputEvent::Payload::WEB_INPUT_EVENT:
        Compare(WebInputEventPayload::Cast(a), WebInputEventPayload::Cast(b));
      default:
        break;
    }
  }

  void Compare(const InputEvent* a, const InputEvent* b) {
    EXPECT_EQ(a->id(), b->id());
    EXPECT_EQ(a->valid(), b->valid());
    Compare(a->payload(), b->payload());
  }

  void Compare(const EventPacket* a, const EventPacket* b) {
    EXPECT_EQ(a->id(), b->id());
    ASSERT_EQ(a->size(), b->size());
    for (size_t i = 0; i < a->size(); ++i)
      Compare(a->events()[i], b->events()[i]);
  }

  void Verify(const EventPacket& packet_in) {
    IPC::Message msg;
    IPC::ParamTraits<EventPacket>::Write(&msg, packet_in);

    EventPacket packet_out;
    PickleIterator iter(msg);
    EXPECT_TRUE(IPC::ParamTraits<EventPacket>::Read(&msg, &iter, &packet_out));

    Compare(&packet_in, &packet_out);

    // Perform a sanity check that logging doesn't explode.
    std::string packet_in_string;
    IPC::ParamTraits<EventPacket>::Log(packet_in, &packet_in_string);
    std::string packet_out_string;
    IPC::ParamTraits<EventPacket>::Log(packet_out, &packet_out_string);
    ASSERT_FALSE(packet_in_string.empty());
    EXPECT_EQ(packet_in_string, packet_out_string);
  }
};

TEST_F(InputParamTraitsTest, EventPacketEmpty) {
  EventPacket packet_in;
  IPC::Message msg;
  IPC::ParamTraits<EventPacket>::Write(&msg, packet_in);

  EventPacket packet_out;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<EventPacket>::Read(&msg, &iter, &packet_out));

  Compare(&packet_in, &packet_out);
}

TEST_F(InputParamTraitsTest, EventPacketUninitializedEvents) {
  EventPacket packet_in;
  packet_in.set_id(1);
  packet_in.Add(InputEvent::Create(1, WebInputEventPayload::Create()));
  packet_in.Add(InputEvent::Create(2, IPCInputEventPayload::Create()));

  IPC::Message msg;
  IPC::ParamTraits<EventPacket>::Write(&msg, packet_in);

  EventPacket packet_out;
  PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ParamTraits<EventPacket>::Read(&msg, &iter, &packet_out));
}

TEST_F(InputParamTraitsTest, EventPacketIPCEvents) {
  EventPacket packet_in;
  packet_in.set_id(1);

  packet_in.Add(
      InputEvent::Create(1,
                         IPCInputEventPayload::Create(
                             scoped_ptr<IPC::Message>(new InputMsg_Undo(1)))));
  packet_in.Add(InputEvent::Create(
      1,
      IPCInputEventPayload::Create(
          scoped_ptr<IPC::Message>(new InputMsg_SetFocus(2, true)))));
  Verify(packet_in);
}

TEST_F(InputParamTraitsTest, EventPacketWebInputEvents) {
  EventPacket packet_in;
  packet_in.set_id(1);

  ui::LatencyInfo latency;

  int64 next_event_id = 1;
  WebKit::WebKeyboardEvent key_event;
  key_event.type = WebKit::WebInputEvent::RawKeyDown;
  key_event.nativeKeyCode = 5;
  packet_in.Add(InputEvent::Create(
      ++next_event_id, WebInputEventPayload::Create(key_event, latency, true)));

  WebKit::WebMouseWheelEvent wheel_event;
  wheel_event.type = WebKit::WebInputEvent::MouseWheel;
  wheel_event.deltaX = 10;
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_RWH_COMPONENT, 1, 1);
  packet_in.Add(InputEvent::Create(
      ++next_event_id,
      WebInputEventPayload::Create(wheel_event, latency, false)));

  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.x = 10;
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 2, 2);
  packet_in.Add(InputEvent::Create(
      ++next_event_id,
      WebInputEventPayload::Create(mouse_event, latency, false)));

  WebKit::WebGestureEvent gesture_event;
  gesture_event.type = WebKit::WebInputEvent::GestureScrollBegin;
  gesture_event.x = -1;
  packet_in.Add(InputEvent::Create(
      ++next_event_id,
      WebInputEventPayload::Create(gesture_event, latency, false)));

  WebKit::WebTouchEvent touch_event;
  touch_event.type = WebKit::WebInputEvent::TouchStart;
  touch_event.touchesLength = 1;
  touch_event.touches[0].radiusX = 1;
  packet_in.Add(InputEvent::Create(
      ++next_event_id,
      WebInputEventPayload::Create(touch_event, latency, false)));

  Verify(packet_in);
}

TEST_F(InputParamTraitsTest, EventPacketMixedEvents) {
  EventPacket packet_in;
  packet_in.set_id(1);
  int64 next_event_id = 1;

  // Add a mix of IPC and WebInputEvents.
  packet_in.Add(
      InputEvent::Create(++next_event_id,
                         IPCInputEventPayload::Create(
                             scoped_ptr<IPC::Message>(new InputMsg_Undo(1)))));

  ui::LatencyInfo latency;
  WebKit::WebKeyboardEvent key_event;
  key_event.type = WebKit::WebInputEvent::RawKeyDown;
  key_event.nativeKeyCode = 5;
  packet_in.Add(InputEvent::Create(
      ++next_event_id, WebInputEventPayload::Create(key_event, latency, true)));

  packet_in.Add(InputEvent::Create(
      ++next_event_id,
      IPCInputEventPayload::Create(
          scoped_ptr<IPC::Message>(new InputMsg_SetFocus(2, true)))));

  WebKit::WebMouseWheelEvent wheel_event;
  wheel_event.type = WebKit::WebInputEvent::MouseWheel;
  wheel_event.deltaX = 10;
  packet_in.Add(InputEvent::Create(
      ++next_event_id,
      WebInputEventPayload::Create(wheel_event, latency, false)));

  Verify(packet_in);
}

}  // namespace
}  // namespace content
