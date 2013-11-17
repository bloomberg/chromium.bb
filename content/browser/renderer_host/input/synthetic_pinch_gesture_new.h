// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_

#include "content/browser/renderer_host/input/synthetic_gesture_new.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_web_input_event_builders.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class CONTENT_EXPORT SyntheticPinchGestureNew : public SyntheticGestureNew {
 public:
  explicit SyntheticPinchGestureNew(const SyntheticPinchGestureParams& params);
  virtual ~SyntheticPinchGestureNew();

  virtual Result ForwardInputEvents(const base::TimeDelta& interval,
                                    SyntheticGestureTarget* target) OVERRIDE;

 private:
  SyntheticPinchGestureParams params_;
  float current_y_0_;
  float current_y_1_;
  float target_y_0_;
  float target_y_1_;
  bool started_;
  SyntheticWebTouchEvent touch_event_;

  SyntheticGestureNew::Result ForwardTouchInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target);

  void ForwardTouchEvent(SyntheticGestureTarget* target);

  float GetPositionDelta(const base::TimeDelta& interval);

  bool HasFinished();

  DISALLOW_COPY_AND_ASSIGN(SyntheticPinchGestureNew);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_
