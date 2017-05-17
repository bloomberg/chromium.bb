// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class SyntheticGestureTarget;

// Controls a synthetic gesture.
// Repeatedly invokes the gesture object's ForwardInputEvent method to send
// input events to the platform until the gesture has finished.
class CONTENT_EXPORT SyntheticGestureController {
 public:
  using BeginFrameCallback = base::OnceClosure;
  using BeginFrameRequestCallback =
      base::RepeatingCallback<void(BeginFrameCallback)>;
  SyntheticGestureController(
      std::unique_ptr<SyntheticGestureTarget> gesture_target,
      BeginFrameRequestCallback begin_frame_callback);
  virtual ~SyntheticGestureController();

  typedef base::Callback<void(SyntheticGesture::Result)>
      OnGestureCompleteCallback;
  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGesture> synthetic_gesture,
      const OnGestureCompleteCallback& completion_callback);

  bool DispatchNextEvent(base::TimeTicks = base::TimeTicks::Now());

 private:
  void OnBeginFrame();

  void StartGesture(const SyntheticGesture& gesture);
  void StopGesture(const SyntheticGesture& gesture,
                   const OnGestureCompleteCallback& completion_callback,
                   SyntheticGesture::Result result);

  std::unique_ptr<SyntheticGestureTarget> gesture_target_;

  // A queue of gesture/callback pairs.  Implemented as two queues to
  // simplify the ownership of SyntheticGesture pointers.
  class GestureAndCallbackQueue {
  public:
    GestureAndCallbackQueue();
    ~GestureAndCallbackQueue();
    void Push(std::unique_ptr<SyntheticGesture> gesture,
              const OnGestureCompleteCallback& callback) {
      gestures_.push_back(std::move(gesture));
      callbacks_.push(callback);
    }
    void Pop() {
      gestures_.erase(gestures_.begin());
      callbacks_.pop();
    }
    SyntheticGesture* FrontGesture() { return gestures_.front().get(); }
    OnGestureCompleteCallback& FrontCallback() {
      return callbacks_.front();
    }
    bool IsEmpty() {
      CHECK(gestures_.empty() == callbacks_.empty());
      return gestures_.empty();
    }
   private:
    std::vector<std::unique_ptr<SyntheticGesture>> gestures_;
    std::queue<OnGestureCompleteCallback> callbacks_;

    DISALLOW_COPY_AND_ASSIGN(GestureAndCallbackQueue);
  } pending_gesture_queue_;

  BeginFrameRequestCallback begin_frame_callback_;

  base::WeakPtrFactory<SyntheticGestureController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
