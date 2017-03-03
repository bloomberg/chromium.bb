// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <new>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
  return memcmp(&lhs, &rhs, lhs.size()) == 0;
}

bool operator==(const WebTouchEvent& lhs, const WebTouchEvent& rhs) {
  return memcmp(&lhs, &rhs, lhs.size()) == 0;
}
}  // namespace blink

namespace content {
namespace {

const unsigned kRafAlignedEnabledTouch = 1;
const unsigned kRafAlignedEnabledMouse = 1 << 1;

// Simulate a 16ms frame signal.
const base::TimeDelta kFrameInterval = base::TimeDelta::FromMilliseconds(16);

const int kTestRoutingID = 13;
const char* kCoalescedCountHistogram =
    "Event.MainThreadEventQueue.CoalescedCount";

}  // namespace

class MainThreadEventQueueTest : public testing::TestWithParam<unsigned>,
                                 public MainThreadEventQueueClient {
 public:
  MainThreadEventQueueTest()
      : main_task_runner_(new base::TestSimpleTaskRunner()),
        raf_aligned_input_setting_(GetParam()),
        needs_main_frame_(false) {
    std::vector<std::string> features;
    if (raf_aligned_input_setting_ & kRafAlignedEnabledTouch)
      features.push_back(features::kRafAlignedTouchInputEvents.name);
    if (raf_aligned_input_setting_ & kRafAlignedEnabledMouse)
      features.push_back(features::kRafAlignedMouseInputEvents.name);

    feature_list_.InitFromCommandLine(base::JoinString(features, ","),
                                      std::string());
  }

  void SetUp() override {
    queue_ = new MainThreadEventQueue(kTestRoutingID, this, main_task_runner_,
                                      &renderer_scheduler_);
  }

  void HandleEventOnMainThread(int routing_id,
                               const blink::WebCoalescedInputEvent* event,
                               const ui::LatencyInfo& latency,
                               InputEventDispatchType type) override {
    EXPECT_EQ(kTestRoutingID, routing_id);
    handled_events_.push_back(blink::WebCoalescedInputEvent(
        event->event(), event->getCoalescedEventsPointers()));

    queue_->EventHandled(event->event().type(),
                         blink::WebInputEventResult::HandledApplication,
                         INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }

  void SendInputEventAck(int routing_id,
                         blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) override {
    additional_acked_events_.push_back(touch_event_id);
  }

  bool HandleEvent(WebInputEvent& event, InputEventAckState ack_result) {
    return queue_->HandleEvent(ui::WebInputEventTraits::Clone(event),
                               ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                               ack_result);
  }

  void NeedsMainFrame(int routing_id) override { needs_main_frame_ = true; }

  WebInputEventQueue<EventWithDispatchType>& event_queue() {
    return queue_->shared_state_.events_;
  }

  bool last_touch_start_forced_nonblocking_due_to_fling() {
    return queue_->last_touch_start_forced_nonblocking_due_to_fling_;
  }

  void set_enable_fling_passive_listener_flag(bool enable_flag) {
    queue_->enable_fling_passive_listener_flag_ = enable_flag;
  }

  void RunPendingTasksWithSimulatedRaf() {
    while (needs_main_frame_ || main_task_runner_->HasPendingTask()) {
      main_task_runner_->RunUntilIdle();
      needs_main_frame_ = false;
      frame_time_ += kFrameInterval;
      queue_->DispatchRafAlignedInput(frame_time_);
    }
  }

  void RunSimulatedRafOnce() {
    if (needs_main_frame_) {
      needs_main_frame_ = false;
      frame_time_ += kFrameInterval;
      queue_->DispatchRafAlignedInput(frame_time_);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  blink::scheduler::MockRendererScheduler renderer_scheduler_;
  scoped_refptr<MainThreadEventQueue> queue_;
  std::vector<blink::WebCoalescedInputEvent> handled_events_;

  std::vector<uint32_t> additional_acked_events_;
  int raf_aligned_input_setting_;
  bool needs_main_frame_;
  base::TimeTicks frame_time_;
};

TEST_P(MainThreadEventQueueTest, NonBlockingWheel) {
  base::HistogramTester histogram_tester;

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
  EXPECT_EQ((raf_aligned_input_setting_ & kRafAlignedEnabledMouse) == 0,
            main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());
  for (const auto& coalesced_event : handled_events_) {
    EXPECT_EQ(2u, coalesced_event.coalescedEventSize());
  }

  {
    EXPECT_EQ(kEvents[0].size(), handled_events_.at(0).event().size());
    EXPECT_EQ(kEvents[0].type(), handled_events_.at(0).event().type());
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_events_.at(0).eventPointer());
    EXPECT_EQ(WebInputEvent::DispatchType::ListenersNonBlockingPassive,
              last_wheel_event->dispatchType);
    WebMouseWheelEvent coalesced_event = kEvents[0];
    ui::Coalesce(kEvents[1], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }

  {
    WebMouseWheelEvent coalesced_event = kEvents[0];
    std::vector<const WebInputEvent*> coalesced_events =
        handled_events_[0].getCoalescedEventsPointers();
    const WebMouseWheelEvent* coalesced_wheel_event0 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[0]);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event0);

    coalesced_event = kEvents[1];
    const WebMouseWheelEvent* coalesced_wheel_event1 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[1]);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event1);
  }

