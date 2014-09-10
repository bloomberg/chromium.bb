// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCHEDULER_TEST_COMMON_H_
#define CC_TEST_SCHEDULER_TEST_COMMON_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/scheduler.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeTimeSourceClient : public TimeSourceClient {
 public:
  FakeTimeSourceClient() { Reset(); }
  void Reset() { tick_called_ = false; }
  bool TickCalled() const { return tick_called_; }

  // TimeSourceClient implementation.
  virtual void OnTimerTick() OVERRIDE;

 protected:
  bool tick_called_;
};

class FakeDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  static scoped_refptr<FakeDelayBasedTimeSource> Create(
      base::TimeDelta interval, base::SingleThreadTaskRunner* task_runner) {
    return make_scoped_refptr(new FakeDelayBasedTimeSource(interval,
                                                           task_runner));
  }

  void SetNow(base::TimeTicks time) { now_ = time; }
  virtual base::TimeTicks Now() const OVERRIDE;

 protected:
  FakeDelayBasedTimeSource(base::TimeDelta interval,
                           base::SingleThreadTaskRunner* task_runner)
      : DelayBasedTimeSource(interval, task_runner) {}
  virtual ~FakeDelayBasedTimeSource() {}

  base::TimeTicks now_;
};

class TestDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  static scoped_refptr<TestDelayBasedTimeSource> Create(
      scoped_refptr<TestNowSource> now_src,
      base::TimeDelta interval,
      OrderedSimpleTaskRunner* task_runner) {
    return make_scoped_refptr(
        new TestDelayBasedTimeSource(now_src, interval, task_runner));
  }

 protected:
  TestDelayBasedTimeSource(scoped_refptr<TestNowSource> now_src,
                           base::TimeDelta interval,
                           OrderedSimpleTaskRunner* task_runner);

  // Overridden from DelayBasedTimeSource
  virtual ~TestDelayBasedTimeSource();
  virtual base::TimeTicks Now() const OVERRIDE;
  virtual std::string TypeString() const OVERRIDE;

  scoped_refptr<TestNowSource> now_src_;
};

class TestScheduler : public Scheduler {
 public:
  static scoped_ptr<TestScheduler> Create(
      scoped_refptr<TestNowSource> now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id) {
    // A bunch of tests require Now() to be > BeginFrameArgs::DefaultInterval()
    now_src->AdvanceNow(base::TimeDelta::FromMilliseconds(100));

    scoped_refptr<OrderedSimpleTaskRunner> test_task_runner =
        new OrderedSimpleTaskRunner(now_src, true);
    return make_scoped_ptr(new TestScheduler(now_src,
                                             client,
                                             scheduler_settings,
                                             layer_tree_host_id,
                                             test_task_runner));
  }

  // Extra test helper functionality
  bool IsBeginRetroFrameArgsEmpty() const {
    return begin_retro_frame_args_.empty();
  }

  bool IsSyntheticBeginFrameSourceActive() const {
    return synthetic_begin_frame_source_->IsActive();
  }

  OrderedSimpleTaskRunner& task_runner() { return *test_task_runner_; }

  virtual ~TestScheduler();

 protected:
  // Overridden from Scheduler.
  virtual base::TimeTicks Now() const OVERRIDE;

 private:
  TestScheduler(scoped_refptr<TestNowSource> now_src,
                SchedulerClient* client,
                const SchedulerSettings& scheduler_settings,
                int layer_tree_host_id,
                const scoped_refptr<OrderedSimpleTaskRunner>& test_task_runner);

  scoped_refptr<TestNowSource> now_src_;
  OrderedSimpleTaskRunner* test_task_runner_;
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
