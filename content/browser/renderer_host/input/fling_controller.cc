// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_controller.h"

#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/fling_booster.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace {
constexpr base::TimeDelta kFrameDelta =
    base::TimeDelta::FromSecondsD(1.0 / 60.0);

// Maximum time between a fling event's timestamp and the first |Progress| call
// for the fling curve to use the fling timestamp as the initial animation time.
// Two frames allows a minor delay between event creation and the first
// progress.
constexpr base::TimeDelta kMaxMicrosecondsFromFlingTimestampToFirstProgress =
    base::TimeDelta::FromSecondsD(2.0 / 60.0);

// Since the progress fling is called in ProcessGestureFlingStart right after
// processing the GFS, it is possible to have a very small delta for the first
// event. Don't send an event with deltas smaller than the
// |kMinInertialScrollDelta| since the renderer ignores it and the fling gets
// cancelled in FlingController::OnGestureEventAck due to an inertial GSU with
// ack ignored.
const float kMinInertialScrollDelta = 0.1f;

const char* kFlingTraceName = "FlingController::HandlingGestureFling";
}  // namespace

namespace content {

FlingController::Config::Config() {}

FlingController::FlingController(
    FlingControllerEventSenderClient* event_sender_client,
    FlingControllerSchedulerClient* scheduler_client,
    const Config& config)
    : event_sender_client_(event_sender_client),
      scheduler_client_(scheduler_client),
      touchpad_tap_suppression_controller_(
          config.touchpad_tap_suppression_config),
      touchscreen_tap_suppression_controller_(
          config.touchscreen_tap_suppression_config),
      fling_in_progress_(false),
      clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this) {
  DCHECK(event_sender_client);
  DCHECK(scheduler_client);
}

FlingController::~FlingController() = default;

bool FlingController::ShouldForwardForGFCFiltering(
    const GestureEventWithLatencyInfo& gesture_event) const {
  if (gesture_event.event.GetType() != WebInputEvent::kGestureFlingCancel)
    return true;

  if (fling_in_progress_)
    return !fling_booster_->fling_cancellation_is_deferred();

  return false;
}

bool FlingController::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.GetType()) {
    case WebInputEvent::kGestureFlingCancel:
      if (gesture_event.event.SourceDevice() ==
          blink::WebGestureDevice::kTouchscreen) {
        touchscreen_tap_suppression_controller_
            .GestureFlingCancelStoppedFling();
      } else if (gesture_event.event.SourceDevice() ==
                 blink::WebGestureDevice::kTouchpad) {
        touchpad_tap_suppression_controller_.GestureFlingCancelStoppedFling();
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
          blink::WebGestureDevice::kTouchscreen) {
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

  bool cancel_current_fling;
  bool should_filter_event = fling_booster_->FilterGestureEventForFlingBoosting(
      gesture_event.event, &cancel_current_fling);
  if (cancel_current_fling) {
    CancelCurrentFling();
  }

  if (should_filter_event) {
    if (gesture_event.event.GetType() == WebInputEvent::kGestureFlingStart) {
      UpdateCurrentFlingState(gesture_event.event,
                              fling_booster_->current_fling_velocity());
      TRACE_EVENT_INSTANT2("input",
                           fling_booster_->fling_boosted()
                               ? "FlingController::FlingBoosted"
                               : "FlingController::FlingReplaced",
                           TRACE_EVENT_SCOPE_THREAD, "vx",
                           fling_booster_->current_fling_velocity().x(), "vy",
                           fling_booster_->current_fling_velocity().y());
    } else if (gesture_event.event.GetType() ==
               WebInputEvent::kGestureFlingCancel) {
      DCHECK(fling_booster_->fling_cancellation_is_deferred());
      TRACE_EVENT_INSTANT0("input", "FlingController::FlingBoostStart",
                           TRACE_EVENT_SCOPE_THREAD);
    } else if (gesture_event.event.GetType() ==
                   WebInputEvent::kGestureScrollBegin ||
               gesture_event.event.GetType() ==
                   WebInputEvent::kGestureScrollUpdate) {
      TRACE_EVENT_INSTANT0("input",
                           "FlingController::ExtendBoostedFlingTimeout",
                           TRACE_EVENT_SCOPE_THREAD);
    }
  }

  return should_filter_event;
}

bool FlingController::ObserveAndMaybeConsumeGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (!ShouldForwardForGFCFiltering(gesture_event) ||
      !ShouldForwardForTapSuppression(gesture_event) ||
      FilterGestureEventForFlingBoosting(gesture_event))
    return true;

  if (gesture_event.event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    last_seen_scroll_update_ = gesture_event.event.TimeStamp();
  } else if (gesture_event.event.GetType() ==
                 WebInputEvent::kGestureScrollEnd ||
             gesture_event.event.GetType() ==
                 WebInputEvent::kGestureScrollBegin) {
    // TODO(bokan): We reset this on Begin as well as End since there appear to
    // be cases where we see an invalid event sequence:
    // https://crbug.com/928569.
    last_seen_scroll_update_ = base::TimeTicks();
  }

  // fling_controller_ is in charge of handling GFS events and the events are
  // not sent to the renderer, the controller processes the fling and generates
  // fling progress events (wheel events for touchpad and GSU events for
  // touchscreen and autoscroll) which are handled normally.
  if (gesture_event.event.GetType() == WebInputEvent::kGestureFlingStart) {
    ProcessGestureFlingStart(gesture_event);
    return true;
  }

  // If the GestureFlingStart event is processed by the fling_controller_, the
  // GestureFlingCancel event should be the same.
  if (gesture_event.event.GetType() == WebInputEvent::kGestureFlingCancel) {
    ProcessGestureFlingCancel(gesture_event);
    return true;
  }

  return false;
}

