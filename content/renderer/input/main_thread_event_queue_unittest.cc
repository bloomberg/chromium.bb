// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <new>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace blink {
bool operator==(const WebMouseWheelEvent& lhs, const WebMouseWheelEvent& rhs) {
  return memcmp(&lhs, &rhs, lhs.size) == 0;
}

bool operator==(const WebTouchEvent& lhs, const WebTouchEvent& rhs) {
  return memcmp(&lhs, &rhs, lhs.size) == 0;
}
}  // namespace blink

namespace content {
namespace {

const int kTestRoutingID = 13;
}

class MainThreadEventQueueTest : public testing::Test,
                                 public MainThreadEventQueueClient {
 public:
  MainThreadEventQueueTest()
      : main_task_runner_(new base::TestSimpleTaskRunner()),
        queue_(
            new MainThreadEventQueue(kTestRoutingID, this, main_task_runner_)) {
  }

  void HandleEventOnMainThread(int routing_id,
                               const blink::WebInputEvent* event,
                               const ui::LatencyInfo& latency,
                               InputEventDispatchType type) override {
    EXPECT_EQ(kTestRoutingID, routing_id);
    handled_events_.push_back(WebInputEventTraits::Clone(*event));
    queue_->EventHandled(event->type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }

  void SendInputEventAck(int routing_id,
                         blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) override {
    additional_acked_events_.push_back(touch_event_id);
  }

  WebInputEventQueue<EventWithDispatchType>& event_queue() {
    return queue_->events_;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  scoped_refptr<MainThreadEventQueue> queue_;
  std::vector<ScopedWebInputEvent> handled_events_;
  std::vector<uint32_t> additional_acked_events_;
};

TEST_F(MainThreadEventQueueTest, NonBlockingWheel) {
  WebMouseWheelEvent kEvents[4] = {
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
  };

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  queue_->HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kEvents[1], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kEvents[2], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kEvents[3], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());

  {
    EXPECT_EQ(kEvents[0].size, handled_events_.at(0)->size);
    EXPECT_EQ(kEvents[0].type, handled_events_.at(0)->type);
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(handled_events_.at(0).get());
    EXPECT_EQ(WebInputEvent::DispatchType::ListenersNonBlockingPassive,
              last_wheel_event->dispatchType);
    WebMouseWheelEvent coalesced_event = kEvents[0];
    internal::Coalesce(kEvents[1], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }

  {
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(handled_events_.at(1).get());
    WebMouseWheelEvent coalesced_event = kEvents[2];
    internal::Coalesce(kEvents[3], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }
}

TEST_F(MainThreadEventQueueTest, NonBlockingTouch) {
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].modifiers = 1;
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);
  queue_->HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kEvents[1], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kEvents[2], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kEvents[3], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_events_.size());

  EXPECT_EQ(kEvents[0].size, handled_events_.at(0)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(0)->type);
  const WebTouchEvent* last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(0).get());
  kEvents[0].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  EXPECT_EQ(kEvents[0], *last_touch_event);

  EXPECT_EQ(kEvents[1].size, handled_events_.at(1)->size);
  EXPECT_EQ(kEvents[1].type, handled_events_.at(1)->type);
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(1).get());
  kEvents[1].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  EXPECT_EQ(kEvents[1], *last_touch_event);

  EXPECT_EQ(kEvents[2].size, handled_events_.at(1)->size);
  EXPECT_EQ(kEvents[2].type, handled_events_.at(2)->type);
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(2).get());
  WebTouchEvent coalesced_event = kEvents[2];
  internal::Coalesce(kEvents[3], &coalesced_event);
  coalesced_event.dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  EXPECT_EQ(coalesced_event, *last_touch_event);
}

TEST_F(MainThreadEventQueueTest, BlockingTouch) {
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);
  // Ensure that coalescing takes place.
  queue_->HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kEvents[1], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  queue_->HandleEvent(&kEvents[2], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  queue_->HandleEvent(&kEvents[3], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, additional_acked_events_.size());
  EXPECT_EQ(kEvents[2].uniqueTouchEventId, additional_acked_events_.at(0));
  EXPECT_EQ(kEvents[3].uniqueTouchEventId, additional_acked_events_.at(1));
}

TEST_F(MainThreadEventQueueTest, InterleavedEvents) {
  WebMouseWheelEvent kWheelEvents[2] = {
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 0, false),
  };
  SyntheticWebTouchEvent kTouchEvents[2];
  kTouchEvents[0].PressPoint(10, 10);
  kTouchEvents[0].MovePoint(0, 20, 20);
  kTouchEvents[1].PressPoint(10, 10);
  kTouchEvents[1].MovePoint(0, 30, 30);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  queue_->HandleEvent(&kWheelEvents[0], ui::LatencyInfo(),
                      DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kTouchEvents[0], ui::LatencyInfo(),
                      DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kWheelEvents[1], ui::LatencyInfo(),
                      DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->HandleEvent(&kTouchEvents[1], ui::LatencyInfo(),
                      DISPATCH_TYPE_BLOCKING,
                      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());
  {
    EXPECT_EQ(kWheelEvents[0].size, handled_events_.at(0)->size);
    EXPECT_EQ(kWheelEvents[0].type, handled_events_.at(0)->type);
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(handled_events_.at(0).get());
    EXPECT_EQ(WebInputEvent::DispatchType::ListenersNonBlockingPassive,
              last_wheel_event->dispatchType);
    WebMouseWheelEvent coalesced_event = kWheelEvents[0];
    internal::Coalesce(kWheelEvents[1], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }
  {
    EXPECT_EQ(kTouchEvents[0].size, handled_events_.at(1)->size);
    EXPECT_EQ(kTouchEvents[0].type, handled_events_.at(1)->type);
    const WebTouchEvent* last_touch_event =
        static_cast<const WebTouchEvent*>(handled_events_.at(1).get());
    WebTouchEvent coalesced_event = kTouchEvents[0];
    internal::Coalesce(kTouchEvents[1], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_touch_event);
  }
}

}  // namespace content
