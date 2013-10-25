// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class SyntheticGestureTarget;

class CONTENT_EXPORT SyntheticGestureNew {
 public:
  SyntheticGestureNew();
  virtual ~SyntheticGestureNew();

  static scoped_ptr<SyntheticGestureNew> Create(
      const SyntheticGestureParams& gesture_params);

  enum Result {
    GESTURE_RUNNING,
    GESTURE_FINISHED,
    GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED,
    GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM,
    GESTURE_RESULT_MAX = GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM
  };
  // Update the state of the gesture and forward the appropriate events to the
  // platform. This function is called repeatedly by the synthetic gesture
  // controller until it stops returning GESTURE_RUNNING.
  virtual Result ForwardInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureNew);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_
