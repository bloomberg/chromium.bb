// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/smooth_scroll_gesture_controller.h"

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/port/browser/smooth_scroll_gesture.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

namespace {

// How many milliseconds apart synthetic scroll messages should be sent.
const int kSyntheticScrollMessageIntervalMs = 8;

}  // namespace

SmoothScrollGestureController::SmoothScrollGestureController()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      pending_input_event_count_(0),
      tick_active_smooth_scroll_gesture_task_posted_(false) {
}

SmoothScrollGestureController::~SmoothScrollGestureController() {
}

bool SmoothScrollGestureController::OnBeginSmoothScroll(
    RenderWidgetHostViewPort* view,
    const ViewHostMsg_BeginSmoothScroll_Params& params) {
  if (pending_smooth_scroll_gesture_)
    return false;
  pending_smooth_scroll_gesture_ = view->CreateSmoothScrollGesture(
      params.scroll_down, params.pixels_to_scroll,
      params.mouse_event_x, params.mouse_event_y);

  // If an input ack is pending, then hold off ticking the gesture
  // until we get an input ack.
  if (pending_input_event_count_)
    return true;
  if (tick_active_smooth_scroll_gesture_task_posted_)
    return true;
  TickActiveSmoothScrollGesture(view->GetRenderWidgetHost());
  return true;
}

void SmoothScrollGestureController::OnForwardInputEvent() {
  ++pending_input_event_count_;
}

void SmoothScrollGestureController::OnInputEventACK(RenderWidgetHost* rwh) {
  if (pending_input_event_count_ > 0)
    --pending_input_event_count_;
  // If an input ack is pending, then hold off ticking the gesture
  // until we get an input ack.
  if (!pending_input_event_count_ && pending_smooth_scroll_gesture_)
    TickActiveSmoothScrollGesture(rwh);
}

int SmoothScrollGestureController::SyntheticScrollMessageInterval() const {
  return kSyntheticScrollMessageIntervalMs;
}

void SmoothScrollGestureController::TickActiveSmoothScrollGesture(
    RenderWidgetHost* rwh) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::TickActiveSmoothScrollGesture");
  tick_active_smooth_scroll_gesture_task_posted_ = false;
  if (!pending_smooth_scroll_gesture_) {
    TRACE_EVENT_INSTANT0("input", "EarlyOut_NoActiveScrollGesture",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  base::TimeTicks now = base::TimeTicks::HighResNow();
  base::TimeDelta preferred_interval =
      base::TimeDelta::FromMilliseconds(kSyntheticScrollMessageIntervalMs);
  base::TimeDelta time_until_next_ideal_interval =
      (last_smooth_scroll_gesture_tick_time_ + preferred_interval) -
      now;
  if (time_until_next_ideal_interval.InMilliseconds() > 0) {
    TRACE_EVENT_INSTANT1(
        "input", "EarlyOut_TickedTooRecently", TRACE_EVENT_SCOPE_THREAD,
        "delay", time_until_next_ideal_interval.InMilliseconds());
    // Post a task.
    tick_active_smooth_scroll_gesture_task_posted_ = true;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &SmoothScrollGestureController::TickActiveSmoothScrollGesture,
            weak_factory_.GetWeakPtr(),
            rwh),
        time_until_next_ideal_interval);
    return;
  }

  last_smooth_scroll_gesture_tick_time_ = now;


  if (!pending_smooth_scroll_gesture_->ForwardInputEvents(now, rwh)) {
    pending_smooth_scroll_gesture_ = NULL;
    rwh->Send(new ViewMsg_SmoothScrollCompleted(rwh->GetRoutingID()));
  }

  // No need to post the next tick if an input is in flight.
  if (pending_input_event_count_)
    return;

  TRACE_EVENT_INSTANT1("input", "PostTickTask", TRACE_EVENT_SCOPE_THREAD,
                       "delay", preferred_interval.InMilliseconds());
  tick_active_smooth_scroll_gesture_task_posted_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SmoothScrollGestureController::TickActiveSmoothScrollGesture,
                 weak_factory_.GetWeakPtr(),
                 rwh),
      preferred_interval);
}

}  // namespace content