  {
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_events_.at(1).eventPointer());
    WebMouseWheelEvent coalesced_event = kEvents[2];
    ui::Coalesce(kEvents[3], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }

  {
    WebMouseWheelEvent coalesced_event = kEvents[2];
    std::vector<const WebInputEvent*> coalesced_events =
        handled_events_[1].getCoalescedEventsPointers();
    const WebMouseWheelEvent* coalesced_wheel_event0 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[0]);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event0);

    coalesced_event = kEvents[3];
    const WebMouseWheelEvent* coalesced_wheel_event1 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[1]);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event1);
  }

  histogram_tester.ExpectUniqueSample(kCoalescedCountHistogram, 1, 2);
}

TEST_P(MainThreadEventQueueTest, NonBlockingTouch) {
  base::HistogramTester histogram_tester;

  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].setModifiers(1);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_events_.size());

  EXPECT_EQ(kEvents[0].size(), handled_events_.at(0).event().size());
  EXPECT_EQ(kEvents[0].type(), handled_events_.at(0).event().type());
  const WebTouchEvent* last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(0).eventPointer());
  kEvents[0].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  EXPECT_EQ(kEvents[0], *last_touch_event);

  {
    EXPECT_EQ(1u, handled_events_[0].coalescedEventSize());
    const WebTouchEvent* coalesced_touch_event =
        static_cast<const WebTouchEvent*>(
            handled_events_[0].getCoalescedEventsPointers()[0]);
    EXPECT_EQ(kEvents[0], *coalesced_touch_event);
  }

  EXPECT_EQ(kEvents[1].size(), handled_events_.at(1).event().size());
  EXPECT_EQ(kEvents[1].type(), handled_events_.at(1).event().type());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(1).eventPointer());
  kEvents[1].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  EXPECT_EQ(kEvents[1], *last_touch_event);

  {
    EXPECT_EQ(1u, handled_events_[1].coalescedEventSize());
    const WebTouchEvent* coalesced_touch_event =
        static_cast<const WebTouchEvent*>(
            handled_events_[1].getCoalescedEventsPointers()[0]);
    EXPECT_EQ(kEvents[1], *coalesced_touch_event);
  }

  EXPECT_EQ(kEvents[2].size(), handled_events_.at(1).event().size());
  EXPECT_EQ(kEvents[2].type(), handled_events_.at(2).event().type());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(2).eventPointer());
  WebTouchEvent coalesced_event = kEvents[2];
  ui::Coalesce(kEvents[3], &coalesced_event);
  coalesced_event.dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  EXPECT_EQ(coalesced_event, *last_touch_event);

  {
    EXPECT_EQ(2u, handled_events_[2].coalescedEventSize());
    WebTouchEvent coalesced_event = kEvents[2];
    std::vector<const WebInputEvent*> coalesced_events =
        handled_events_[2].getCoalescedEventsPointers();
    const WebTouchEvent* coalesced_touch_event0 =
        static_cast<const WebTouchEvent*>(coalesced_events[0]);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_touch_event0);

    coalesced_event = kEvents[3];
    const WebTouchEvent* coalesced_touch_event1 =
        static_cast<const WebTouchEvent*>(coalesced_events[1]);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_touch_event1);
  }

  histogram_tester.ExpectBucketCount(kCoalescedCountHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(kCoalescedCountHistogram, 1, 1);
}

