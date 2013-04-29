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
    : rwh_(NULL) {
}

SmoothScrollGestureController::~SmoothScrollGestureController() {
}

void SmoothScrollGestureController::BeginSmoothScroll(
    RenderWidgetHostViewPort* view,
    const ViewHostMsg_BeginSmoothScroll_Params& params) {
  if (pending_smooth_scroll_gesture_)
    return;

  rwh_ = view->GetRenderWidgetHost();
  pending_smooth_scroll_gesture_ = view->CreateSmoothScrollGesture(
      params.scroll_down,
      params.pixels_to_scroll,
      params.mouse_event_x,
      params.mouse_event_y);

  timer_.Start(FROM_HERE, GetSyntheticScrollMessageInterval(), this,
               &SmoothScrollGestureController::OnTimer);
}

base::TimeDelta
    SmoothScrollGestureController::GetSyntheticScrollMessageInterval() const {
  return base::TimeDelta::FromMilliseconds(kSyntheticScrollMessageIntervalMs);
}

void SmoothScrollGestureController::OnTimer() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!pending_smooth_scroll_gesture_->ForwardInputEvents(now, rwh_)) {
    timer_.Stop();
    pending_smooth_scroll_gesture_ = NULL;
    rwh_->Send(new ViewMsg_SmoothScrollCompleted(rwh_->GetRoutingID()));
  }
}

}  // namespace content
