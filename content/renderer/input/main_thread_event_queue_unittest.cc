// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <new>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/mock_renderer_scheduler.h"

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

// Simulate a 16ms frame signal.
const base::TimeDelta kFrameInterval = base::TimeDelta::FromMilliseconds(16);

}  // namespace

class HandledTask {
 public:
  virtual ~HandledTask() {}

  virtual blink::WebCoalescedInputEvent* taskAsEvent() = 0;
  virtual unsigned taskAsClosure() const = 0;
};

class HandledEvent : public HandledTask {
 public:
  explicit HandledEvent(const blink::WebCoalescedInputEvent& event)
      : event_(event.Event(), event.GetCoalescedEventsPointers()) {}
  ~HandledEvent() override {}

  blink::WebCoalescedInputEvent* taskAsEvent() override { return &event_; }
  unsigned taskAsClosure() const override {
    NOTREACHED();
    return 0;
  }

 private:
  blink::WebCoalescedInputEvent event_;
};

class HandledClosure : public HandledTask {
 public:
  explicit HandledClosure(unsigned closure_id) : closure_id_(closure_id) {}
  ~HandledClosure() override {}

  blink::WebCoalescedInputEvent* taskAsEvent() override {
    NOTREACHED();
    return nullptr;
  }
  unsigned taskAsClosure() const override { return closure_id_; }

 private:
  unsigned closure_id_;
};

enum class CallbackReceivedState {
  kPending,
  kCalledWhileHandlingEvent,
  kCalledAfterHandleEvent,
};

class ReceivedCallback {
 public:
  ReceivedCallback()
      : ReceivedCallback(CallbackReceivedState::kPending, false) {}

  ReceivedCallback(CallbackReceivedState state, bool coalesced_latency)
      : state_(state), coalesced_latency_(coalesced_latency) {}
  bool operator==(const ReceivedCallback& other) const {
    return state_ == other.state_ &&
           coalesced_latency_ == other.coalesced_latency_;
  }

 private:
  CallbackReceivedState state_;
  bool coalesced_latency_;
};

class HandledEventCallbackTracker {
 public:
  HandledEventCallbackTracker()
      : handling_event_(false), weak_ptr_factory_(this) {
    weak_this_ = weak_ptr_factory_.GetWeakPtr();
  }

  HandledEventCallback GetCallback() {
    callbacks_received_.push_back(ReceivedCallback());
    HandledEventCallback callback =
        base::BindOnce(&HandledEventCallbackTracker::DidHandleEvent, weak_this_,
                       callbacks_received_.size() - 1);
    return callback;
  }

  void DidHandleEvent(size_t index,
                      InputEventAckState ack_result,
                      const ui::LatencyInfo& latency,
                      std::unique_ptr<ui::DidOverscrollParams> params,
                      base::Optional<cc::TouchAction> touch_action) {
    callbacks_received_[index] = ReceivedCallback(
        handling_event_ ? CallbackReceivedState::kCalledWhileHandlingEvent
                        : CallbackReceivedState::kCalledAfterHandleEvent,
        latency.coalesced());
  }

  const std::vector<ReceivedCallback>& GetReceivedCallbacks() const {
    return callbacks_received_;
  }

  bool handling_event_;

 private:
  std::vector<ReceivedCallback> callbacks_received_;
  std::vector<ui::LatencyInfo> latency_info_received_;
  base::WeakPtr<HandledEventCallbackTracker> weak_this_;
  base::WeakPtrFactory<HandledEventCallbackTracker> weak_ptr_factory_;
};

