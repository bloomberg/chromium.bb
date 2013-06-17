// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCHEDULER_TEST_COMMON_H_
#define CC_TEST_SCHEDULER_TEST_COMMON_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/base/thread.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeTimeSourceClient : public cc::TimeSourceClient {
 public:
  FakeTimeSourceClient() { Reset(); }
  void Reset() { tick_called_ = false; }
  bool TickCalled() const { return tick_called_; }

  // TimeSourceClient implementation.
  virtual void OnTimerTick() OVERRIDE;

 protected:
  bool tick_called_;
};

class FakeThread : public cc::Thread {
 public:
  FakeThread();
  virtual ~FakeThread();

  void Reset() {
    pending_task_delay_ = 0;
    pending_task_.reset();
    run_pending_task_on_overwrite_ = false;
  }

  void RunPendingTaskOnOverwrite(bool enable) {
    run_pending_task_on_overwrite_ = enable;
  }

  bool HasPendingTask() const { return pending_task_; }
  void RunPendingTask();

  int64 PendingDelayMs() const {
    EXPECT_TRUE(HasPendingTask());
    return pending_task_delay_;
  }

  virtual void PostTask(base::Closure cb) OVERRIDE;
  virtual void PostDelayedTask(base::Closure cb, base::TimeDelta delay)
      OVERRIDE;
  virtual bool BelongsToCurrentThread() const OVERRIDE;

 protected:
  scoped_ptr<base::Closure> pending_task_;
  int64 pending_task_delay_;
  bool run_pending_task_on_overwrite_;
};

class FakeTimeSource : public cc::TimeSource {
 public:
  FakeTimeSource() : active_(false), client_(0) {}

  virtual void SetClient(cc::TimeSourceClient* client) OVERRIDE;
  virtual void SetActive(bool b) OVERRIDE;
  virtual bool Active() const OVERRIDE;
  virtual void SetTimebaseAndInterval(base::TimeTicks timebase,
                                      base::TimeDelta interval) OVERRIDE {}
  virtual base::TimeTicks LastTickTime() OVERRIDE;
  virtual base::TimeTicks NextTickTime() OVERRIDE;

  void Tick() {
    ASSERT_TRUE(active_);
    if (client_)
      client_->OnTimerTick();
  }

  void SetNextTickTime(base::TimeTicks next_tick_time) {
    next_tick_time_ = next_tick_time;
  }

 protected:
  virtual ~FakeTimeSource() {}

  bool active_;
  base::TimeTicks next_tick_time_;
  cc::TimeSourceClient* client_;
};

class FakeDelayBasedTimeSource : public cc::DelayBasedTimeSource {
 public:
  static scoped_refptr<FakeDelayBasedTimeSource> Create(
      base::TimeDelta interval, cc::Thread* thread) {
    return make_scoped_refptr(new FakeDelayBasedTimeSource(interval, thread));
  }

  void SetNow(base::TimeTicks time) { now_ = time; }
  virtual base::TimeTicks Now() const OVERRIDE;

 protected:
  FakeDelayBasedTimeSource(base::TimeDelta interval, cc::Thread* thread)
      : DelayBasedTimeSource(interval, thread) {}
  virtual ~FakeDelayBasedTimeSource() {}

  base::TimeTicks now_;
};

class FakeFrameRateController : public cc::FrameRateController {
 public:
  explicit FakeFrameRateController(scoped_refptr<cc::TimeSource> timer)
      : cc::FrameRateController(timer) {}

  int NumFramesPending() const { return num_frames_pending_; }
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
