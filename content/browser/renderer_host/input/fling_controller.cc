// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_controller.h"

#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/public/common/content_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/fling_booster.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace {
// Maximum time between a fling event's timestamp and the first |Progress| call
// for the fling curve to use the fling timestamp as the initial animation time.
// Two frames allows a minor delay between event creation and the first
// progress.
constexpr base::TimeDelta kMaxMicrosecondsFromFlingTimestampToFirstProgress =
    base::TimeDelta::FromMicroseconds(33333);
}  // namespace

namespace content {

FlingController::Config::Config() {}

FlingController::FlingController(
    GestureEventQueue* gesture_event_queue,
    TouchpadTapSuppressionControllerClient* touchpad_client,
    FlingControllerClient* fling_client,
    const Config& config)
    : gesture_event_queue_(gesture_event_queue),
      client_(fling_client),
      touchpad_tap_suppression_controller_(
          touchpad_client,
          config.touchpad_tap_suppression_config),
      touchscreen_tap_suppression_controller_(
          gesture_event_queue,
          config.touchscreen_tap_suppression_config),
      fling_in_progress_(false),
      send_wheel_events_nonblocking_(
          base::FeatureList::IsEnabled(
              features::kTouchpadAndWheelScrollLatching) &&
          base::FeatureList::IsEnabled(features::kAsyncWheelEvents)) {
  DCHECK(gesture_event_queue);
  DCHECK(touchpad_client);
  DCHECK(fling_client);
}

FlingController::~FlingController() {}

bool FlingController::ShouldForwardForGFCFiltering(
    const GestureEventWithLatencyInfo& gesture_event) const {
  if (gesture_event.event.GetType() != WebInputEvent::kGestureFlingCancel)
    return true;

  if (fling_in_progress_)
    return !fling_booster_->fling_cancellation_is_deferred();

  // Touchpad fling with wheel scroll latching disabled, touchscreen, and
  // auto-scroll flings are still handled by renderer.
  return !gesture_event_queue_->ShouldDiscardFlingCancelEvent(gesture_event);
}

bool FlingController::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.GetType()) {
    case WebInputEvent::kGestureFlingCancel:
      if (gesture_event.event.source_device ==
          blink::kWebGestureDeviceTouchscreen) {
        touchscreen_tap_suppression_controller_.GestureFlingCancel();
      } else if (gesture_event.event.source_device ==
                 blink::kWebGestureDeviceTouchpad) {
        touchpad_tap_suppression_controller_.GestureFlingCancel();
      }
      return true;
    case WebInputEvent::kGestureTapDown:
    case WebInputEvent::kGestureShowPress:
    case WebInputEvent::kGestureTapUnconfirmed:
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGestureTap:
    case WebInputEvent::kGestureDoubleTap:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
    case WebInputEvent::kGestureTwoFingerTap:
      if (gesture_event.event.source_device ==
          blink::kWebGestureDeviceTouchscreen) {
        return !touchscreen_tap_suppression_controller_.FilterTapEvent(
            gesture_event);
      }
      return true;
    default:
      return true;
  }
}

bool FlingController::FilterGestureEventForFlingBoosting(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (!fling_booster_)
    return false;

  // TODO(sahel): Don't boost touchpad fling for now. Once browserside
  // touchscreen fling is implemented, move the fling_controller_ from
  // GestureEventQueue to RednerWidgetHostImpl. This will gaurantee proper
  // gesture scroll event order in RednerWidgetHostImpl while boosting.
  if (gesture_event.event.source_device == blink::kWebGestureDeviceTouchpad)
    return false;

  bool cancel_current_fling;
  bool should_filter_event = fling_booster_->FilterGestureEventForFlingBoosting(
      gesture_event.event, &cancel_current_fling);
  if (cancel_current_fling) {
    CancelCurrentFling();
  }

  if (should_filter_event &&
      gesture_event.event.GetType() == WebInputEvent::kGestureFlingStart) {
    UpdateCurrentFlingState(gesture_event.event,
                            fling_booster_->current_fling_velocity());
  }

  return should_filter_event;
}

bool FlingController::FilterGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  return !ShouldForwardForGFCFiltering(gesture_event) ||
         !ShouldForwardForTapSuppression(gesture_event) ||
         FilterGestureEventForFlingBoosting(gesture_event);
}

