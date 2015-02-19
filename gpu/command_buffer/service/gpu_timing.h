// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_TIMING_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_TIMING_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/gpu_export.h"

namespace gfx {
class GLContext;
}

namespace gpu {
class GPUTiming;

// Class to compute the amount of time it takes to fully
// complete a set of GL commands
class GPU_EXPORT GPUTimer {
 public:
  // gpu_timing must outlive GPUTimer instance we're creating.
  explicit GPUTimer(GPUTiming* gpu_timing);
  ~GPUTimer();

  void Start();
  void End();
  bool IsAvailable();

  void GetStartEndTimestamps(int64* start, int64* end);
  int64 GetDeltaElapsed();

 private:
  unsigned int queries_[2];
  int64 offset_ = 0;
  bool end_requested_ = false;
  GPUTiming* gpu_timing_;

  DISALLOW_COPY_AND_ASSIGN(GPUTimer);
};

// GPUTiming contains all the gl timing logic that is not specific
// to a single GPUTimer.
class GPU_EXPORT GPUTiming {
 public:
  enum TimerType {
    kTimerTypeInvalid = -1,

    kTimerTypeARB,      // ARB_timer_query
    kTimerTypeDisjoint  // EXT_disjoint_timer_query
  };

  GPUTiming();
  virtual ~GPUTiming();

  bool Initialize(gfx::GLContext* context);
  bool IsAvailable();

  // CheckAndResetTimerErrors has to be called before reading timestamps
  // from GPUTimers instances and after making sure all the timers
  // were available.
  // If the returned value is false, all the previous timers should be
  // discarded.
  bool CheckAndResetTimerErrors();

  const char* GetTimerTypeName() const;

  // Returns the offset between the current gpu time and the cpu time.
  int64 CalculateTimerOffset();
  void InvalidateTimerOffset();

  void SetCpuTimeForTesting(const base::Callback<int64(void)>& cpu_time);
  void SetOffsetForTesting(int64 offset, bool cache_it);
  void SetTimerTypeForTesting(TimerType type);

 private:
  TimerType timer_type_ = kTimerTypeInvalid;
  int64 offset_ = 0;  // offset cache when timer_type_ == kTimerTypeARB
  bool offset_valid_ = false;
  base::Callback<int64(void)> cpu_time_for_testing_;
  friend class GPUTimer;
  DISALLOW_COPY_AND_ASSIGN(GPUTiming);
};
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_TIMING_H_
