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

  // Auto-scroll flings are still handled by renderer.
  return !gesture_event_queue_->ShouldDiscardFlingCancelEvent(gesture_event);
}

bool FlingController::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.GetType()) {
    case WebInputEvent::kGestureFlingCancel:
      if (gesture_event.event.SourceDevice() ==
          blink::kWebGestureDeviceTouchscreen) {
        touchscreen_tap_suppression_controller_.GestureFlingCancel();
      } else if (gesture_event.event.SourceDevice() ==
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
      if (gesture_event.event.SourceDevice() ==
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
  if (gesture_event.event.SourceDevice() == blink::kWebGestureDeviceTouchpad)
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
    case WebInputEvent::kGestureScrollUpdate:
      if (acked_event.event.data.scroll_update.inertial_phase ==
              WebGestureEvent::kMomentumPhase &&
          fling_curve_ && !processed &&
          current_fling_parameters_.source_device !=
              blink::kWebGestureDeviceSyntheticAutoscroll) {
        CancelCurrentFling();
      }
      break;
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
  bool processed = false;
  if (fling_curve_) {
    CancelCurrentFling();
    processed = true;
  }
  // FlingCancelEvent handled without being sent to the renderer.
  blink::WebGestureDevice source_device = gesture_event.event.SourceDevice();
  if (source_device == blink::kWebGestureDeviceTouchscreen) {
    touchscreen_tap_suppression_controller_.GestureFlingCancelAck(processed);
  } else if (source_device == blink::kWebGestureDeviceTouchpad) {
    touchpad_tap_suppression_controller_.GestureFlingCancelAck(processed);
  }
}

gfx::Vector2dF FlingController::ProgressFling(base::TimeTicks current_time) {
  if (!fling_curve_)
    return gfx::Vector2dF();

  DCHECK(fling_booster_);
  fling_booster_->set_last_fling_animation_time(
      (current_time - base::TimeTicks()).InSecondsF());
  if (fling_booster_->MustCancelDeferredFling()) {
    CancelCurrentFling();
    return gfx::Vector2dF();
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
      return current_fling_parameters_.velocity;
    }
  }

  gfx::Vector2dF delta_to_scroll;
  bool fling_is_active = fling_curve_->Advance(
      (current_time - current_fling_parameters_.start_time).InSecondsF(),
      current_fling_parameters_.velocity, delta_to_scroll);
  if (fling_is_active) {
    if (delta_to_scroll != gfx::Vector2d()) {
      GenerateAndSendFlingProgressEvents(delta_to_scroll);
      has_fling_animation_started_ = true;
    }
    // As long as the fling curve is active, the fling progress must get
    // scheduled even when the last delta to scroll was zero.
    ScheduleFlingProgress();
    return current_fling_parameters_.velocity;
  }

  if (current_fling_parameters_.source_device !=
      blink::kWebGestureDeviceSyntheticAutoscroll) {
    CancelCurrentFling();
  }
  return gfx::Vector2dF();
}

void FlingController::StopFling() {
  if (!fling_curve_)
    return;

  CancelCurrentFling();
}

void FlingController::GenerateAndSendWheelEvents(
    const gfx::Vector2dF& delta,
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
  synthetic_wheel.event.SetPositionInWidget(current_fling_parameters_.point);
  synthetic_wheel.event.SetPositionInScreen(
      current_fling_parameters_.global_point);
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

void FlingController::GenerateAndSendGestureScrollEvents(
    WebInputEvent::Type type,
    const gfx::Vector2dF& delta /* = gfx::Vector2dF() */) {
  GestureEventWithLatencyInfo synthetic_gesture(
      type, current_fling_parameters_.modifiers,
      ui::EventTimeStampToSeconds(base::TimeTicks::Now()),
      ui::LatencyInfo(ui::SourceEventType::TOUCH));
  synthetic_gesture.event.SetPositionInWidget(current_fling_parameters_.point);
  synthetic_gesture.event.SetPositionInScreen(
      current_fling_parameters_.global_point);
  synthetic_gesture.event.primary_pointer_type =
      blink::WebPointerProperties::PointerType::kTouch;
  synthetic_gesture.event.SetSourceDevice(
      current_fling_parameters_.source_device);
  if (type == WebInputEvent::kGestureScrollUpdate) {
    synthetic_gesture.event.data.scroll_update.delta_x = delta.x();
    synthetic_gesture.event.data.scroll_update.delta_y = delta.y();
    synthetic_gesture.event.data.scroll_update.inertial_phase =
        WebGestureEvent::kMomentumPhase;
  } else {
    DCHECK_EQ(WebInputEvent::kGestureScrollEnd, type);
    synthetic_gesture.event.data.scroll_end.inertial_phase =
        WebGestureEvent::kMomentumPhase;
  }
  client_->SendGeneratedGestureScrollEvents(synthetic_gesture);
}

void FlingController::GenerateAndSendFlingProgressEvents(
    const gfx::Vector2dF& delta) {
  switch (current_fling_parameters_.source_device) {
    case blink::kWebGestureDeviceTouchpad: {
      blink::WebMouseWheelEvent::Phase phase =
          has_fling_animation_started_
              ? blink::WebMouseWheelEvent::kPhaseChanged
              : blink::WebMouseWheelEvent::kPhaseBegan;
      GenerateAndSendWheelEvents(delta, phase);
      break;
    }
    case blink::kWebGestureDeviceTouchscreen:
    case blink::kWebGestureDeviceSyntheticAutoscroll:
      GenerateAndSendGestureScrollEvents(WebInputEvent::kGestureScrollUpdate,
                                         delta);
      break;
    default:
      NOTREACHED()
          << "Fling controller doesn't handle flings with source device:"
          << current_fling_parameters_.source_device;
  }
}

void FlingController::GenerateAndSendFlingEndEvents() {
  switch (current_fling_parameters_.source_device) {
    case blink::kWebGestureDeviceTouchpad:
      GenerateAndSendWheelEvents(gfx::Vector2d(),
                                 blink::WebMouseWheelEvent::kPhaseEnded);
      break;
    case blink::kWebGestureDeviceTouchscreen:
    case blink::kWebGestureDeviceSyntheticAutoscroll:
      GenerateAndSendGestureScrollEvents(WebInputEvent::kGestureScrollEnd);
      break;
    default:
      NOTREACHED()
          << "Fling controller doesn't handle flings with source device:"
          << current_fling_parameters_.source_device;
  }
}

void FlingController::CancelCurrentFling() {
  bool had_active_fling = !!fling_curve_;
  fling_curve_.reset();
  has_fling_animation_started_ = false;
  fling_in_progress_ = false;
  gesture_event_queue_->FlingHasBeenHalted();

  // Extract the last event filtered by the fling booster if it exists.
  bool fling_cancellation_is_deferred =
      fling_booster_ && fling_booster_->fling_cancellation_is_deferred();
  WebGestureEvent last_fling_boost_event;
  if (fling_cancellation_is_deferred)
    last_fling_boost_event = fling_booster_->last_boost_event();

  // Reset the state of the fling.
  fling_booster_.reset();
  GenerateAndSendFlingEndEvents();
  current_fling_parameters_ = ActiveFlingParameters();

  // Synthesize a GestureScrollBegin, as the original event was suppressed. It
  // is important to send the GSB after resetting the fling_booster_ otherwise
  // it will get filtered by the booster again.
  if (fling_cancellation_is_deferred &&
      (last_fling_boost_event.GetType() == WebInputEvent::kGestureScrollBegin ||
       last_fling_boost_event.GetType() ==
           WebInputEvent::kGestureScrollUpdate)) {
    WebGestureEvent scroll_begin_event = last_fling_boost_event;
    scroll_begin_event.SetType(WebInputEvent::kGestureScrollBegin);
    scroll_begin_event.SetTimeStampSeconds(
        ui::EventTimeStampToSeconds(base::TimeTicks::Now()));
    bool is_update =
        last_fling_boost_event.GetType() == WebInputEvent::kGestureScrollUpdate;
    float delta_x_hint =
        is_update ? last_fling_boost_event.data.scroll_update.delta_x
                  : last_fling_boost_event.data.scroll_begin.delta_x_hint;
    float delta_y_hint =
        is_update ? last_fling_boost_event.data.scroll_update.delta_y
                  : last_fling_boost_event.data.scroll_begin.delta_y_hint;
    scroll_begin_event.data.scroll_begin.delta_x_hint = delta_x_hint;
    scroll_begin_event.data.scroll_begin.delta_y_hint = delta_y_hint;
    ui::SourceEventType latency_source_event_type =
        ui::SourceEventType::UNKNOWN;
    if (scroll_begin_event.SourceDevice() ==
        blink::kWebGestureDeviceTouchscreen) {
      latency_source_event_type = ui::SourceEventType::TOUCH;
    } else if (scroll_begin_event.SourceDevice() ==
               blink::kWebGestureDeviceTouchpad) {
      latency_source_event_type = ui::SourceEventType::WHEEL;
    }

    client_->SendGeneratedGestureScrollEvents(GestureEventWithLatencyInfo(
        scroll_begin_event, ui::LatencyInfo(latency_source_event_type)));
  }

  if (had_active_fling)
    client_->DidStopFlingingOnBrowser();
}

bool FlingController::UpdateCurrentFlingState(
    const WebGestureEvent& fling_start_event,
    const gfx::Vector2dF& velocity) {
  DCHECK_EQ(WebInputEvent::kGestureFlingStart, fling_start_event.GetType());

  current_fling_parameters_.velocity = velocity;
  current_fling_parameters_.point = fling_start_event.PositionInWidget();
  current_fling_parameters_.global_point = fling_start_event.PositionInScreen();
  current_fling_parameters_.modifiers = fling_start_event.GetModifiers();
  current_fling_parameters_.source_device = fling_start_event.SourceDevice();
  current_fling_parameters_.start_time =
      base::TimeTicks() +
      base::TimeDelta::FromSecondsD(fling_start_event.TimeStampSeconds());

  if (velocity.IsZero() && fling_start_event.SourceDevice() !=
                               blink::kWebGestureDeviceSyntheticAutoscroll) {
    CancelCurrentFling();
    return false;
  }

  fling_curve_ = std::unique_ptr<blink::WebGestureCurve>(
      ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
          current_fling_parameters_.source_device,
          current_fling_parameters_.velocity,
          gfx::Vector2dF() /*initial_offset*/, false /*on_main_thread*/));
  return true;
}

bool FlingController::FlingCancellationIsDeferred() const {
  return fling_booster_ && fling_booster_->fling_cancellation_is_deferred();
}

bool FlingController::TouchscreenFlingInProgress() const {
  return fling_in_progress_ && current_fling_parameters_.source_device ==
                                   blink::kWebGestureDeviceTouchscreen;
}

TouchpadTapSuppressionController*
FlingController::GetTouchpadTapSuppressionController() {
  return &touchpad_tap_suppression_controller_;
}

}  // namespace content
