// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_controller.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/fling_booster.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace {
void GiveItSomeTime(uint64_t delay_in_ms) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMillisecondsD(delay_in_ms));
  run_loop.Run();
}
}  // namespace

namespace content {

class FakeFlingController : public FlingController {
 public:
  FakeFlingController(GestureEventQueue* gesture_event_queue,
                      TouchpadTapSuppressionControllerClient* touchpad_client,
                      FlingControllerClient* fling_client,
                      const Config& config)
      : FlingController(gesture_event_queue,
                        touchpad_client,
                        fling_client,
                        config) {}

  bool FlingBoosted() const { return fling_booster_->fling_boosted(); }
};

class FlingControllerTest : public testing::Test,
                            public TouchpadTapSuppressionControllerClient,
                            public GestureEventQueueClient,
                            public FlingControllerClient {
 public:
  // testing::Test
  FlingControllerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  ~FlingControllerTest() override {}

  void SetUp() override {
    queue_ = std::make_unique<GestureEventQueue>(this, this, this,
                                                 GestureEventQueue::Config());
    fling_controller_ = std::make_unique<FakeFlingController>(
        queue_.get(), this, this, FlingController::Config());
    feature_list_.InitFromCommandLine(
        features::kTouchpadAndWheelScrollLatching.name, "");
  }

  // TouchpadTapSuppressionControllerClient
  void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& event) override {}

  // GestureEventQueueClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) override {}
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckSource ack_source,
                         InputEventAckState ack_result) override {}

  // FlingControllerClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {
    last_sent_wheel_ = wheel_event.event;
  }
  void SetNeedsBeginFrameForFlingProgress() override {
    DCHECK(!scheduled_next_fling_progress_);
    scheduled_next_fling_progress_ = true;
  }

  void SimulateFlingStart(blink::WebGestureDevice source_device,
                          const gfx::Vector2dF& velocity) {
    scheduled_next_fling_progress_ = false;
    WebGestureEvent fling_start(
        WebInputEvent::kGestureFlingStart, 0,
        ui::EventTimeStampToSeconds(base::TimeTicks::Now()));
    fling_start.source_device = source_device;
    fling_start.data.fling_start.velocity_x = velocity.x();
    fling_start.data.fling_start.velocity_y = velocity.y();
    GestureEventWithLatencyInfo fling_start_with_latency(fling_start);
    if (!fling_controller_->FilterGestureEvent(fling_start_with_latency))
      fling_controller_->ProcessGestureFlingStart(fling_start_with_latency);
  }

  void SimulateFlingCancel(blink::WebGestureDevice source_device) {
    WebGestureEvent fling_cancel(
        WebInputEvent::kGestureFlingCancel, 0,
        ui::EventTimeStampToSeconds(base::TimeTicks::Now()));
    fling_cancel.source_device = source_device;
    GestureEventWithLatencyInfo fling_cancel_with_latency(fling_cancel);
    last_fling_cancel_filtered_ =
        fling_controller_->FilterGestureEvent(fling_cancel_with_latency);
    if (!last_fling_cancel_filtered_)
      fling_controller_->ProcessGestureFlingCancel(fling_cancel_with_latency);
  }

  void ProgressFling() {
    DCHECK(scheduled_next_fling_progress_);
    scheduled_next_fling_progress_ = false;
    fling_controller_->ProgressFling(base::TimeTicks::Now());
  }

  bool FlingInProgress() { return fling_controller_->fling_in_progress(); }
  bool FlingBoosted() { return fling_controller_->FlingBoosted(); }

 protected:
  std::unique_ptr<FakeFlingController> fling_controller_;
  WebMouseWheelEvent last_sent_wheel_;
  bool last_fling_cancel_filtered_;
  bool scheduled_next_fling_progress_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<GestureEventQueue> queue_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FlingControllerTest, ControllerSendsWheelEndOnFlingWithZeroVelocity) {
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF());
  // The controller doesn't start a fling and sends a wheel end event
  // immediately.
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

// TODO(crbug.com/795617): Test timing expectations make it flaky.
TEST_F(FlingControllerTest, DISABLED_ControllerHandlesGestureFling) {
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // The first wheel event must have momentum_phase == KPhaseBegan.
  GiveItSomeTime(17);
  ProgressFling();
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // The rest of the wheel events must have momentum_phase == KPhaseChanged.
  GiveItSomeTime(17);
  ProgressFling();
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // Now cancel the fling. The GFC will get suppressed by fling booster.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchpad);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // Wait for the boosting timer to expire. The delayed cancelation must work.
  GiveItSomeTime(500);
  ProgressFling();
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

// TODO(crbug.com/795617): Test timing expectations make it flaky.
TEST_F(FlingControllerTest, DISABLED_ControllerSendsWheelEndWhenFlingIsOver) {
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(100, 0));
  EXPECT_TRUE(FlingInProgress());
  GiveItSomeTime(17);
  ProgressFling();
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  GiveItSomeTime(17);
  ProgressFling();
  while (FlingInProgress()) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
    EXPECT_GT(last_sent_wheel_.delta_x, 0.f);
    GiveItSomeTime(17);
    ProgressFling();
  }

  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

// TODO(crbug.com/795617): Test timing expectations make it flaky.
TEST_F(FlingControllerTest,
       DISABLED_EarlyFlingCancelationOnInertialGSUAckNotConsumed) {
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  GiveItSomeTime(17);
  ProgressFling();
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // A non-consumed GSU ack in inertial state cancels out the rest of the fling.
  WebGestureEvent scroll_update(
      WebInputEvent::kGestureScrollUpdate, 0,
      ui::EventTimeStampToSeconds(base::TimeTicks::Now()));
  scroll_update.data.scroll_update.inertial_phase =
      WebGestureEvent::kMomentumPhase;

  fling_controller_->OnGestureEventAck(
      GestureEventWithLatencyInfo(scroll_update),
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

// TODO(crbug.com/795617): Test timing expectations make it flaky.
TEST_F(FlingControllerTest, DISABLED_ControllerBoostsFling) {
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // The first wheel event must have momentum_phase == KPhaseBegan.
  GiveItSomeTime(17);
  ProgressFling();
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // The rest of the wheel events must have momentum_phase == KPhaseChanged.
  GiveItSomeTime(17);
  ProgressFling();
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // Now cancel the fling. The GFC will get suppressed by fling booster.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchpad);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // The second GFS will boost the current active fling.
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  EXPECT_TRUE(FlingBoosted());
}

}  // namespace content