class MainThreadEventQueueTest : public testing::Test,
                                 public MainThreadEventQueueClient {
 public:
  MainThreadEventQueueTest()
      : main_task_runner_(new base::TestSimpleTaskRunner()),
        needs_main_frame_(false),
        closure_count_(0) {
    handler_callback_ = std::make_unique<HandledEventCallbackTracker>();
  }

  void SetUp() override {
    queue_ = new MainThreadEventQueue(this, main_task_runner_,
                                      &renderer_scheduler_, true);
    queue_->set_use_raf_fallback_timer(false);
  }

  void HandleEvent(WebInputEvent& event, InputEventAckState ack_result) {
    base::AutoReset<bool> in_handle_event(&handler_callback_->handling_event_,
                                          true);
    queue_->HandleEvent(ui::WebInputEventTraits::Clone(event),
                        ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING, ack_result,
                        handler_callback_->GetCallback());
  }

  void RunClosure(unsigned closure_id) {
    std::unique_ptr<HandledTask> closure(new HandledClosure(closure_id));
    handled_tasks_.push_back(std::move(closure));
  }

  void QueueClosure() {
    unsigned closure_id = ++closure_count_;
    queue_->QueueClosure(base::BindOnce(&MainThreadEventQueueTest::RunClosure,
                                        base::Unretained(this), closure_id));
  }

  MainThreadEventQueueTaskList& event_queue() {
    return queue_->shared_state_.events_;
  }

  bool needs_low_latency_until_pointer_up() {
    return queue_->needs_low_latency_until_pointer_up_;
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

  void HandleInputEvent(const blink::WebCoalescedInputEvent& event,
                        const ui::LatencyInfo& latency,
                        HandledEventCallback callback) override {
    std::unique_ptr<HandledTask> handled_event(new HandledEvent(event));
    handled_tasks_.push_back(std::move(handled_event));
    std::move(callback).Run(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, latency,
                            nullptr, base::nullopt);
  }

  void SetNeedsMainFrame() override { needs_main_frame_ = true; }

  std::vector<ReceivedCallback> GetAndResetCallbackResults() {
    std::unique_ptr<HandledEventCallbackTracker> callback =
        std::make_unique<HandledEventCallbackTracker>();
    handler_callback_.swap(callback);
    return callback->GetReceivedCallbacks();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  blink::scheduler::MockRendererScheduler renderer_scheduler_;
  scoped_refptr<MainThreadEventQueue> queue_;
  std::vector<std::unique_ptr<HandledTask>> handled_tasks_;
  std::unique_ptr<HandledEventCallbackTracker> handler_callback_;

  int raf_aligned_input_setting_;
  bool needs_main_frame_;
  base::TimeTicks frame_time_;
  unsigned closure_count_;
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

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(2);

  for (WebMouseWheelEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  for (const auto& task : handled_tasks_) {
    EXPECT_EQ(2u, task->taskAsEvent()->CoalescedEventSize());
  }

  {
    EXPECT_EQ(kEvents[0].size(),
              handled_tasks_.at(0)->taskAsEvent()->Event().size());
    EXPECT_EQ(kEvents[0].GetType(),
              handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_tasks_.at(0)->taskAsEvent()->EventPointer());
    EXPECT_EQ(WebInputEvent::DispatchType::kListenersNonBlockingPassive,
              last_wheel_event->dispatch_type);
    WebMouseWheelEvent coalesced_event = kEvents[0];
    ui::Coalesce(kEvents[1], &coalesced_event);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }

  {
    WebMouseWheelEvent coalesced_event = kEvents[0];
    std::vector<const WebInputEvent*> coalesced_events =
        handled_tasks_[0]->taskAsEvent()->GetCoalescedEventsPointers();
    const WebMouseWheelEvent* coalesced_wheel_event0 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[0]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event0);

    coalesced_event = kEvents[1];
    const WebMouseWheelEvent* coalesced_wheel_event1 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[1]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event1);
  }

  {
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_tasks_.at(1)->taskAsEvent()->EventPointer());
    WebMouseWheelEvent coalesced_event = kEvents[2];
    ui::Coalesce(kEvents[3], &coalesced_event);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }

  {
    WebMouseWheelEvent coalesced_event = kEvents[2];
    std::vector<const WebInputEvent*> coalesced_events =
        handled_tasks_[1]->taskAsEvent()->GetCoalescedEventsPointers();
    const WebMouseWheelEvent* coalesced_wheel_event0 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[0]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event0);

    coalesced_event = kEvents[3];
    const WebMouseWheelEvent* coalesced_wheel_event1 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[1]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_wheel_event1);
  }
}

