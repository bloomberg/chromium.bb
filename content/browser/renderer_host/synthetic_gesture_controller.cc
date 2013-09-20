// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/synthetic_gesture_controller.h"

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/port/browser/synthetic_gesture.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

namespace {

// How many milliseconds apart synthetic scroll messages should be sent.
const int kSyntheticGestureMessageIntervalMs = 7;

}  // namespace

SyntheticGestureController::SyntheticGestureController()
    : rwh_(NULL) {
}

SyntheticGestureController::~SyntheticGestureController() {
}

void SyntheticGestureController::BeginSmoothScroll(
    RenderWidgetHostViewPort* view,
    const ViewHostMsg_BeginSmoothScroll_Params& params) {
  if (pending_synthetic_gesture_.get())
    return;

  rwh_ = view->GetRenderWidgetHost();
  pending_synthetic_gesture_ = view->CreateSmoothScrollGesture(
      params.scroll_down,
      params.pixels_to_scroll,
      params.mouse_event_x,
      params.mouse_event_y);

  TRACE_EVENT_ASYNC_BEGIN0("benchmark", "SyntheticGestureController::running",
      pending_synthetic_gesture_);
  timer_.Start(FROM_HERE, GetSyntheticGestureMessageInterval(), this,
               &SyntheticGestureController::OnTimer);
}

void SyntheticGestureController::BeginPinch(
    RenderWidgetHostViewPort* view,
    const ViewHostMsg_BeginPinch_Params& params) {
  if (pending_synthetic_gesture_.get())
    return;

  rwh_ = view->GetRenderWidgetHost();
  pending_synthetic_gesture_ = view->CreatePinchGesture(
      params.zoom_in,
      params.pixels_to_move,
      params.anchor_x,
      params.anchor_y);

  TRACE_EVENT_ASYNC_BEGIN0("benchmark", "SyntheticGestureController::running",
      pending_synthetic_gesture_);
  timer_.Start(FROM_HERE, GetSyntheticGestureMessageInterval(), this,
               &SyntheticGestureController::OnTimer);
}

base::TimeDelta
    SyntheticGestureController::GetSyntheticGestureMessageInterval() const {
  return base::TimeDelta::FromMilliseconds(kSyntheticGestureMessageIntervalMs);
}

void SyntheticGestureController::OnTimer() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!pending_synthetic_gesture_->ForwardInputEvents(now, rwh_)) {
    timer_.Stop();
    TRACE_EVENT_ASYNC_END0("benchmark", "SyntheticGestureController::running",
        pending_synthetic_gesture_);
    pending_synthetic_gesture_ = NULL;
    rwh_->Send(new ViewMsg_SyntheticGestureCompleted(rwh_->GetRoutingID()));
  }
}

}  // namespace content
