// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer_action.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticPointerAction::SyntheticPointerAction(
    SyntheticGestureParams::GestureSourceType gesture_source_type,
    PointerActionType pointer_action_type,
    SyntheticPointer* synthetic_pointer,
    gfx::PointF position,
    int index)
    : gesture_source_type_(gesture_source_type),
      pointer_action_type_(pointer_action_type),
      position_(position),
      index_(index),
      synthetic_pointer_(synthetic_pointer) {}

SyntheticPointerAction::~SyntheticPointerAction() {}

SyntheticGesture::Result SyntheticPointerAction::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
    gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);

  ForwardTouchOrMouseInputEvents(timestamp, target);
  return SyntheticGesture::GESTURE_FINISHED;
}

void SyntheticPointerAction::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (pointer_action_type_) {
    case SyntheticGesture::PRESS:
      synthetic_pointer_->Press(position_.x(), position_.y(), target,
                                timestamp);
      break;
    case SyntheticGesture::MOVE:
      synthetic_pointer_->Move(index_, position_.x(), position_.y(), target,
                               timestamp);
      break;
    case SyntheticGesture::RELEASE:
      synthetic_pointer_->Release(index_, target, timestamp);
      break;
  }
  synthetic_pointer_->DispatchEvent(target, timestamp);
}

}  // namespace content
