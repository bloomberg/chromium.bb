// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_

#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class CONTENT_EXPORT SyntheticPinchGesture : public SyntheticGesture {
 public:
  explicit SyntheticPinchGesture(const SyntheticPinchGestureParams& params);
  virtual ~SyntheticPinchGesture();

  virtual SyntheticGesture::Result ForwardInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target) OVERRIDE;

 private:
  enum GestureState {
    SETUP,
    STARTED,
    MOVING,
    DONE
  };

  void ForwardTouchInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target);

  void UpdateTouchPoints(base::TimeDelta interval);
  void PressTouchPoints(SyntheticGestureTarget* target);
  void MoveTouchPoints(SyntheticGestureTarget* target);
  void ReleaseTouchPoints(SyntheticGestureTarget* target);
  void ForwardTouchEvent(SyntheticGestureTarget* target) const;

  void SetupCoordinates(SyntheticGestureTarget* target);
  float GetDeltaForPointer0(const base::TimeDelta& interval) const;
  float ComputeAbsoluteRemainingDistance() const;
  bool HasReachedTarget() const;

  SyntheticPinchGestureParams params_;
  float current_y_0_;
  float current_y_1_;
  float target_y_0_;
  float target_y_1_;
  SyntheticGestureParams::GestureSourceType gesture_source_type_;
  GestureState state_;
  SyntheticWebTouchEvent touch_event_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticPinchGesture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_