TEST_P(MainThreadEventQueueTest, BlockingTouch) {
  base::HistogramTester histogram_tester;
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(2);
  // Ensure that coalescing takes place.
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(kEvents[2], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(kEvents[3], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();

  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, additional_acked_events_.size());
  EXPECT_EQ(kEvents[1].uniqueTouchEventId, additional_acked_events_.at(0));
  EXPECT_EQ(kEvents[2].uniqueTouchEventId, additional_acked_events_.at(1));

  const WebTouchEvent* last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(1).eventPointer());
  EXPECT_EQ(kEvents[3].uniqueTouchEventId,
            last_touch_event->uniqueTouchEventId);

  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[2], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[3], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  histogram_tester.ExpectUniqueSample(kCoalescedCountHistogram, 2, 2);
}

TEST_P(MainThreadEventQueueTest, InterleavedEvents) {
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
  EXPECT_EQ(raf_aligned_input_setting_ !=
                (kRafAlignedEnabledMouse | kRafAlignedEnabledTouch),
            main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());
  {
    EXPECT_EQ(kWheelEvents[0].size(), handled_events_.at(0).event().size());
    EXPECT_EQ(kWheelEvents[0].type(), handled_events_.at(0).event().type());
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_events_.at(0).eventPointer());
    EXPECT_EQ(WebInputEvent::DispatchType::ListenersNonBlockingPassive,
              last_wheel_event->dispatchType);
    WebMouseWheelEvent coalesced_event = kWheelEvents[0];
    ui::Coalesce(kWheelEvents[1], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }
  {
    EXPECT_EQ(kTouchEvents[0].size(), handled_events_.at(1).event().size());
    EXPECT_EQ(kTouchEvents[0].type(), handled_events_.at(1).event().type());
    const WebTouchEvent* last_touch_event =
        static_cast<const WebTouchEvent*>(handled_events_.at(1).eventPointer());
    WebTouchEvent coalesced_event = kTouchEvents[0];
    ui::Coalesce(kTouchEvents[1], &coalesced_event);
    coalesced_event.dispatchType =
        WebInputEvent::DispatchType::ListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_touch_event);
  }
}

