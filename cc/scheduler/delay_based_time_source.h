// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_DELAY_BASED_TIME_SOURCE_H_
#define CC_SCHEDULER_DELAY_BASED_TIME_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/scheduler/time_source.h"

namespace cc {

class Thread;

// This timer implements a time source that achieves the specified interval
// in face of millisecond-precision delayed callbacks and random queueing
// delays.
class CC_EXPORT DelayBasedTimeSource : public TimeSource {
 public:
  static scoped_refptr<DelayBasedTimeSource> Create(base::TimeDelta interval,
                                                    Thread* thread);

  virtual void SetClient(TimeSourceClient* client) OVERRIDE;

  // TimeSource implementation
  virtual void SetTimebaseAndInterval(base::TimeTicks timebase,
                                      base::TimeDelta interval) OVERRIDE;

  virtual void SetActive(bool active) OVERRIDE;
  virtual bool Active() const OVERRIDE;

  // Get the last and next tick times. nextTimeTime() returns null when
  // inactive.
  virtual base::TimeTicks LastTickTime() OVERRIDE;
  virtual base::TimeTicks NextTickTime() OVERRIDE;

  // Virtual for testing.
  virtual base::TimeTicks Now() const;

 protected:
  DelayBasedTimeSource(base::TimeDelta interval, Thread* thread);
  virtual ~DelayBasedTimeSource();

  base::TimeTicks NextTickTarget(base::TimeTicks now);
  void PostNextTickTask(base::TimeTicks now);
  void OnTimerFired();

  enum State {
    STATE_INACTIVE,
    STATE_STARTING,
    STATE_ACTIVE,
  };

  struct Parameters {
    Parameters(base::TimeDelta interval, base::TimeTicks tick_target)
        : interval(interval), tick_target(tick_target) {}
    base::TimeDelta interval;
    base::TimeTicks tick_target;
  };

  TimeSourceClient* client_;
  bool has_tick_target_;
  base::TimeTicks last_tick_time_;

  // current_parameters_ should only be written by PostNextTickTask.
  // next_parameters_ will take effect on the next call to PostNextTickTask.
  // Maintaining a pending set of parameters allows NextTickTime() to always
  // reflect the actual time we expect OnTimerFired to be called.
  Parameters current_parameters_;
  Parameters next_parameters_;

  State state_;

  Thread* thread_;
  base::WeakPtrFactory<DelayBasedTimeSource> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(DelayBasedTimeSource);
};

}  // namespace cc

#endif  // CC_SCHEDULER_DELAY_BASED_TIME_SOURCE_H_
