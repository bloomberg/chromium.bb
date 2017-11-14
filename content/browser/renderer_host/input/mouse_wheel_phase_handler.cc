// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/events/base_event_utils.h"

namespace content {
MouseWheelPhaseHandler::MouseWheelPhaseHandler(
    RenderWidgetHostImpl* const host,
    RenderWidgetHostViewBase* const host_view)
    : host_(RenderWidgetHostImpl::From(host)),
      host_view_(host_view),
      scroll_phase_state_(SCROLL_STATE_UNKNOWN) {}

void MouseWheelPhaseHandler::AddPhaseIfNeededAndScheduleEndEvent(
    blink::WebMouseWheelEvent& mouse_wheel_event,
    bool should_route_event) {

  bool has_phase =
      mouse_wheel_event.phase != blink::WebMouseWheelEvent::kPhaseNone ||
      mouse_wheel_event.momentum_phase != blink::WebMouseWheelEvent::kPhaseNone;
  if (has_phase) {
    if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
      // Don't send the wheel end event immediately, start a timer instead to
      // see whether momentum phase of the scrolling starts or not.
      ScheduleMouseWheelEndDispatching(should_route_event);
    } else if (mouse_wheel_event.phase ==
               blink::WebMouseWheelEvent::kPhaseBegan) {
      // A new scrolling sequence has started, send the pending wheel end
      // event to end the previous scrolling sequence.
      DispatchPendingWheelEndEvent();
    } else if (mouse_wheel_event.momentum_phase ==
               blink::WebMouseWheelEvent::kPhaseBegan) {
      // Momentum phase has started, drop the pending wheel end event to make
      // sure that no wheel end event will be sent during the momentum phase
      // of scrolling.
      IgnorePendingWheelEndEvent();
    }
  } else {  // !has_phase
    switch (scroll_phase_state_) {
      case SCROLL_STATE_UNKNOWN: {
        mouse_wheel_event.has_synthetic_phase = true;
        if (!mouse_wheel_end_dispatch_timer_.IsRunning()) {
          mouse_wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
          ScheduleMouseWheelEndDispatching(should_route_event);
        } else {  // mouse_wheel_end_dispatch_timer_.IsRunning()
          bool non_zero_delta =
              mouse_wheel_event.delta_x || mouse_wheel_event.delta_y;
          mouse_wheel_event.phase =
              non_zero_delta ? blink::WebMouseWheelEvent::kPhaseChanged
                             : blink::WebMouseWheelEvent::kPhaseStationary;
          mouse_wheel_end_dispatch_timer_.Reset();
        }
        break;
      }
      case SCROLL_MAY_BEGIN:
        mouse_wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
        scroll_phase_state_ = SCROLL_IN_PROGRESS;
        break;
      case SCROLL_IN_PROGRESS:
        mouse_wheel_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
        break;
      default:
        NOTREACHED();
    }
  }

  last_mouse_wheel_event_ = mouse_wheel_event;
}

void MouseWheelPhaseHandler::DispatchPendingWheelEndEvent() {
  if (!mouse_wheel_end_dispatch_timer_.IsRunning())
    return;

  base::Closure task = mouse_wheel_end_dispatch_timer_.user_task();
  mouse_wheel_end_dispatch_timer_.Stop();
  task.Run();
}

void MouseWheelPhaseHandler::IgnorePendingWheelEndEvent() {
  mouse_wheel_end_dispatch_timer_.Stop();
}

void MouseWheelPhaseHandler::ResetScrollSequence() {
  scroll_phase_state_ = SCROLL_STATE_UNKNOWN;
}

void MouseWheelPhaseHandler::SendWheelEndIfNeeded() {
  if (scroll_phase_state_ == SCROLL_IN_PROGRESS) {
    DCHECK(host_);
    bool should_route_event =
        host_->delegate() && host_->delegate()->GetInputEventRouter();
    SendSyntheticWheelEventWithPhaseEnded(should_route_event);
  }

  ResetScrollSequence();
}

void MouseWheelPhaseHandler::ScrollingMayBegin() {
  scroll_phase_state_ = SCROLL_MAY_BEGIN;
}

void MouseWheelPhaseHandler::SendSyntheticWheelEventWithPhaseEnded(
    bool should_route_event) {
  DCHECK(host_view_->wheel_scroll_latching_enabled());
  last_mouse_wheel_event_.SetTimeStampSeconds(
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  last_mouse_wheel_event_.delta_x = 0;
  last_mouse_wheel_event_.delta_y = 0;
  last_mouse_wheel_event_.wheel_ticks_x = 0;
  last_mouse_wheel_event_.wheel_ticks_y = 0;
  last_mouse_wheel_event_.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  last_mouse_wheel_event_.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;

  if (should_route_event) {
    host_->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
        host_view_, &last_mouse_wheel_event_,
        ui::LatencyInfo(ui::SourceEventType::WHEEL));
  } else {
    host_view_->ProcessMouseWheelEvent(
        last_mouse_wheel_event_, ui::LatencyInfo(ui::SourceEventType::WHEEL));
  }
}

void MouseWheelPhaseHandler::ScheduleMouseWheelEndDispatching(
    bool should_route_event) {
  mouse_wheel_end_dispatch_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          kDefaultMouseWheelLatchingTransactionMs),
      base::Bind(&MouseWheelPhaseHandler::SendSyntheticWheelEventWithPhaseEnded,
                 base::Unretained(this), should_route_event));
}

}  // namespace content