TEST_P(MainThreadEventQueueTest, RafAlignedMouseInput) {
  // Don't run the test when we aren't supporting rAF aligned input.
  if ((raf_aligned_input_setting_ & kRafAlignedEnabledMouse) == 0)
    return;

  WebMouseEvent mouseDown =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::MouseDown, 10, 10, 0);

  WebMouseEvent mouseMove =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::MouseMove, 10, 10, 0);

  WebMouseEvent mouseUp =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::MouseUp, 10, 10, 0);

  WebMouseWheelEvent wheelEvents[2] = {
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 0, false),
  };

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  // Simulate enqueing a discrete event, followed by continuous events and
  // then a discrete event. The last discrete event should flush the
  // continuous events so the aren't aligned to rAF and are processed
  // immediately.
  HandleEvent(mouseDown, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(mouseMove, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(wheelEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(wheelEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(mouseUp, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(4u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();

  // Simulate the rAF running before the PostTask occurs. The first rAF
  // shouldn't do anything.
  HandleEvent(mouseDown, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(wheelEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  RunSimulatedRafOnce();
  EXPECT_FALSE(needs_main_frame_);
  EXPECT_EQ(2u, event_queue().size());
  main_task_runner_->RunUntilIdle();
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
}

TEST_P(MainThreadEventQueueTest, RafAlignedTouchInput) {
  // Don't run the test when we aren't supporting rAF aligned input.
  if ((raf_aligned_input_setting_ & kRafAlignedEnabledTouch) == 0)
    return;

  SyntheticWebTouchEvent kEvents[3];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 50, 50);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].ReleasePoint(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  // Simulate enqueing a discrete event, followed by continuous events and
  // then a discrete event. The last discrete event should flush the
  // continuous events so the aren't aligned to rAF and are processed
  // immediately.
  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();

  // Simulate the rAF running before the PostTask occurs. The first rAF
  // shouldn't do anything.
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  RunSimulatedRafOnce();
  EXPECT_FALSE(needs_main_frame_);
  EXPECT_EQ(2u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());

  // Simulate the touch move being discrete
  kEvents[0].touchStartOrFirstTouchMove = true;
  kEvents[1].touchStartOrFirstTouchMove = true;

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
}

TEST_P(MainThreadEventQueueTest, RafAlignedTouchInputCoalescedMoves) {
  // Don't run the test when we aren't supporting rAF aligned input.
  if ((raf_aligned_input_setting_ & kRafAlignedEnabledTouch) == 0)
    return;

  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 50, 50);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[0].dispatchType = WebInputEvent::EventNonBlocking;

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  // Send a discrete input event and then a continuous
  // (ack required)  event. The events should coalesce together
  // and a post task should be on the queue at the end.
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, additional_acked_events_.size());
  additional_acked_events_.clear();

  // Send a non-cancelable ack required event, and then a non-ack
  // required event they should be coalesced together.
  EXPECT_TRUE(HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED));
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_TRUE(HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING));
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(0u, additional_acked_events_.size());

  // Send a non-ack required event, and then a non-cancelable ack
  // required event they should be coalesced together.
  EXPECT_TRUE(HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING));
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_TRUE(HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED));
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(0u, additional_acked_events_.size());
}

TEST_P(MainThreadEventQueueTest, RafAlignedTouchInputThrottlingMoves) {
  // Don't run the test when we aren't supporting rAF aligned input.
  if ((raf_aligned_input_setting_ & kRafAlignedEnabledTouch) == 0)
    return;

  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 50, 50);
  kEvents[0].dispatchType = WebInputEvent::EventNonBlocking;
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[1].dispatchType = WebInputEvent::EventNonBlocking;

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  // Send a non-cancelable touch move and then send it another one. The
  // second one shouldn't go out with the next rAF call and should be throttled.
  EXPECT_TRUE(HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED));
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED));
  EXPECT_TRUE(HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED));
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);

  // Event should still be in queue after handling a single rAF call.
  RunSimulatedRafOnce();
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);

  // And should eventually flush.
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(0u, additional_acked_events_.size());
}

TEST_P(MainThreadEventQueueTest, BlockingTouchesDuringFling) {
  SyntheticWebTouchEvent kEvents;
  kEvents.PressPoint(10, 10);
  kEvents.touchStartOrFirstTouchMove = true;
  set_enable_fling_passive_listener_flag(true);

  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(0).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(0).event().type());
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(0).eventPointer());
  kEvents.dispatchType = WebInputEvent::ListenersForcedNonBlockingDueToFling;
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.MovePoint(0, 30, 30);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  EXPECT_EQ((raf_aligned_input_setting_ & kRafAlignedEnabledTouch) == 0,
            main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(1).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(1).event().type());
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(1).eventPointer());
  kEvents.dispatchType = WebInputEvent::ListenersForcedNonBlockingDueToFling;
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.MovePoint(0, 50, 50);
  kEvents.touchStartOrFirstTouchMove = false;
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(2).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(2).event().type());
  EXPECT_EQ(kEvents.dispatchType, WebInputEvent::Blocking);
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(2).eventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.ReleasePoint(0);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(3).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(3).event().type());
  EXPECT_EQ(kEvents.dispatchType, WebInputEvent::Blocking);
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(3).eventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);
}

