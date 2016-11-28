// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pointer_action_params.h"

using blink::WebTouchEvent;

namespace content {

class CONTENT_EXPORT SyntheticPointerAction : public SyntheticGesture {
 public:
  SyntheticPointerAction(std::vector<SyntheticPointerActionParams>* param_list,
                         SyntheticPointerDriver* synthetic_pointer_driver);
  ~SyntheticPointerAction() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;

 private:
  explicit SyntheticPointerAction(const SyntheticPointerActionParams& params);
  SyntheticGesture::Result ForwardTouchOrMouseInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target);

  // SyntheticGestureController is responsible to create the
  // SyntheticPointerActions and control when to forward them.

  // These two objects will be owned by SyntheticGestureController, which
  // will manage their lifetime by initiating them when it starts processing a
  // pointer action sequence and resetting them when it finishes.
  // param_list_ contains a list of pointer actions which will be dispatched
  // together.
  std::vector<SyntheticPointerActionParams>* param_list_;
  SyntheticPointerDriver* synthetic_pointer_driver_;

  SyntheticPointerActionParams params_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticPointerAction);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
