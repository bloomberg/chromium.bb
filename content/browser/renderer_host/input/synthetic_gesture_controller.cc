// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"

#include "base/debug/trace_event.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input_messages.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

SyntheticGestureController::SyntheticGestureController(
    scoped_ptr<SyntheticGestureTarget> gesture_target)
    : gesture_target_(gesture_target.Pass()) {}

SyntheticGestureController::~SyntheticGestureController() {}

void SyntheticGestureController::QueueSyntheticGesture(
    scoped_ptr<SyntheticGesture> synthetic_gesture) {
  DCHECK(synthetic_gesture);

  pending_gesture_queue_.push_back(synthetic_gesture.release());

  // Start forwarding input events if the queue was previously empty.
  if (pending_gesture_queue_.size() == 1)
    StartGesture(*pending_gesture_queue_.front());
}

void SyntheticGestureController::Flush(base::TimeTicks timestamp) {
  TRACE_EVENT0("benchmark", "SyntheticGestureController::Flush");
  if (pending_gesture_queue_.empty())
    return;

  SyntheticGesture::Result result =
      pending_gesture_queue_.front()->ForwardInputEvents(timestamp,
                                                         gesture_target_.get());

  if (result == SyntheticGesture::GESTURE_RUNNING) {
    gesture_target_->SetNeedsFlush();
    return;
  }

  StopGesture(*pending_gesture_queue_.front(), result);
  pending_gesture_queue_.erase(pending_gesture_queue_.begin());

  if (!pending_gesture_queue_.empty())
    StartGesture(*pending_gesture_queue_.front());
}

void SyntheticGestureController::StartGesture(const SyntheticGesture& gesture) {
  TRACE_EVENT_ASYNC_BEGIN0("benchmark", "SyntheticGestureController::running",
                           &gesture);
  gesture_target_->SetNeedsFlush();
}

void SyntheticGestureController::StopGesture(
    const SyntheticGesture& gesture, SyntheticGesture::Result result) {
  DCHECK_NE(result, SyntheticGesture::GESTURE_RUNNING);
  TRACE_EVENT_ASYNC_END0("benchmark", "SyntheticGestureController::running",
                         &gesture);

  gesture_target_->OnSyntheticGestureCompleted(result);
}

}  // namespace content
