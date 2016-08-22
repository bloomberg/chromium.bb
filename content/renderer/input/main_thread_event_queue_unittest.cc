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
#include "content/renderer/render_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/scheduler/test/mock_renderer_scheduler.h"

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
        queue_(new MainThreadEventQueue(kTestRoutingID,
                                        this,
                                        main_task_runner_,
                                        &renderer_scheduler_)) {}

  void HandleEventOnMainThread(int routing_id,
                               const blink::WebInputEvent* event,
                               const ui::LatencyInfo& latency,
                               InputEventDispatchType type) override {
    EXPECT_EQ(kTestRoutingID, routing_id);
    handled_events_.push_back(ui::WebInputEventTraits::Clone(*event));
    queue_->EventHandled(event->type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }

  void SendInputEventAck(int routing_id,
                         blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) override {
    additional_acked_events_.push_back(touch_event_id);
  }

  void HandleEvent(WebInputEvent& event, InputEventAckState ack_result) {
    queue_->HandleEvent(ui::WebInputEventTraits::Clone(event),
                        ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING, ack_result);
  }

  WebInputEventQueue<EventWithDispatchType>& event_queue() {
    return queue_->events_;
  }

  void set_is_flinging(bool is_flinging) {
    queue_->set_is_flinging(is_flinging);
  }

  bool last_touch_start_forced_nonblocking_due_to_fling() {
    return queue_->last_touch_start_forced_nonblocking_due_to_fling_;
  }

  void set_enable_fling_passive_listener_flag(bool enable_flag) {
    queue_->enable_fling_passive_listener_flag_ = enable_flag;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  blink::scheduler::MockRendererScheduler renderer_scheduler_;
  scoped_refptr<MainThreadEventQueue> queue_;
  std::vector<ui::ScopedWebInputEvent> handled_events_;
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

  for (WebMouseWheelEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

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

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

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

  EXPECT_CALL(renderer_scheduler_, DidHandleInputEventOnMainThread(testing::_))
      .Times(2);
  // Ensure that coalescing takes place.
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(kEvents[2], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(kEvents[3], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

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

  HandleEvent(kWheelEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kTouchEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kWheelEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kTouchEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

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

TEST_F(MainThreadEventQueueTest, BlockingTouchesDuringFling) {
  SyntheticWebTouchEvent kEvents[1];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].touchStartOrFirstTouchMove = true;
  set_is_flinging(true);
  set_enable_fling_passive_listener_flag(true);

  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(0)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(0)->type);
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(0).get());
  kEvents[0].dispatchedDuringFling = true;
  kEvents[0].dispatchType = WebInputEvent::ListenersForcedNonBlockingDueToFling;
  EXPECT_EQ(kEvents[0], *last_touch_event);

  kEvents[0].MovePoint(0, 30, 30);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(1)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(1)->type);
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(1).get());
  kEvents[0].dispatchedDuringFling = true;
  kEvents[0].dispatchType = WebInputEvent::ListenersForcedNonBlockingDueToFling;
  EXPECT_EQ(kEvents[0], *last_touch_event);

  kEvents[0].MovePoint(0, 50, 50);
  kEvents[0].touchStartOrFirstTouchMove = false;
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(2)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(2)->type);
  EXPECT_TRUE(kEvents[0].dispatchedDuringFling);
  EXPECT_EQ(kEvents[0].dispatchType, WebInputEvent::Blocking);
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(2).get());
  EXPECT_EQ(kEvents[0], *last_touch_event);

  kEvents[0].ReleasePoint(0);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(3)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(3)->type);
  EXPECT_TRUE(kEvents[0].dispatchedDuringFling);
  EXPECT_EQ(kEvents[0].dispatchType, WebInputEvent::Blocking);
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(3).get());
  EXPECT_EQ(kEvents[0], *last_touch_event);
}

TEST_F(MainThreadEventQueueTest, BlockingTouchesOutsideFling) {
  SyntheticWebTouchEvent kEvents[1];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].touchStartOrFirstTouchMove = true;
  set_is_flinging(false);
  set_enable_fling_passive_listener_flag(false);

  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(0)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(0)->type);
  EXPECT_FALSE(kEvents[0].dispatchedDuringFling);
  EXPECT_EQ(kEvents[0].dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(0).get());
  EXPECT_EQ(kEvents[0], *last_touch_event);

  set_is_flinging(true);
  set_enable_fling_passive_listener_flag(false);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(1)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(1)->type);
  EXPECT_FALSE(kEvents[0].dispatchedDuringFling);
  EXPECT_EQ(kEvents[0].dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(1).get());
  kEvents[0].dispatchedDuringFling = true;
  EXPECT_EQ(kEvents[0], *last_touch_event);

  set_is_flinging(false);
  set_enable_fling_passive_listener_flag(true);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(2)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(2)->type);
  EXPECT_TRUE(kEvents[0].dispatchedDuringFling);
  EXPECT_EQ(kEvents[0].dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(2).get());
  kEvents[0].dispatchedDuringFling = false;
  EXPECT_EQ(kEvents[0], *last_touch_event);

  kEvents[0].MovePoint(0, 30, 30);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  main_task_runner_->RunUntilIdle();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_events_.size());
  EXPECT_EQ(kEvents[0].size, handled_events_.at(3)->size);
  EXPECT_EQ(kEvents[0].type, handled_events_.at(3)->type);
  EXPECT_FALSE(kEvents[0].dispatchedDuringFling);
  EXPECT_EQ(kEvents[0].dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(3).get());
  EXPECT_EQ(kEvents[0], *last_touch_event);
}

}  // namespace content