TEST_F(MainThreadEventQueueTest, NonBlockingTouch) {
  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(3);

  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].SetModifiers(1);
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
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_tasks_.size());

  EXPECT_EQ(kEvents[0].size(),
            handled_tasks_.at(0)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents[0].GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(0)->taskAsEvent()->EventPointer());
  kEvents[0].dispatch_type =
      WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  EXPECT_EQ(kEvents[0], *last_touch_event);

  {
    EXPECT_EQ(1u, handled_tasks_[0]->taskAsEvent()->CoalescedEventSize());
    const WebTouchEvent* coalesced_touch_event =
        static_cast<const WebTouchEvent*>(
            handled_tasks_[0]->taskAsEvent()->GetCoalescedEventsPointers()[0]);
    EXPECT_EQ(kEvents[0], *coalesced_touch_event);
  }

  EXPECT_EQ(kEvents[1].size(),
            handled_tasks_.at(1)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents[1].GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(1)->taskAsEvent()->EventPointer());
  kEvents[1].dispatch_type =
      WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  EXPECT_EQ(kEvents[1], *last_touch_event);

  {
    EXPECT_EQ(1u, handled_tasks_[1]->taskAsEvent()->CoalescedEventSize());
    const WebTouchEvent* coalesced_touch_event =
        static_cast<const WebTouchEvent*>(
            handled_tasks_[1]->taskAsEvent()->GetCoalescedEventsPointers()[0]);
    EXPECT_EQ(kEvents[1], *coalesced_touch_event);
  }

  EXPECT_EQ(kEvents[2].size(),
            handled_tasks_.at(1)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents[2].GetType(),
            handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(2)->taskAsEvent()->EventPointer());
  WebTouchEvent coalesced_event = kEvents[2];
  ui::Coalesce(kEvents[3], &coalesced_event);
  coalesced_event.dispatch_type =
      WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  EXPECT_EQ(coalesced_event, *last_touch_event);

  {
    EXPECT_EQ(2u, handled_tasks_[2]->taskAsEvent()->CoalescedEventSize());
    WebTouchEvent coalesced_event = kEvents[2];
    std::vector<const WebInputEvent*> coalesced_events =
        handled_tasks_[2]->taskAsEvent()->GetCoalescedEventsPointers();
    const WebTouchEvent* coalesced_touch_event0 =
        static_cast<const WebTouchEvent*>(coalesced_events[0]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_touch_event0);

    coalesced_event = kEvents[3];
    const WebTouchEvent* coalesced_touch_event1 =
        static_cast<const WebTouchEvent*>(coalesced_events[1]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *coalesced_touch_event1);
  }
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

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(5);
  {
    // Ensure that coalescing takes place.
    HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
    HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    HandleEvent(kEvents[2], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    HandleEvent(kEvents[3], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

    EXPECT_EQ(2u, event_queue().size());
    EXPECT_TRUE(main_task_runner_->HasPendingTask());
    RunPendingTasksWithSimulatedRaf();

    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledWhileHandlingEvent,
                             false),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true)));
    EXPECT_EQ(0u, event_queue().size());

    const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
        handled_tasks_.at(1)->taskAsEvent()->EventPointer());
    EXPECT_EQ(kEvents[1].unique_touch_event_id,
              last_touch_event->unique_touch_event_id);
  }

  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[2], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[3], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
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

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(2);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  HandleEvent(kWheelEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kTouchEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kWheelEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kTouchEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  {
    EXPECT_EQ(kWheelEvents[0].size(),
              handled_tasks_.at(0)->taskAsEvent()->Event().size());
    EXPECT_EQ(kWheelEvents[0].GetType(),
              handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_tasks_.at(0)->taskAsEvent()->EventPointer());
    EXPECT_EQ(WebInputEvent::DispatchType::kListenersNonBlockingPassive,
              last_wheel_event->dispatch_type);
    WebMouseWheelEvent coalesced_event = kWheelEvents[0];
    ui::Coalesce(kWheelEvents[1], &coalesced_event);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_wheel_event);
  }
  {
    EXPECT_EQ(kTouchEvents[0].size(),
              handled_tasks_.at(1)->taskAsEvent()->Event().size());
    EXPECT_EQ(kTouchEvents[0].GetType(),
              handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
    const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
        handled_tasks_.at(1)->taskAsEvent()->EventPointer());
    WebTouchEvent coalesced_event = kTouchEvents[0];
    ui::Coalesce(kTouchEvents[1], &coalesced_event);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(coalesced_event, *last_touch_event);
  }
}