void FlingController::OnGestureEventAck(
    const GestureEventWithLatencyInfo& acked_event,
    InputEventAckState ack_result) {
  bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  switch (acked_event.event.GetType()) {
    case WebInputEvent::kGestureFlingCancel: {
      blink::WebGestureDevice source_device = acked_event.event.source_device;
      if (source_device == blink::kWebGestureDeviceTouchscreen) {
        touchscreen_tap_suppression_controller_.GestureFlingCancelAck(
            processed);
      } else if (source_device == blink::kWebGestureDeviceTouchpad) {
        touchpad_tap_suppression_controller_.GestureFlingCancelAck(processed);
      }
      break;
    }
    case WebInputEvent::kGestureScrollUpdate:
      if (acked_event.event.data.scroll_update.inertial_phase ==
              WebGestureEvent::kMomentumPhase &&
          fling_curve_ && !processed) {
        CancelCurrentFling();
      }
    default:
      break;
  }
}

void FlingController::ProcessGestureFlingStart(
    const GestureEventWithLatencyInfo& gesture_event) {
  const float vx = gesture_event.event.data.fling_start.velocity_x;
  const float vy = gesture_event.event.data.fling_start.velocity_y;
  if (!UpdateCurrentFlingState(gesture_event.event, gfx::Vector2dF(vx, vy)))
    return;
  has_fling_animation_started_ = false;
  fling_in_progress_ = true;
  fling_booster_ = std::make_unique<ui::FlingBooster>(
      current_fling_parameters_.velocity,
      current_fling_parameters_.source_device,
      current_fling_parameters_.modifiers);
  ScheduleFlingProgress();
}

void FlingController::ScheduleFlingProgress() {
  client_->SetNeedsBeginFrameForFlingProgress();
}

void FlingController::ProcessGestureFlingCancel(
    const GestureEventWithLatencyInfo& gesture_event) {
  fling_in_progress_ = false;
  if (fling_curve_) {
    CancelCurrentFling();

    // FlingCancelEvent handled without being sent to the renderer.
    touchpad_tap_suppression_controller_.GestureFlingCancelAck(true);
    return;
  }

  // FlingCancelEvent ignored without being sent to the renderer.
  touchpad_tap_suppression_controller_.GestureFlingCancelAck(false);
}

void FlingController::ProgressFling(base::TimeTicks current_time) {
  if (!fling_curve_)
    return;

  DCHECK(fling_booster_);
  fling_booster_->set_last_fling_animation_time(
      (current_time - base::TimeTicks()).InSecondsF());
  if (fling_booster_->MustCancelDeferredFling()) {
    CancelCurrentFling();
    return;
  }
  if (!has_fling_animation_started_) {
    // Guard against invalid, future or sufficiently stale start times, as there
    // are no guarantees fling event and progress timestamps are compatible.
    if (current_fling_parameters_.start_time.is_null() ||
        current_time <= current_fling_parameters_.start_time ||
        current_time >= current_fling_parameters_.start_time +
                            kMaxMicrosecondsFromFlingTimestampToFirstProgress) {
      current_fling_parameters_.start_time = current_time;
      ScheduleFlingProgress();
      return;
    }
  }

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF delta_to_scroll;
  bool fling_is_active = fling_curve_->Advance(
      (current_time - current_fling_parameters_.start_time).InSecondsF(),
      current_velocity, delta_to_scroll);
  if (fling_is_active) {
    if (delta_to_scroll != gfx::Vector2d()) {
      blink::WebMouseWheelEvent::Phase phase =
          has_fling_animation_started_
              ? blink::WebMouseWheelEvent::kPhaseChanged
              : blink::WebMouseWheelEvent::kPhaseBegan;
      GenerateAndSendWheelEvents(delta_to_scroll, phase);
      has_fling_animation_started_ = true;
    }
    // As long as the fling curve is active, the fling progress must get
    // scheduled even when the last delta to scroll was zero.
    ScheduleFlingProgress();
  } else {  // !is_fling_active
    CancelCurrentFling();
  }
}

void FlingController::StopFling() {
  if (!fling_curve_)
    return;

  CancelCurrentFling();
}

