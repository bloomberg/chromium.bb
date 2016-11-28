// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer_action.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticPointerAction::SyntheticPointerAction(
    const SyntheticPointerActionParams& params)
    : params_(params) {}

SyntheticPointerAction::SyntheticPointerAction(
    std::vector<SyntheticPointerActionParams>* param_list,
    SyntheticPointerDriver* synthetic_pointer_driver)
    : param_list_(param_list),
      synthetic_pointer_driver_(synthetic_pointer_driver) {}

SyntheticPointerAction::~SyntheticPointerAction() {}

SyntheticGesture::Result SyntheticPointerAction::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  DCHECK(synthetic_pointer_driver_);
  return ForwardTouchOrMouseInputEvents(timestamp, target);
}

SyntheticGesture::Result SyntheticPointerAction::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  for (SyntheticPointerActionParams& params : *param_list_) {
    if (!synthetic_pointer_driver_->UserInputCheck(params))
      return POINTER_ACTION_INPUT_INVALID;

    switch (params.pointer_action_type()) {
      case SyntheticPointerActionParams::PointerActionType::PRESS: {
        int index = synthetic_pointer_driver_->Press(params.position().x(),
                                                     params.position().y());
        params.set_index(index);
        break;
      }
      case SyntheticPointerActionParams::PointerActionType::MOVE:
        synthetic_pointer_driver_->Move(params.position().x(),
                                        params.position().y(), params.index());
        break;
      case SyntheticPointerActionParams::PointerActionType::RELEASE:
        synthetic_pointer_driver_->Release(params.index());
        // Only reset the index for touch pointers.
        if (params.gesture_source_type != SyntheticGestureParams::MOUSE_INPUT)
          params.set_index(-1);
        break;
      case SyntheticPointerActionParams::PointerActionType::IDLE:
        break;
      case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
        return POINTER_ACTION_INPUT_INVALID;
      case SyntheticPointerActionParams::PointerActionType::FINISH:
        return GESTURE_FINISHED;
    }
  }
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
  return GESTURE_FINISHED;
}

}  // namespace content
