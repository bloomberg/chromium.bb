// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/legacy_input_router_impl.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_sender.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/vector2d_f.h"

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

class NullInputAckHandler : public InputAckHandler {
 public:
  NullInputAckHandler() : ack_count_(0) {}
  ~NullInputAckHandler() override {}

  // InputAckHandler
  void OnKeyboardEventAck(const NativeWebKeyboardEventWithLatencyInfo& event,
                          InputEventAckState ack_result) override {
    ++ack_count_;
  }
  void OnMouseEventAck(const MouseEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override {
    ++ack_count_;
  }
  void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override {
    ++ack_count_;
  }
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override {
    ++ack_count_;
  }
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckState ack_result) override {
    ++ack_count_;
  }
  void OnUnexpectedEventAck(UnexpectedEventAckType type) override {
    ++ack_count_;
  }

  size_t GetAndResetAckCount() {
    size_t ack_count = ack_count_;
    ack_count_ = 0;
    return ack_count;
  }

  size_t ack_count() const { return ack_count_; }

 private:
  size_t ack_count_;
};

class NullInputRouterClient : public InputRouterClient {
 public:
  NullInputRouterClient() {}
  ~NullInputRouterClient() override {}

  // InputRouterClient
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) override {
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }
  void IncrementInFlightEventCount(
      blink::WebInputEvent::Type event_type) override {}
  void DecrementInFlightEventCount(InputEventAckSource ack_source) override {}
  void OnHasTouchEventHandlers(bool has_handlers) override {}
  void DidOverscroll(const ui::DidOverscrollParams& params) override {}
  void OnSetWhiteListedTouchAction(
      cc::TouchAction white_listed_touch_action) override {}
  void DidStopFlinging() override {}
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& event,
      const ui::LatencyInfo& latency_info) override {}
};

class NullIPCSender : public IPC::Sender {
 public:
  NullIPCSender() : sent_count_(0) {}
  ~NullIPCSender() override {}

  bool Send(IPC::Message* message) override {
    delete message;
    ++sent_count_;
    return true;
  }

  size_t GetAndResetSentEventCount() {
    size_t message_count = sent_count_;
    sent_count_ = 0;
    return message_count;
  }

  bool HasMessages() const { return sent_count_ > 0; }

 private:
  size_t sent_count_;
};

typedef std::vector<WebGestureEvent> Gestures;
Gestures BuildScrollSequence(size_t steps,
                             const gfx::Vector2dF& origin,
                             const gfx::Vector2dF& distance) {
  Gestures gestures;
  const gfx::Vector2dF delta = ScaleVector2d(distance, 1.f / steps);

  WebGestureEvent gesture(WebInputEvent::kGestureScrollBegin,
                          WebInputEvent::kNoModifiers,
                          ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  gesture.x = origin.x();
  gesture.y = origin.y();
  gestures.push_back(gesture);

  gesture.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture.data.scroll_update.delta_x = delta.x();
  gesture.data.scroll_update.delta_y = delta.y();
  for (size_t i = 0; i < steps; ++i) {
    gesture.x += delta.x();
    gesture.y += delta.y();
    gestures.push_back(gesture);
  }

  gesture.SetType(WebInputEvent::kGestureScrollEnd);
  gestures.push_back(gesture);
  return gestures;
}

typedef std::vector<WebTouchEvent> Touches;
Touches BuildTouchSequence(size_t steps,
                           const gfx::Vector2dF& origin,
                           const gfx::Vector2dF& distance) {
  Touches touches;
  const gfx::Vector2dF delta = ScaleVector2d(distance, 1.f / steps);

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  touch.touches_length = 1;
  touch.touches[0].id = 0;
  touch.touches[0].state = WebTouchPoint::kStatePressed;
  touch.touches[0].SetPositionInWidget(origin.x(), origin.y());
  touch.touches[0].SetPositionInScreen(origin.x(), origin.y());
  touches.push_back(touch);

  touch.SetType(WebInputEvent::kTouchMove);
  touch.touches[0].state = WebTouchPoint::kStateMoved;
  for (size_t i = 0; i < steps; ++i) {
    touch.touches[0].SetPositionInWidget(
        touch.touches[0].PositionInWidget().x + delta.x(),
        touch.touches[0].PositionInWidget().y + delta.y());
    touch.touches[0].SetPositionInScreen(
        touch.touches[0].PositionInScreen().x + delta.x(),
        touch.touches[0].PositionInScreen().y + delta.y());
    touches.push_back(touch);
  }

  touch.SetType(WebInputEvent::kTouchEnd);
  touch.touches[0].state = WebTouchPoint::kStateReleased;
  touches.push_back(touch);
  return touches;
}

class InputEventTimer {
 public:
  InputEventTimer(const char* test_name, int64_t event_count)
      : test_name_(test_name),
        event_count_(event_count),
        start_(base::TimeTicks::Now()) {}

  ~InputEventTimer() {
    perf_test::PrintResult(
        "avg_time_per_event", "", test_name_,
        static_cast<size_t>(((base::TimeTicks::Now() - start_) / event_count_)
                                .InMicroseconds()),
        "us", true);
  }

 private:
  const char* test_name_;
  int64_t event_count_;
  base::TimeTicks start_;
  DISALLOW_COPY_AND_ASSIGN(InputEventTimer);
};

bool ShouldBlockEventStream(const blink::WebInputEvent& event) {
  return ui::WebInputEventTraits::ShouldBlockEventStream(
      event,
      base::FeatureList::IsEnabled(features::kRafAlignedTouchInputEvents),
      base::FeatureList::IsEnabled(features::kTouchpadAndWheelScrollLatching));
}

}  // namespace