void FlingController::GenerateAndSendWheelEvents(
    gfx::Vector2dF delta,
    blink::WebMouseWheelEvent::Phase phase) {
  MouseWheelEventWithLatencyInfo synthetic_wheel(
      WebInputEvent::kMouseWheel, current_fling_parameters_.modifiers,
      ui::EventTimeStampToSeconds(base::TimeTicks::Now()),
      ui::LatencyInfo(ui::SourceEventType::WHEEL));
  synthetic_wheel.event.delta_x = delta.x();
  synthetic_wheel.event.delta_y = delta.y();
  synthetic_wheel.event.has_precise_scrolling_deltas = true;
  synthetic_wheel.event.momentum_phase = phase;
  synthetic_wheel.event.has_synthetic_phase = true;
  synthetic_wheel.event.SetPositionInWidget(
      current_fling_parameters_.point.x(), current_fling_parameters_.point.y());
  synthetic_wheel.event.SetPositionInScreen(
      current_fling_parameters_.global_point.x(),
      current_fling_parameters_.global_point.y());
  // Send wheel end events nonblocking since they have zero delta and are not
  // sent to JS.
  if (phase == blink::WebMouseWheelEvent::kPhaseEnded) {
    synthetic_wheel.event.dispatch_type = WebInputEvent::kEventNonBlocking;
  } else {
    synthetic_wheel.event.dispatch_type = send_wheel_events_nonblocking_
                                              ? WebInputEvent::kEventNonBlocking
                                              : WebInputEvent::kBlocking;
  }

  client_->SendGeneratedWheelEvent(synthetic_wheel);
}

void FlingController::CancelCurrentFling() {
  fling_curve_.reset();
  has_fling_animation_started_ = false;
  fling_in_progress_ = false;
  GenerateAndSendWheelEvents(gfx::Vector2d(),
                             blink::WebMouseWheelEvent::kPhaseEnded);
  current_fling_parameters_ = ActiveFlingParameters();

  if (fling_booster_) {
    if (fling_booster_->fling_cancellation_is_deferred()) {
      WebGestureEvent last_fling_boost_event =
          fling_booster_->last_boost_event();
      fling_booster_.reset();
      if (last_fling_boost_event.GetType() ==
              WebInputEvent::kGestureScrollBegin ||
          last_fling_boost_event.GetType() ==
              WebInputEvent::kGestureScrollUpdate) {
        // Synthesize a GestureScrollBegin, as the original event was
        // suppressed.
        WebGestureEvent scroll_begin_event = last_fling_boost_event;
        scroll_begin_event.SetType(WebInputEvent::kGestureScrollBegin);
        scroll_begin_event.SetTimeStampSeconds(
            ui::EventTimeStampToSeconds(base::TimeTicks::Now()));
        bool is_update = last_fling_boost_event.GetType() ==
                         WebInputEvent::kGestureScrollUpdate;
        float delta_x_hint =
            is_update ? last_fling_boost_event.data.scroll_update.delta_x
                      : last_fling_boost_event.data.scroll_begin.delta_x_hint;
        float delta_y_hint =
            is_update ? last_fling_boost_event.data.scroll_update.delta_y
                      : last_fling_boost_event.data.scroll_begin.delta_y_hint;
        scroll_begin_event.data.scroll_begin.delta_x_hint = delta_x_hint;
        scroll_begin_event.data.scroll_begin.delta_y_hint = delta_y_hint;
        gesture_event_queue_->QueueEvent(GestureEventWithLatencyInfo(
            scroll_begin_event, ui::LatencyInfo(ui::SourceEventType::WHEEL)));
      }
    } else {
      fling_booster_.reset();
    }
  }
}

bool FlingController::UpdateCurrentFlingState(
    const WebGestureEvent& fling_start_event,
    const gfx::Vector2dF& velocity) {
  DCHECK_EQ(WebInputEvent::kGestureFlingStart, fling_start_event.GetType());
  if (velocity.IsZero()) {
    CancelCurrentFling();
    return false;
  }

  current_fling_parameters_.velocity = velocity;
  current_fling_parameters_.point =
      gfx::Vector2d(fling_start_event.x, fling_start_event.y);
  current_fling_parameters_.global_point =
      gfx::Vector2d(fling_start_event.global_x, fling_start_event.global_y);
  current_fling_parameters_.modifiers = fling_start_event.GetModifiers();
  current_fling_parameters_.source_device = fling_start_event.source_device;
  current_fling_parameters_.start_time =
      base::TimeTicks() +
      base::TimeDelta::FromSecondsD(fling_start_event.TimeStampSeconds());
  fling_curve_ = std::unique_ptr<blink::WebGestureCurve>(
      ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
          current_fling_parameters_.source_device,
          current_fling_parameters_.velocity,
          gfx::Vector2dF() /*initial_offset*/, false /*on_main_thread*/));
  return true;
}

TouchpadTapSuppressionController*
FlingController::GetTouchpadTapSuppressionController() {
  return &touchpad_tap_suppression_controller_;
}

}  // namespace content
