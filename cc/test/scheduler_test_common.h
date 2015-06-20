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

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTimeSourceClient);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDelayBasedTimeSource);
};

class TestDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  static scoped_refptr<TestDelayBasedTimeSource> Create(
      base::SimpleTestTickClock* now_src,
      base::TimeDelta interval,
      OrderedSimpleTaskRunner* task_runner) {
    return make_scoped_refptr(
        new TestDelayBasedTimeSource(now_src, interval, task_runner));
  }

 protected:
  TestDelayBasedTimeSource(base::SimpleTestTickClock* now_src,
                           base::TimeDelta interval,
                           OrderedSimpleTaskRunner* task_runner);

  // Overridden from DelayBasedTimeSource
  ~TestDelayBasedTimeSource() override;
  base::TimeTicks Now() const override;
  std::string TypeString() const override;
  // Not owned.
  base::SimpleTestTickClock* now_src_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDelayBasedTimeSource);
};

class FakeBeginFrameSource : public BeginFrameSourceBase {
 public:
  FakeBeginFrameSource() : remaining_frames_(false) {}
  ~FakeBeginFrameSource() override {}

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

 private:
  bool remaining_frames_;

  DISALLOW_COPY_AND_ASSIGN(FakeBeginFrameSource);
};

class TestBackToBackBeginFrameSource : public BackToBackBeginFrameSource {
 public:
  ~TestBackToBackBeginFrameSource() override;

  static scoped_ptr<TestBackToBackBeginFrameSource> Create(
      base::SimpleTestTickClock* now_src,
      base::SingleThreadTaskRunner* task_runner) {
    return make_scoped_ptr(
        new TestBackToBackBeginFrameSource(now_src, task_runner));
  }

 protected:
  TestBackToBackBeginFrameSource(base::SimpleTestTickClock* now_src,
                                 base::SingleThreadTaskRunner* task_runner);

  base::TimeTicks Now() override;
  // Not owned.
  base::SimpleTestTickClock* now_src_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBackToBackBeginFrameSource);
};

class TestSyntheticBeginFrameSource : public SyntheticBeginFrameSource {
 public:
  ~TestSyntheticBeginFrameSource() override;

  static scoped_ptr<TestSyntheticBeginFrameSource> Create(
      base::SimpleTestTickClock* now_src,
      OrderedSimpleTaskRunner* task_runner,
      base::TimeDelta initial_interval) {
    return make_scoped_ptr(
        new TestSyntheticBeginFrameSource(TestDelayBasedTimeSource::Create(
            now_src, initial_interval, task_runner)));
  }

 protected:
  TestSyntheticBeginFrameSource(
      scoped_refptr<DelayBasedTimeSource> time_source);

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSyntheticBeginFrameSource);
};

class TestScheduler;
class TestSchedulerFrameSourcesConstructor
    : public SchedulerFrameSourcesConstructor {
 public:
  ~TestSchedulerFrameSourcesConstructor() override;

 protected:
  BeginFrameSource* ConstructPrimaryFrameSource(Scheduler* scheduler) override;
  BeginFrameSource* ConstructUnthrottledFrameSource(
      Scheduler* scheduler) override;

  OrderedSimpleTaskRunner* test_task_runner_;
  // Not owned.
  base::SimpleTestTickClock* now_src_;

 protected:
  explicit TestSchedulerFrameSourcesConstructor(
      OrderedSimpleTaskRunner* test_task_runner,
      base::SimpleTestTickClock* now_src);
  friend class TestScheduler;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSchedulerFrameSourcesConstructor);
};

class TestScheduler : public Scheduler {
 public:
  static scoped_ptr<TestScheduler> Create(
      base::SimpleTestTickClock* now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      const scoped_refptr<OrderedSimpleTaskRunner>& task_runner,
      scoped_ptr<BeginFrameSource> external_begin_frame_source) {
    TestSchedulerFrameSourcesConstructor frame_sources_constructor(
        task_runner.get(), now_src);
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

  base::TimeDelta BeginImplFrameInterval() {
    return begin_impl_frame_tracker_.Interval();
  }

 protected:
  // Overridden from Scheduler.
  base::TimeTicks Now() const override;

 private:
  TestScheduler(base::SimpleTestTickClock* now_src,
                SchedulerClient* client,
                const SchedulerSettings& scheduler_settings,
                int layer_tree_host_id,
                const scoped_refptr<OrderedSimpleTaskRunner>& test_task_runner,
                TestSchedulerFrameSourcesConstructor* frame_sources_constructor,
                scoped_ptr<BeginFrameSource> external_begin_frame_source);

  // Not owned.
  base::SimpleTestTickClock* now_src_;

  DISALLOW_COPY_AND_ASSIGN(TestScheduler);
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