TEST_P(MainThreadEventQueueTest, BlockingTouchesOutsideFling) {
  SyntheticWebTouchEvent kEvents;
  kEvents.PressPoint(10, 10);
  kEvents.touchStartOrFirstTouchMove = true;
  set_enable_fling_passive_listener_flag(false);

  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(0).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(0).event().type());
  EXPECT_EQ(kEvents.dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(0).eventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  set_enable_fling_passive_listener_flag(false);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(1).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(1).event().type());
  EXPECT_EQ(kEvents.dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(1).eventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  set_enable_fling_passive_listener_flag(true);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(2).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(2).event().type());
  EXPECT_EQ(kEvents.dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(2).eventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.MovePoint(0, 30, 30);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_events_.size());
  EXPECT_EQ(kEvents.size(), handled_events_.at(3).event().size());
  EXPECT_EQ(kEvents.type(), handled_events_.at(3).event().type());
  EXPECT_EQ(kEvents.dispatchType, WebInputEvent::Blocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event =
      static_cast<const WebTouchEvent*>(handled_events_.at(3).eventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);
}

// The boolean parameterized test varies whether rAF aligned input
// is enabled or not.
INSTANTIATE_TEST_CASE_P(
    MainThreadEventQueueTests,
    MainThreadEventQueueTest,
    testing::Range(0u,
                   (kRafAlignedEnabledTouch | kRafAlignedEnabledMouse) + 1));

class DummyMainThreadEventQueueClient : public MainThreadEventQueueClient {
  void HandleEventOnMainThread(int routing_id,
                               const blink::WebCoalescedInputEvent* event,
                               const ui::LatencyInfo& latency,
                               InputEventDispatchType dispatch_type) override {}

  void SendInputEventAck(int routing_id,
                         blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) override {}

  void NeedsMainFrame(int routing_id) override {}
};

class MainThreadEventQueueInitializationTest
    : public testing::Test {
 public:
  MainThreadEventQueueInitializationTest()
      : field_trial_list_(new base::FieldTrialList(nullptr)) {}

  base::TimeDelta main_thread_responsiveness_threshold() {
    return queue_->main_thread_responsiveness_threshold_;
  }

  bool enable_non_blocking_due_to_main_thread_responsiveness_flag() {
    return queue_->enable_non_blocking_due_to_main_thread_responsiveness_flag_;
  }

 protected:
  scoped_refptr<MainThreadEventQueue> queue_;
  base::test::ScopedFeatureList feature_list_;
  blink::scheduler::MockRendererScheduler renderer_scheduler_;
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  DummyMainThreadEventQueueClient dummy_main_thread_event_queue_client_;
};

TEST_F(MainThreadEventQueueInitializationTest,
       MainThreadResponsivenessThresholdEnabled) {
  feature_list_.InitFromCommandLine(
      features::kMainThreadBusyScrollIntervention.name, "");

  base::FieldTrialList::CreateFieldTrial(
      "MainThreadResponsivenessScrollIntervention", "Enabled123");
  queue_ = new MainThreadEventQueue(kTestRoutingID,
                                    &dummy_main_thread_event_queue_client_,
                                    main_task_runner_, &renderer_scheduler_);
  EXPECT_TRUE(enable_non_blocking_due_to_main_thread_responsiveness_flag());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(123),
            main_thread_responsiveness_threshold());
}

TEST_F(MainThreadEventQueueInitializationTest,
       MainThreadResponsivenessThresholdDisabled) {
  base::FieldTrialList::CreateFieldTrial(
      "MainThreadResponsivenessScrollIntervention", "Control");
  queue_ = new MainThreadEventQueue(kTestRoutingID,
                                    &dummy_main_thread_event_queue_client_,
                                    main_task_runner_, &renderer_scheduler_);
  EXPECT_FALSE(enable_non_blocking_due_to_main_thread_responsiveness_flag());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            main_thread_responsiveness_threshold());
}

}  // namespace content
