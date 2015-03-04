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
  FakeTimeSourceClient() : tick_called_(false) {}
  void Reset() { tick_called_ = false; }
  bool TickCalled() const { return tick_called_; }

  // TimeSourceClient implementation.
  void OnTimerTick() override;

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
  base::TimeTicks Now() const override;

 protected:
  FakeDelayBasedTimeSource(base::TimeDelta interval,
                           base::SingleThreadTaskRunner* task_runner)
      : DelayBasedTimeSource(interval, task_runner) {}
  ~FakeDelayBasedTimeSource() override {}

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
  ~TestDelayBasedTimeSource() override;
  base::TimeTicks Now() const override;
  std::string TypeString() const override;

  scoped_refptr<TestNowSource> now_src_;
};

struct FakeBeginFrameSource : public BeginFrameSourceMixIn {
  bool remaining_frames_ = false;

  BeginFrameObserver* GetObserver() { return observer_; }

  BeginFrameArgs TestLastUsedBeginFrameArgs() {
    if (observer_) {
      return observer_->LastUsedBeginFrameArgs();
    }
    return BeginFrameArgs();
  }
  void TestOnBeginFrame(const BeginFrameArgs& args) {
    return CallOnBeginFrame(args);
  }

  // BeginFrameSource
  void DidFinishFrame(size_t remaining_frames) override;
  void AsValueInto(base::trace_event::TracedValue* dict) const override;

  ~FakeBeginFrameSource() override {}
};

class TestBackToBackBeginFrameSource : public BackToBackBeginFrameSource {
 public:
  ~TestBackToBackBeginFrameSource() override;

  static scoped_ptr<TestBackToBackBeginFrameSource> Create(
      scoped_refptr<TestNowSource> now_src,
      base::SingleThreadTaskRunner* task_runner) {
    return make_scoped_ptr(
        new TestBackToBackBeginFrameSource(now_src, task_runner));
  }

 protected:
  TestBackToBackBeginFrameSource(scoped_refptr<TestNowSource> now_src,
                                 base::SingleThreadTaskRunner* task_runner);

  base::TimeTicks Now() override;

  scoped_refptr<TestNowSource> now_src_;
};

class TestSyntheticBeginFrameSource : public SyntheticBeginFrameSource {
 public:
  ~TestSyntheticBeginFrameSource() override;

  static scoped_ptr<TestSyntheticBeginFrameSource> Create(
      scoped_refptr<TestNowSource> now_src,
      OrderedSimpleTaskRunner* task_runner,
      base::TimeDelta initial_interval) {
    return make_scoped_ptr(
        new TestSyntheticBeginFrameSource(TestDelayBasedTimeSource::Create(
            now_src, initial_interval, task_runner)));
  }

 protected:
  TestSyntheticBeginFrameSource(
      scoped_refptr<DelayBasedTimeSource> time_source);
};

class TestScheduler;
class TestSchedulerFrameSourcesConstructor
    : public SchedulerFrameSourcesConstructor {
 public:
  ~TestSchedulerFrameSourcesConstructor() override;

 protected:
  BeginFrameSource* ConstructPrimaryFrameSource(Scheduler* scheduler) override;
  BeginFrameSource* ConstructBackgroundFrameSource(
      Scheduler* scheduler) override;
  BeginFrameSource* ConstructUnthrottledFrameSource(
      Scheduler* scheduler) override;

  OrderedSimpleTaskRunner* test_task_runner_;
  TestNowSource* now_src_;

 protected:
  explicit TestSchedulerFrameSourcesConstructor(
      OrderedSimpleTaskRunner* test_task_runner,
      TestNowSource* now_src);
  friend class TestScheduler;
};

class TestScheduler : public Scheduler {
 public:
  static scoped_ptr<TestScheduler> Create(
      scoped_refptr<TestNowSource> now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      const scoped_refptr<OrderedSimpleTaskRunner>& task_runner,
      scoped_ptr<BeginFrameSource> external_begin_frame_source) {
    TestSchedulerFrameSourcesConstructor frame_sources_constructor(
        task_runner.get(), now_src.get());
    return make_scoped_ptr(new TestScheduler(
                                   now_src,
                                   client,
                                   scheduler_settings,
                                   layer_tree_host_id,
                                   task_runner,
                                   &frame_sources_constructor,
                                   external_begin_frame_source.Pass()));
  }

  // Extra test helper functionality
  bool IsBeginRetroFrameArgsEmpty() const {
    return begin_retro_frame_args_.empty();
  }

  bool CanStart() const { return state_machine_.CanStartForTesting(); }

  BeginFrameSource& frame_source() { return *frame_source_; }
  bool FrameProductionThrottled() { return throttle_frame_production_; }

  ~TestScheduler() override;

  void NotifyReadyToCommitThenActivateIfNeeded() {
    NotifyReadyToCommit();
    if (settings_.impl_side_painting) {
      NotifyReadyToActivate();
    }
  }

 protected:
  // Overridden from Scheduler.
  base::TimeTicks Now() const override;

 private:
  TestScheduler(
      scoped_refptr<TestNowSource> now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      const scoped_refptr<OrderedSimpleTaskRunner>& test_task_runner,
      TestSchedulerFrameSourcesConstructor* frame_sources_constructor,
      scoped_ptr<BeginFrameSource> external_begin_frame_source);

  scoped_refptr<TestNowSource> now_src_;
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
