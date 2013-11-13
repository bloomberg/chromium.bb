// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture_new.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class SyntheticGestureTarget;

// Controls a synthetic gesture.
// Repeatedly invokes the gesture object's ForwardInputEvent method to send
// input events to the platform until the gesture has finished.
class CONTENT_EXPORT SyntheticGestureControllerNew {
 public:
  explicit SyntheticGestureControllerNew(
      scoped_ptr<SyntheticGestureTarget> gesture_target);
  virtual ~SyntheticGestureControllerNew();

  void QueueSyntheticGesture(
      scoped_ptr<SyntheticGestureNew> synthetic_gesture);

  // Forward input events of the currently processed gesture.
  void Flush(base::TimeTicks timestamp);

 private:
  void StartGesture(const SyntheticGestureNew& gesture);
  void StopGesture(const SyntheticGestureNew& gesture,
                   SyntheticGestureNew::Result result);

  scoped_ptr<SyntheticGestureTarget> gesture_target_;
  ScopedVector<SyntheticGestureNew> pending_gesture_queue_;

  base::TimeTicks last_tick_time_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureControllerNew);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