void FlingController::ProcessGestureFlingStart(
    const GestureEventWithLatencyInfo& gesture_event) {
  const float vx = gesture_event.event.data.fling_start.velocity_x;
  const float vy = gesture_event.event.data.fling_start.velocity_y;
  if (!UpdateCurrentFlingState(gesture_event.event, gfx::Vector2dF(vx, vy)))
    return;

  TRACE_EVENT_ASYNC_BEGIN2("input", kFlingTraceName, this, "vx", vx, "vy", vy);

  has_fling_animation_started_ = false;
  last_progress_time_ = base::TimeTicks();
  fling_in_progress_ = true;
  fling_booster_ = std::make_unique<ui::FlingBooster>(
      current_fling_parameters_.velocity,
      current_fling_parameters_.source_device,
      current_fling_parameters_.modifiers);
  // Wait for BeginFrame to call ProgressFling when
  // SetNeedsBeginFrameForFlingProgress is used to progress flings instead of
  // compositor animation observer (happens on Android WebView).
  if (scheduler_client_->NeedsBeginFrameForFlingProgress())
    ScheduleFlingProgress();
  else
    ProgressFling(clock_->NowTicks());
}

void FlingController::ScheduleFlingProgress() {
  scheduler_client_->ScheduleFlingProgress(weak_ptr_factory_.GetWeakPtr());
}

void FlingController::ProcessGestureFlingCancel(
    const GestureEventWithLatencyInfo& gesture_event) {
  fling_in_progress_ = false;

  if (fling_curve_)
    CancelCurrentFling();
}

