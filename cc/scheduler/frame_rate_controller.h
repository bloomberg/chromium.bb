// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_FRAME_RATE_CONTROLLER_H_
#define CC_SCHEDULER_FRAME_RATE_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "cc/base/cc_export.h"

namespace cc {

class Thread;
class TimeSource;

class CC_EXPORT FrameRateControllerClient {
 public:
  // Throttled is true when we have a maximum number of frames pending.
  virtual void VSyncTick(bool throttled) = 0;

 protected:
  virtual ~FrameRateControllerClient() {}
};

class FrameRateControllerTimeSourceAdapter;

class CC_EXPORT FrameRateController {
 public:
  enum {
    DEFAULT_MAX_FRAMES_PENDING = 2
  };

  explicit FrameRateController(scoped_refptr<TimeSource> timer);
  // Alternate form of FrameRateController with unthrottled frame-rate.
  explicit FrameRateController(Thread* thread);
  virtual ~FrameRateController();

  void SetClient(FrameRateControllerClient* client) { client_ = client; }

  void SetActive(bool active);

  // Use the following methods to adjust target frame rate.
  //
  // Multiple frames can be in-progress, but for every DidBeginFrame, a
  // DidFinishFrame should be posted.
  //
  // If the rendering pipeline crashes, call DidAbortAllPendingFrames.
  void DidBeginFrame();
  void DidFinishFrame();
  void DidAbortAllPendingFrames();
  void SetMaxFramesPending(int max_frames_pending);  // 0 for unlimited.
  int MaxFramesPending() const { return max_frames_pending_; }

  // This returns null for unthrottled frame-rate.
  base::TimeTicks NextTickTime();

  // This returns now for unthrottled frame-rate.
  base::TimeTicks LastTickTime();

  void SetTimebaseAndInterval(base::TimeTicks timebase,
                              base::TimeDelta interval);
  void SetSwapBuffersCompleteSupported(bool supported);

 protected:
  friend class FrameRateControllerTimeSourceAdapter;
  void OnTimerTick();

  void PostManualTick();
  void ManualTick();

  FrameRateControllerClient* client_;
  int num_frames_pending_;
  int max_frames_pending_;
  scoped_refptr<TimeSource> time_source_;
  scoped_ptr<FrameRateControllerTimeSourceAdapter> time_source_client_adapter_;
  bool active_;
  bool swap_buffers_complete_supported_;

  // Members for unthrottled frame-rate.
  bool is_time_source_throttling_;
  base::WeakPtrFactory<FrameRateController> weak_factory_;
  Thread* thread_;

  DISALLOW_COPY_AND_ASSIGN(FrameRateController);
};

}  // namespace cc

#endif  // CC_SCHEDULER_FRAME_RATE_CONTROLLER_H_