TEST_F(MainThreadEventQueueTest, RafAlignedMouseInput) {
  WebMouseEvent mouseDown = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseDown, 10, 10, 0);

  WebMouseEvent mouseMove = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);

  WebMouseEvent mouseUp =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseUp, 10, 10, 0);

  WebMouseWheelEvent wheelEvents[3] = {
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 1, false),
  };

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(11);

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
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));

  // Simulate the rAF running before the PostTask occurs. The rAF
  // will consume everything.
  HandleEvent(mouseDown, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(wheelEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  RunSimulatedRafOnce();
  EXPECT_FALSE(needs_main_frame_);
  EXPECT_EQ(0u, event_queue().size());
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));

  // Simulate event consumption but no rAF signal. The mouse wheel events
  // should still be in the queue.
  handled_tasks_.clear();
  HandleEvent(mouseDown, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(wheelEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(mouseUp, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(wheelEvents[2], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(wheelEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(5u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_EQ(2u, event_queue().size());
  RunSimulatedRafOnce();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_EQ(wheelEvents[2].GetModifiers(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetModifiers());
  EXPECT_EQ(wheelEvents[0].GetModifiers(),
            handled_tasks_.at(4)->taskAsEvent()->Event().GetModifiers());
}

TEST_F(MainThreadEventQueueTest, RafAlignedTouchInput) {
  SyntheticWebTouchEvent kEvents[3];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 50, 50);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].ReleasePoint(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(10);

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
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));

  // Simulate the rAF running before the PostTask occurs. The rAF
  // will consume everything.
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  RunSimulatedRafOnce();
  EXPECT_FALSE(needs_main_frame_);
  EXPECT_EQ(0u, event_queue().size());
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));

  // Simulate event consumption but no rAF signal. The touch events
  // should still be in the queue.
  handled_tasks_.clear();
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_EQ(1u, event_queue().size());
  RunSimulatedRafOnce();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));

  // Simulate the touch move being discrete
  kEvents[0].touch_start_or_first_touch_move = true;
  kEvents[1].touch_start_or_first_touch_move = true;

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
}

TEST_F(MainThreadEventQueueTest, RafAlignedTouchInputCoalescedMoves) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 50, 50);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[0].dispatch_type = WebInputEvent::kEventNonBlocking;

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(4);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  {
    // Send a non-blocking input event and then blocking  event.
    // The events should coalesce together.
    HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(1u, event_queue().size());
    EXPECT_FALSE(main_task_runner_->HasPendingTask());
    EXPECT_TRUE(needs_main_frame_);
    HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(1u, event_queue().size());
    EXPECT_FALSE(main_task_runner_->HasPendingTask());
    EXPECT_TRUE(needs_main_frame_);
    RunPendingTasksWithSimulatedRaf();
    EXPECT_EQ(0u, event_queue().size());
    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledWhileHandlingEvent,
                             false),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true)));
  }

  // Send a non-cancelable ack required event, and then a non-ack
  // required event they should be coalesced together.
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));

  // Send a non-ack required event, and then a non-cancelable ack
  // required event they should be coalesced together.
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
}

TEST_F(MainThreadEventQueueTest, RafAlignedTouchInputThrottlingMoves) {
  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(2);

  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 50, 50);
  kEvents[0].dispatch_type = WebInputEvent::kEventNonBlocking;
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[1].dispatch_type = WebInputEvent::kEventNonBlocking;

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  // Send a non-cancelable touch move and then send it another one. The
  // second one shouldn't go out with the next rAF call and should be throttled.
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
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
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_EQ(0u, event_queue().size());
}