class LegacyInputRouterImplPerfTest : public testing::Test {
 public:
  LegacyInputRouterImplPerfTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        last_input_id_(0) {}
  ~LegacyInputRouterImplPerfTest() override {}

 protected:
  // testing::Test
  void SetUp() override {
    sender_.reset(new NullIPCSender());
    client_.reset(new NullInputRouterClient());
    ack_handler_.reset(new NullInputAckHandler());
    input_router_.reset(new LegacyInputRouterImpl(
        sender_.get(), client_.get(), ack_handler_.get(), MSG_ROUTING_NONE,
        InputRouter::Config()));
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();

    input_router_.reset();
    ack_handler_.reset();
    client_.reset();
    sender_.reset();
  }

  void SendEvent(const WebGestureEvent& gesture,
                 const ui::LatencyInfo& latency) {
    input_router_->SendGestureEvent(
        GestureEventWithLatencyInfo(gesture, latency));
  }

  void SendEvent(const WebTouchEvent& touch, const ui::LatencyInfo& latency) {
    input_router_->SendTouchEvent(TouchEventWithLatencyInfo(touch, latency));
  }

  void SendEventAckIfNecessary(const blink::WebInputEvent& event,
                               InputEventAckState ack_result) {
    if (!ShouldBlockEventStream(event))
      return;
    InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD, event.GetType(),
                      ack_result);
    InputHostMsg_HandleInputEvent_ACK response(0, ack);
    input_router_->OnMessageReceived(response);
  }

  void OnHasTouchEventHandlers(bool has_handlers) {
    input_router_->OnMessageReceived(
        ViewHostMsg_HasTouchEventHandlers(0, has_handlers));
  }

  size_t GetAndResetSentEventCount() {
    return sender_->GetAndResetSentEventCount();
  }

  size_t GetAndResetAckCount() { return ack_handler_->GetAndResetAckCount(); }

  size_t AckCount() const { return ack_handler_->ack_count(); }

  int64_t NextLatencyID() { return ++last_input_id_; }

  ui::LatencyInfo CreateLatencyInfo() {
    ui::LatencyInfo latency;
    latency.AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, 1, 0);
    latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1,
                             NextLatencyID());
    return latency;
  }

  template <typename EventType>
  void SimulateEventSequence(const char* test_name,
                             const std::vector<EventType>& events,
                             bool ack_delay,
                             size_t iterations) {
    OnHasTouchEventHandlers(true);

    const size_t event_count = events.size();
    const size_t total_event_count = event_count * iterations;

    InputEventTimer timer(test_name, total_event_count);
    while (iterations--) {
      size_t i = 0, ack_i = 0;
      if (ack_delay)
        SendEvent(events[i++], CreateLatencyInfo());

      for (; i < event_count; ++i, ++ack_i) {
        SendEvent(events[i], CreateLatencyInfo());
        SendEventAckIfNecessary(events[ack_i], INPUT_EVENT_ACK_STATE_CONSUMED);
      }

      if (ack_delay)
        SendEventAckIfNecessary(events.back(), INPUT_EVENT_ACK_STATE_CONSUMED);

      EXPECT_EQ(event_count, GetAndResetSentEventCount());
      EXPECT_EQ(event_count, GetAndResetAckCount());
    }
  }

  void SimulateTouchAndScrollEventSequence(const char* test_name,
                                           size_t steps,
                                           const gfx::Vector2dF& origin,
                                           const gfx::Vector2dF& distance,
                                           size_t iterations) {
    OnHasTouchEventHandlers(true);

    Gestures gestures = BuildScrollSequence(steps, origin, distance);
    Touches touches = BuildTouchSequence(steps, origin, distance);
    ASSERT_EQ(touches.size(), gestures.size());

    const size_t event_count = gestures.size();
    const size_t total_event_count = event_count * iterations * 2;

    InputEventTimer timer(test_name, total_event_count);
    while (iterations--) {
      for (size_t i = 0; i < event_count; ++i) {
        SendEvent(touches[i], CreateLatencyInfo());
        // Touches may not be forwarded after the scroll sequence has begun, so
        // only ack if necessary.
        if (!AckCount()) {
          SendEventAckIfNecessary(touches[i],
                                  INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        }

        SendEvent(gestures[i], CreateLatencyInfo());
        SendEventAckIfNecessary(gestures[i], INPUT_EVENT_ACK_STATE_CONSUMED);
        EXPECT_EQ(2U, GetAndResetAckCount());
      }
    }
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  int64_t last_input_id_;
  std::unique_ptr<NullIPCSender> sender_;
  std::unique_ptr<NullInputRouterClient> client_;
  std::unique_ptr<NullInputAckHandler> ack_handler_;
  std::unique_ptr<LegacyInputRouterImpl> input_router_;
};

const size_t kDefaultSteps(100);
const size_t kDefaultIterations(100);
const gfx::Vector2dF kDefaultOrigin(100, 100);
const gfx::Vector2dF kDefaultDistance(500, 500);

TEST_F(LegacyInputRouterImplPerfTest, TouchSwipe) {
  SimulateEventSequence(
      "TouchSwipe ",
      BuildTouchSequence(kDefaultSteps, kDefaultOrigin, kDefaultDistance),
      false, kDefaultIterations);
}

TEST_F(LegacyInputRouterImplPerfTest, TouchSwipeDelayedAck) {
  SimulateEventSequence(
      "TouchSwipeDelayedAck ",
      BuildTouchSequence(kDefaultSteps, kDefaultOrigin, kDefaultDistance), true,
      kDefaultIterations);
}

TEST_F(LegacyInputRouterImplPerfTest, GestureScroll) {
  SimulateEventSequence(
      "GestureScroll ",
      BuildScrollSequence(kDefaultSteps, kDefaultOrigin, kDefaultDistance),
      false, kDefaultIterations);
}

TEST_F(LegacyInputRouterImplPerfTest, GestureScrollDelayedAck) {
  SimulateEventSequence(
      "GestureScrollDelayedAck ",
      BuildScrollSequence(kDefaultSteps, kDefaultOrigin, kDefaultDistance),
      true, kDefaultIterations);
}

TEST_F(LegacyInputRouterImplPerfTest, TouchSwipeToGestureScroll) {
  SimulateTouchAndScrollEventSequence("TouchSwipeToGestureScroll ",
                                      kDefaultSteps, kDefaultOrigin,
                                      kDefaultDistance, kDefaultIterations);
}

}  // namespace content
