// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/web_gesture_curve_mock.h"

#include "content/test/weburl_loader_mock_factory.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"

WebGestureCurveMock::WebGestureCurveMock(const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll)
    : velocity_(velocity),
      cumulative_scroll_(cumulative_scroll) {
}

WebGestureCurveMock::~WebGestureCurveMock() {
}

bool WebGestureCurveMock::apply(double time,
                                blink::WebGestureCurveTarget* target) {
  blink::WebSize displacement(velocity_.x * time, velocity_.y * time);
  blink::WebFloatSize increment(displacement.width - cumulative_scroll_.width,
      displacement.height - cumulative_scroll_.height);
  cumulative_scroll_ = displacement;
  blink::WebFloatSize velocity(velocity_.x, velocity_.y);
  // scrollBy() could delete this curve if the animation is over, so don't
  // touch any member variables after making that call.
  target->scrollBy(increment, velocity);
  return true;
}