TEST_F(MainThreadEventQueueTest, LowLatency) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 50, 50);

  queue_->SetNeedsLowLatency(true);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(8);

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());

  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);
  WebMouseWheelEvent mouse_wheel =
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 0, false);

  HandleEvent(mouse_move, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(mouse_wheel, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_EQ(0u, event_queue().size());

  // Now turn off low latency mode.
  queue_->SetNeedsLowLatency(false);
  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());

  HandleEvent(mouse_move, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  HandleEvent(mouse_wheel, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_EQ(0u, event_queue().size());
}

TEST_F(MainThreadEventQueueTest, BlockingTouchesDuringFling) {
  SyntheticWebTouchEvent kEvents;
  kEvents.PressPoint(10, 10);
  kEvents.touch_start_or_first_touch_move = true;
  set_enable_fling_passive_listener_flag(true);

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(4);

  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(0)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(0)->taskAsEvent()->EventPointer());
  kEvents.dispatch_type = WebInputEvent::kListenersForcedNonBlockingDueToFling;
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.MovePoint(0, 30, 30);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(1)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(1)->taskAsEvent()->EventPointer());
  kEvents.dispatch_type = WebInputEvent::kListenersForcedNonBlockingDueToFling;
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.MovePoint(0, 50, 50);
  kEvents.touch_start_or_first_touch_move = false;
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(2)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::kBlocking);
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(2)->taskAsEvent()->EventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.ReleasePoint(0);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(3)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::kBlocking);
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(3)->taskAsEvent()->EventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);
}

TEST_F(MainThreadEventQueueTest, BlockingTouchesOutsideFling) {
  SyntheticWebTouchEvent kEvents;
  kEvents.PressPoint(10, 10);
  kEvents.touch_start_or_first_touch_move = true;
  set_enable_fling_passive_listener_flag(false);

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(4);

  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(0)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(0)->taskAsEvent()->EventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  set_enable_fling_passive_listener_flag(false);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(1)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(1)->taskAsEvent()->EventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  set_enable_fling_passive_listener_flag(true);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(2)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(2)->taskAsEvent()->EventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);

  kEvents.MovePoint(0, 30, 30);
  HandleEvent(kEvents, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_tasks_.size());
  EXPECT_EQ(kEvents.size(),
            handled_tasks_.at(3)->taskAsEvent()->Event().size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(3)->taskAsEvent()->EventPointer());
  EXPECT_EQ(kEvents, *last_touch_event);
}

class MainThreadEventQueueInitializationTest
    : public testing::Test,
      public MainThreadEventQueueClient {
 public:
  MainThreadEventQueueInitializationTest()
      : field_trial_list_(new base::FieldTrialList(nullptr)) {}

  base::TimeDelta main_thread_responsiveness_threshold() {
    return queue_->main_thread_responsiveness_threshold_;
  }

  bool enable_non_blocking_due_to_main_thread_responsiveness_flag() {
    return queue_->enable_non_blocking_due_to_main_thread_responsiveness_flag_;
  }

  void HandleInputEvent(const blink::WebCoalescedInputEvent& event,
                        const ui::LatencyInfo& latency,
                        HandledEventCallback callback) override {
    std::move(callback).Run(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, latency,
                            nullptr, base::nullopt);
  }

  void SetNeedsMainFrame() override {}

 protected:
  scoped_refptr<MainThreadEventQueue> queue_;
  base::test::ScopedFeatureList feature_list_;
  blink::scheduler::MockRendererScheduler renderer_scheduler_;
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(MainThreadEventQueueInitializationTest,
       MainThreadResponsivenessThresholdEnabled) {
  feature_list_.InitFromCommandLine(
      features::kMainThreadBusyScrollIntervention.name, "");

  base::FieldTrialList::CreateFieldTrial(
      "MainThreadResponsivenessScrollIntervention", "Enabled123");
  queue_ = new MainThreadEventQueue(this, main_task_runner_,
                                    &renderer_scheduler_, true);
  EXPECT_TRUE(enable_non_blocking_due_to_main_thread_responsiveness_flag());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(123),
            main_thread_responsiveness_threshold());
}

TEST_F(MainThreadEventQueueInitializationTest,
       MainThreadResponsivenessThresholdDisabled) {
  base::FieldTrialList::CreateFieldTrial(
      "MainThreadResponsivenessScrollIntervention", "Control");
  queue_ = new MainThreadEventQueue(this, main_task_runner_,
                                    &renderer_scheduler_, true);
  EXPECT_FALSE(enable_non_blocking_due_to_main_thread_responsiveness_flag());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            main_thread_responsiveness_threshold());
}

TEST_F(MainThreadEventQueueTest, QueuingTwoClosures) {
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  QueueClosure();
  QueueClosure();
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, handled_tasks_.at(0)->taskAsClosure());
  EXPECT_EQ(2u, handled_tasks_.at(1)->taskAsClosure());
}

