// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_FRAME_RATE_CONTROLLER_H_
#define CC_SCHEDULER_FRAME_RATE_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"

namespace base { class SingleThreadTaskRunner; }

namespace cc {

class TimeSource;
class FrameRateController;

class CC_EXPORT FrameRateControllerClient {
 protected:
  virtual ~FrameRateControllerClient() {}

 public:
  // Throttled is true when we have a maximum number of frames pending.
  virtual void FrameRateControllerTick(bool throttled,
                                       const BeginFrameArgs& args) = 0;
};

class FrameRateControllerTimeSourceAdapter;

// The FrameRateController is used in cases where we self-tick (i.e. BeginFrame
// is not sent by a parent compositor.
class CC_EXPORT FrameRateController {
 public:
  explicit FrameRateController(scoped_refptr<TimeSource> timer);
  // Alternate form of FrameRateController with unthrottled frame-rate.
  explicit FrameRateController(base::SingleThreadTaskRunner* task_runner);
  virtual ~FrameRateController();

  void SetClient(FrameRateControllerClient* client) { client_ = client; }

  // Returns a valid BeginFrame on activation to potentially be used
  // retroactively.
  BeginFrameArgs SetActive(bool active);

  bool IsActive() { return active_; }

  // Use the following methods to adjust target frame rate.
  //
  // Multiple frames can be in-progress, but for every DidSwapBuffers, a
  // DidFinishFrame should be posted.
  //
  // If the rendering pipeline crashes, call DidAbortAllPendingFrames.
  void DidSwapBuffers();
  void DidSwapBuffersComplete();
  void DidAbortAllPendingFrames();
  void SetMaxSwapsPending(int max_swaps_pending);  // 0 for unlimited.
  int MaxSwapsPending() const { return max_swaps_pending_; }
  int NumSwapsPendingForTesting() const { return num_frames_pending_; }

  void SetTimebaseAndInterval(base::TimeTicks timebase,
                              base::TimeDelta interval);
  void SetDeadlineAdjustment(base::TimeDelta delta);

 protected:
  friend class FrameRateControllerTimeSourceAdapter;
  void OnTimerTick();

  void PostManualTick();
  void ManualTick();

    // This returns null for unthrottled frame-rate.
  base::TimeTicks NextTickTime();
  // This returns now for unthrottled frame-rate.
  base::TimeTicks LastTickTime();

  FrameRateControllerClient* client_;
  int num_frames_pending_;
  int max_swaps_pending_;
  base::TimeDelta interval_;
  base::TimeDelta deadline_adjustment_;
  scoped_refptr<TimeSource> time_source_;
  scoped_ptr<FrameRateControllerTimeSourceAdapter> time_source_client_adapter_;
  bool active_;

  // Members for unthrottled frame-rate.
  bool is_time_source_throttling_;
  bool manual_tick_pending_;
  base::SingleThreadTaskRunner* task_runner_;
  base::WeakPtrFactory<FrameRateController> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameRateController);
};

}  // namespace cc

#endif  // CC_SCHEDULER_FRAME_RATE_CONTROLLER_H_