void FlingController::ProgressFling(base::TimeTicks current_time) {
  if (!fling_curve_)
    return;

  TRACE_EVENT_ASYNC_STEP_INTO0("input", kFlingTraceName, this, "ProgressFling");
  DCHECK(fling_booster_);
  fling_booster_->set_last_fling_animation_time(
      (current_time - base::TimeTicks()).InSecondsF());
  if (fling_booster_->MustCancelDeferredFling()) {
    CancelCurrentFling();
    return;
  }

  if (!has_fling_animation_started_) {
    // Guard against invalid as there are no guarantees fling event and progress
    // timestamps are compatible.
    if (current_fling_parameters_.start_time.is_null()) {
      current_fling_parameters_.start_time = current_time;
      ScheduleFlingProgress();
      return;
    }

    // If the first time that progressFling is called is more than two frames
    // later than the fling start time, delay the fling start time to one frame
    // prior to the current time. This makes sure that at least one progress
    // event is sent while the fling is active even when the fling duration is
    // short (small velocity) and the time delta between its timestamp and its
    // processing time is big (e.g. When a GFS gets bubbled from an oopif).
    if (current_time >= current_fling_parameters_.start_time +
                            kMaxMicrosecondsFromFlingTimestampToFirstProgress) {
      current_fling_parameters_.start_time = current_time - kFrameDelta;
    }
  }

  // ProgressFling is called inside FlingScheduler::OnAnimationStep. Sometimes
  // the first OnAnimationStep call has the time of the last frame before
  // AddAnimationObserver call rather than time of the first frame after
  // AddAnimationObserver call. Do not advance the fling when current_time is
  // less than last fling progress time or less than the GFS event timestamp.
  if (current_time < last_progress_time_ ||
      current_time <= current_fling_parameters_.start_time) {
    ScheduleFlingProgress();
    return;
  }

  gfx::Vector2dF delta_to_scroll;
  bool fling_is_active = fling_curve_->Advance(
      (current_time - current_fling_parameters_.start_time).InSecondsF(),
      current_fling_parameters_.velocity, delta_to_scroll);
  if (fling_is_active) {
    if (std::abs(delta_to_scroll.x()) > kMinInertialScrollDelta ||
        std::abs(delta_to_scroll.y()) > kMinInertialScrollDelta) {
      GenerateAndSendFlingProgressEvents(delta_to_scroll);
      has_fling_animation_started_ = true;
      last_progress_time_ = current_time;
    }
    // As long as the fling curve is active, the fling progress must get
    // scheduled even when the last delta to scroll was zero.
    ScheduleFlingProgress();
    return;
  }

  if (current_fling_parameters_.source_device !=
      blink::WebGestureDevice::kSyntheticAutoscroll) {
    CancelCurrentFling();
  }
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
      clock_->NowTicks(), ui::LatencyInfo(ui::SourceEventType::WHEEL));
  synthetic_wheel.event.delta_x = delta.x();
  synthetic_wheel.event.delta_y = delta.y();
  synthetic_wheel.event.has_precise_scrolling_deltas = true;
  synthetic_wheel.event.momentum_phase = phase;
  synthetic_wheel.event.has_synthetic_phase = true;
  synthetic_wheel.event.SetPositionInWidget(current_fling_parameters_.point);
  synthetic_wheel.event.SetPositionInScreen(
      current_fling_parameters_.global_point);
  // Send wheel events nonblocking.
  synthetic_wheel.event.dispatch_type = WebInputEvent::kEventNonBlocking;

  event_sender_client_->SendGeneratedWheelEvent(synthetic_wheel);
}

void FlingController::GenerateAndSendGestureScrollEvents(
    WebInputEvent::Type type,
    const gfx::Vector2dF& delta /* = gfx::Vector2dF() */) {
  GestureEventWithLatencyInfo synthetic_gesture(
      type, current_fling_parameters_.modifiers, clock_->NowTicks(),
      ui::LatencyInfo(ui::SourceEventType::INERTIAL));
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
    synthetic_gesture.event.data.scroll_end.generated_by_fling_controller =
        true;
  }
  event_sender_client_->SendGeneratedGestureScrollEvents(synthetic_gesture);
}

void FlingController::GenerateAndSendFlingProgressEvents(
    const gfx::Vector2dF& delta) {
  switch (current_fling_parameters_.source_device) {
    case blink::WebGestureDevice::kTouchpad: {
      blink::WebMouseWheelEvent::Phase phase =
          has_fling_animation_started_
              ? blink::WebMouseWheelEvent::kPhaseChanged
              : blink::WebMouseWheelEvent::kPhaseBegan;
      GenerateAndSendWheelEvents(delta, phase);
      break;
    }
    case blink::WebGestureDevice::kTouchscreen:
    case blink::WebGestureDevice::kSyntheticAutoscroll:
      GenerateAndSendGestureScrollEvents(WebInputEvent::kGestureScrollUpdate,
                                         delta);
      break;
    case blink::WebGestureDevice::kUninitialized:
    case blink::WebGestureDevice::kScrollbar:
      NOTREACHED()
          << "Fling controller doesn't handle flings with source device:"
          << static_cast<int>(current_fling_parameters_.source_device);
  }
}

