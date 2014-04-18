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
#include "cc/scheduler/time_source.h"

namespace base { class SingleThreadTaskRunner; }

namespace cc {

class TimeSource;
class FrameRateController;

class CC_EXPORT FrameRateControllerClient {
 protected:
  virtual ~FrameRateControllerClient() {}

 public:
  virtual void FrameRateControllerTick(const BeginFrameArgs& args) = 0;
};

// The FrameRateController is used in cases where we self-tick (i.e. BeginFrame
// is not sent by a parent compositor.
class CC_EXPORT FrameRateController : TimeSourceClient {
 public:
  explicit FrameRateController(scoped_refptr<TimeSource> timer);
  virtual ~FrameRateController();

  void SetClient(FrameRateControllerClient* client) { client_ = client; }

  // Returns a valid BeginFrame on activation to potentially be used
  // retroactively.
  BeginFrameArgs SetActive(bool active);

  bool IsActive() { return active_; }

  void SetTimebaseAndInterval(base::TimeTicks timebase,
                              base::TimeDelta interval);
  void SetDeadlineAdjustment(base::TimeDelta delta);

  virtual void OnTimerTick() OVERRIDE;

 protected:
    // This returns null for unthrottled frame-rate.
  base::TimeTicks NextTickTime();
  // This returns now for unthrottled frame-rate.
  base::TimeTicks LastTickTime();

  FrameRateControllerClient* client_;
  base::TimeDelta interval_;
  base::TimeDelta deadline_adjustment_;
  scoped_refptr<TimeSource> time_source_;
  bool active_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameRateController);
};

}  // namespace cc

#endif  // CC_SCHEDULER_FRAME_RATE_CONTROLLER_H_