TEST_F(MainThreadEventQueueTest, QueuingClosureWithRafEvent) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);

  // Simulate queueuing closure, event, closure, raf aligned event.
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  QueueClosure();
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(2);

  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  QueueClosure();
  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(4u, event_queue().size());

  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();

  // The queue should still have the rAF event.
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();

  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_EQ(1u, handled_tasks_.at(0)->taskAsClosure());
  EXPECT_EQ(kEvents[0].GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(2u, handled_tasks_.at(2)->taskAsClosure());
  EXPECT_EQ(kEvents[1].GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
}

TEST_F(MainThreadEventQueueTest, QueuingClosuresBetweenEvents) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].ReleasePoint(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(2);

  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  QueueClosure();
  QueueClosure();
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(4u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_EQ(kEvents[0].GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(1u, handled_tasks_.at(1)->taskAsClosure());
  EXPECT_EQ(2u, handled_tasks_.at(2)->taskAsClosure());
  EXPECT_EQ(kEvents[1].GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
}

TEST_F(MainThreadEventQueueTest, BlockingTouchMoveBecomesNonBlocking) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 20, 20);
  kEvents[1].SetModifiers(1);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 30);
  kEvents[1].dispatch_type = WebInputEvent::kEventNonBlocking;
  WebTouchEvent scroll_start(WebInputEvent::kTouchScrollStarted,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(3);
  EXPECT_EQ(WebInputEvent::kBlocking, kEvents[0].dispatch_type);
  EXPECT_EQ(WebInputEvent::kEventNonBlocking, kEvents[1].dispatch_type);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(scroll_start, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(3u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::ElementsAre(
                  ReceivedCallback(
                      CallbackReceivedState::kCalledAfterHandleEvent, false),
                  ReceivedCallback(
                      CallbackReceivedState::kCalledWhileHandlingEvent, false),
                  ReceivedCallback(
                      CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_EQ(WebInputEvent::kEventNonBlocking,
            static_cast<const WebTouchEvent&>(
                handled_tasks_.at(0)->taskAsEvent()->Event())
                .dispatch_type);
  EXPECT_EQ(WebInputEvent::kEventNonBlocking,
            static_cast<const WebTouchEvent&>(
                handled_tasks_.at(1)->taskAsEvent()->Event())
                .dispatch_type);
}

TEST_F(MainThreadEventQueueTest, BlockingTouchMoveWithTouchEnd) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 20, 20);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].ReleasePoint(0);
  WebTouchEvent scroll_start(WebInputEvent::kTouchScrollStarted,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(3);
  EXPECT_EQ(WebInputEvent::kBlocking, kEvents[0].dispatch_type);
  EXPECT_EQ(WebInputEvent::kBlocking, kEvents[1].dispatch_type);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  HandleEvent(scroll_start, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(3u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_EQ(WebInputEvent::kBlocking,
            static_cast<const WebTouchEvent&>(
                handled_tasks_.at(0)->taskAsEvent()->Event())
                .dispatch_type);
  EXPECT_EQ(WebInputEvent::kBlocking,
            static_cast<const WebTouchEvent&>(
                handled_tasks_.at(1)->taskAsEvent()->Event())
                .dispatch_type);
}

TEST_F(MainThreadEventQueueTest, UnbufferedDispatchTouchEvent) {
  SyntheticWebTouchEvent kEvents[3];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].ReleasePoint(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(3);

  EXPECT_EQ(WebInputEvent::kBlocking, kEvents[0].dispatch_type);
  EXPECT_EQ(WebInputEvent::kBlocking, kEvents[1].dispatch_type);
  HandleEvent(kEvents[0], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(kEvents[1], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(kEvents[2], INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);
}

TEST_F(MainThreadEventQueueTest, UnbufferedDispatchMouseEvent) {
  WebMouseEvent mouse_down = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseDown, 10, 10, 0);
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);
  WebMouseEvent mouse_up =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseUp, 10, 10, 0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(renderer_scheduler_,
              DidHandleInputEventOnMainThread(testing::_, testing::_))
      .Times(3);

  HandleEvent(mouse_down, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(mouse_move, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(mouse_up, INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);
}

}  // namespace content