void FlingController::GenerateAndSendFlingEndEvents() {
  switch (current_fling_parameters_.source_device) {
    case blink::WebGestureDevice::kTouchpad:
      GenerateAndSendWheelEvents(gfx::Vector2d(),
                                 blink::WebMouseWheelEvent::kPhaseEnded);
      break;
    case blink::WebGestureDevice::kTouchscreen:
    case blink::WebGestureDevice::kSyntheticAutoscroll:
      GenerateAndSendGestureScrollEvents(WebInputEvent::kGestureScrollEnd);
      break;
    case blink::WebGestureDevice::kUninitialized:
    case blink::WebGestureDevice::kScrollbar:
      NOTREACHED()
          << "Fling controller doesn't handle flings with source device:"
          << static_cast<int>(current_fling_parameters_.source_device);
  }
}

void FlingController::CancelCurrentFling() {
  bool had_active_fling = !!fling_curve_;
  fling_curve_.reset();
  has_fling_animation_started_ = false;
  last_progress_time_ = base::TimeTicks();
  fling_in_progress_ = false;

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
  // it will get filtered by the booster again. This is necessary for
  // touchscreen fling cancelation only, since autoscroll fling cancelation
  // doesn't get deferred and when the touchpad fling cancelation gets deferred,
  // the first wheel event after the cancelation will cause a GSB generation.
  if (fling_cancellation_is_deferred &&
      last_fling_boost_event.SourceDevice() ==
          blink::WebGestureDevice::kTouchscreen &&
      (last_fling_boost_event.GetType() == WebInputEvent::kGestureScrollBegin ||
       last_fling_boost_event.GetType() ==
           WebInputEvent::kGestureScrollUpdate)) {
    WebGestureEvent scroll_begin_event = last_fling_boost_event;
    scroll_begin_event.SetType(WebInputEvent::kGestureScrollBegin);
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

    event_sender_client_->SendGeneratedGestureScrollEvents(
        GestureEventWithLatencyInfo(
            scroll_begin_event,
            ui::LatencyInfo(ui::SourceEventType::INERTIAL)));
  }

  if (had_active_fling) {
    scheduler_client_->DidStopFlingingOnBrowser(weak_ptr_factory_.GetWeakPtr());
    TRACE_EVENT_ASYNC_END0("input", kFlingTraceName, this);
  }
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

  if (fling_start_event.SourceDevice() ==
          blink::WebGestureDevice::kSyntheticAutoscroll ||
      last_seen_scroll_update_.is_null()) {
    current_fling_parameters_.start_time = fling_start_event.TimeStamp();
  } else {
    // To maintain a smooth, continuous transition from a drag scroll to a fling
    // scroll, the animation should begin at the time of the last update.
    current_fling_parameters_.start_time = last_seen_scroll_update_;
  }

  if (velocity.IsZero() && fling_start_event.SourceDevice() !=
                               blink::WebGestureDevice::kSyntheticAutoscroll) {
    CancelCurrentFling();
    return false;
  }

  fling_curve_ = std::unique_ptr<blink::WebGestureCurve>(
      ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
          current_fling_parameters_.source_device,
          current_fling_parameters_.velocity,
          gfx::Vector2dF() /*initial_offset*/, false /*on_main_thread*/,
          GetContentClient()->browser()->ShouldUseMobileFlingCurve()));
  return true;
}

bool FlingController::FlingCancellationIsDeferred() const {
  return fling_booster_ && fling_booster_->fling_cancellation_is_deferred();
}

gfx::Vector2dF FlingController::CurrentFlingVelocity() const {
  return current_fling_parameters_.velocity;
}

TouchpadTapSuppressionController*
FlingController::GetTouchpadTapSuppressionController() {
  return &touchpad_tap_suppression_controller_;
}

}  // namespace content
