// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEB_GESTURE_CURVE_MOCK_H_
#define CONTENT_TEST_WEB_GESTURE_CURVE_MOCK_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebSize.h"

// A simple class for mocking a WebGestureCurve. The curve flings at velocity
// indefinitely.
class WebGestureCurveMock : public blink::WebGestureCurve {
 public:
  WebGestureCurveMock(const blink::WebFloatPoint& velocity,
                      const blink::WebSize& cumulative_scroll);
  virtual ~WebGestureCurveMock();

  // Returns false if curve has finished and can no longer be applied.
  virtual bool apply(double time,
                     blink::WebGestureCurveTarget* target) OVERRIDE;

 private:
  blink::WebFloatPoint velocity_;
  blink::WebSize cumulative_scroll_;

  DISALLOW_COPY_AND_ASSIGN(WebGestureCurveMock);
};

#endif  // CONTENT_TEST_WEB_GESTURE_CURVE_MOCK_H_
