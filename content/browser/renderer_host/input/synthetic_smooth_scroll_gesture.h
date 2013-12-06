// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_

#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"

namespace content {

class CONTENT_EXPORT SyntheticSmoothScrollGesture : public SyntheticGesture {
 public:
  explicit SyntheticSmoothScrollGesture(
      const SyntheticSmoothScrollGestureParams& params);
  virtual ~SyntheticSmoothScrollGesture();

  virtual SyntheticGesture::Result ForwardInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target) OVERRIDE;

 private:
  enum GestureState {
    SETUP,
    STARTED,
    MOVING,
    STOPPING,
    DONE
  };

  void ForwardTouchInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target);
  void ForwardMouseInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target);

  void ForwardTouchEvent(SyntheticGestureTarget* target) const;
  void ForwardMouseWheelEvent(SyntheticGestureTarget* target,
                              const gfx::Vector2dF& delta) const;

  void PressTouchPoint(SyntheticGestureTarget* target);
  void MoveTouchPoint(SyntheticGestureTarget* target);
  void ReleaseTouchPoint(SyntheticGestureTarget* target);

  void AddTouchSlopToDistance(SyntheticGestureTarget* target);
  gfx::Vector2dF GetPositionDelta(const base::TimeDelta& interval) const;
  gfx::Vector2dF ProjectLengthOntoScrollDirection(float delta_length) const;
  gfx::Vector2dF ComputeRemainingDelta() const;
  bool HasScrolledEntireDistance() const;

  SyntheticSmoothScrollGestureParams params_;
  gfx::Vector2dF total_delta_;
  gfx::Vector2d total_delta_discrete_;
  SyntheticWebTouchEvent touch_event_;
  SyntheticGestureParams::GestureSourceType gesture_source_type_;
  GestureState state_;
  base::TimeDelta total_stopping_wait_time_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticSmoothScrollGesture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
