// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCHEDULER_TEST_COMMON_H_
#define CC_TEST_SCHEDULER_TEST_COMMON_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/scheduler/compositor_timing_history.h"
#include "cc/scheduler/scheduler.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class RenderingStatsInstrumentation;

class FakeDelayBasedTimeSourceClient : public DelayBasedTimeSourceClient {
 public:
  FakeDelayBasedTimeSourceClient() : tick_called_(false) {}
  void Reset() { tick_called_ = false; }
  bool TickCalled() const { return tick_called_; }

  // DelayBasedTimeSourceClient implementation.
  void OnTimerTick() override;

 protected:
  bool tick_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDelayBasedTimeSourceClient);
};

class FakeDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  static scoped_ptr<FakeDelayBasedTimeSource> Create(
      base::TimeDelta interval,
      base::SingleThreadTaskRunner* task_runner) {
    return make_scoped_ptr(new FakeDelayBasedTimeSource(interval, task_runner));
  }

  ~FakeDelayBasedTimeSource() override {}

  void SetNow(base::TimeTicks time) { now_ = time; }
  base::TimeTicks Now() const override;

 protected:
  FakeDelayBasedTimeSource(base::TimeDelta interval,
                           base::SingleThreadTaskRunner* task_runner)
      : DelayBasedTimeSource(interval, task_runner) {}

  base::TimeTicks now_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDelayBasedTimeSource);
};

class TestDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  static scoped_ptr<TestDelayBasedTimeSource> Create(
      base::SimpleTestTickClock* now_src,
      base::TimeDelta interval,
      OrderedSimpleTaskRunner* task_runner) {
    return make_scoped_ptr(
        new TestDelayBasedTimeSource(now_src, interval, task_runner));
  }

  ~TestDelayBasedTimeSource() override;

 protected:
  TestDelayBasedTimeSource(base::SimpleTestTickClock* now_src,
                           base::TimeDelta interval,
                           OrderedSimpleTaskRunner* task_runner);

  // Overridden from DelayBasedTimeSource
  base::TimeTicks Now() const override;
  std::string TypeString() const override;

  // Not owned.
  base::SimpleTestTickClock* now_src_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDelayBasedTimeSource);
};

class FakeBeginFrameSource : public BeginFrameSourceBase {
 public:
  FakeBeginFrameSource();
  ~FakeBeginFrameSource() override;

  // TODO(sunnyps): Use using BeginFrameSourceBase::CallOnBeginFrame instead.
  void TestOnBeginFrame(const BeginFrameArgs& args) {
    return CallOnBeginFrame(args);
  }

  BeginFrameArgs TestLastUsedBeginFrameArgs() {
    if (!observers_.empty())
      return (*observers_.begin())->LastUsedBeginFrameArgs();
    return BeginFrameArgs();
  }

  bool has_observers() const { return !observers_.empty(); }

  // BeginFrameSource
  void AsValueInto(base::trace_event::TracedValue* dict) const override;

  using BeginFrameSourceBase::SetBeginFrameSourcePaused;

 private:
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
    scoped_ptr<TestDelayBasedTimeSource> time_source =
        TestDelayBasedTimeSource::Create(now_src, initial_interval,
                                         task_runner);
    return make_scoped_ptr(
        new TestSyntheticBeginFrameSource(std::move(time_source)));
  }

 protected:
  explicit TestSyntheticBeginFrameSource(
      scoped_ptr<DelayBasedTimeSource> time_source);

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSyntheticBeginFrameSource);
};

class FakeCompositorTimingHistory : public CompositorTimingHistory {
 public:
  static scoped_ptr<FakeCompositorTimingHistory> Create();
  ~FakeCompositorTimingHistory() override;

  void SetAllEstimatesTo(base::TimeDelta duration);

  void SetBeginMainFrameToCommitDurationEstimate(base::TimeDelta duration);
  void SetBeginMainFrameQueueDurationCriticalEstimate(base::TimeDelta duration);
  void SetBeginMainFrameQueueDurationNotCriticalEstimate(
      base::TimeDelta duration);
  void SetBeginMainFrameStartToCommitDurationEstimate(base::TimeDelta duration);
  void SetCommitToReadyToActivateDurationEstimate(base::TimeDelta duration);
  void SetPrepareTilesDurationEstimate(base::TimeDelta duration);
  void SetActivateDurationEstimate(base::TimeDelta duration);
  void SetDrawDurationEstimate(base::TimeDelta duration);

  base::TimeDelta BeginMainFrameToCommitDurationEstimate() const override;
  base::TimeDelta BeginMainFrameQueueDurationCriticalEstimate() const override;
  base::TimeDelta BeginMainFrameQueueDurationNotCriticalEstimate()
      const override;
  base::TimeDelta BeginMainFrameStartToCommitDurationEstimate() const override;
  base::TimeDelta CommitToReadyToActivateDurationEstimate() const override;
  base::TimeDelta PrepareTilesDurationEstimate() const override;
  base::TimeDelta ActivateDurationEstimate() const override;
  base::TimeDelta DrawDurationEstimate() const override;

 protected:
  FakeCompositorTimingHistory(scoped_ptr<RenderingStatsInstrumentation>
                                  rendering_stats_instrumentation_owned);

  scoped_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_owned_;

  base::TimeDelta begin_main_frame_to_commit_duration_;
  base::TimeDelta begin_main_frame_queue_duration_critical_;
  base::TimeDelta begin_main_frame_queue_duration_not_critical_;
  base::TimeDelta begin_main_frame_start_to_commit_duration_;
  base::TimeDelta commit_to_ready_to_activate_duration_;
  base::TimeDelta prepare_tiles_duration_;
  base::TimeDelta activate_duration_;
  base::TimeDelta draw_duration_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeCompositorTimingHistory);
};

class TestScheduler : public Scheduler {
 public:
  static scoped_ptr<TestScheduler> Create(
      base::SimpleTestTickClock* now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      OrderedSimpleTaskRunner* task_runner,
      BeginFrameSource* external_frame_source,
      scoped_ptr<CompositorTimingHistory> compositor_timing_history);

  // Extra test helper functionality
  bool IsBeginRetroFrameArgsEmpty() const {
    return begin_retro_frame_args_.empty();
  }

  bool SwapThrottled() const { return state_machine_.SwapThrottled(); }

  bool NeedsBeginMainFrame() const {
    return state_machine_.needs_begin_main_frame();
  }

  BeginFrameSource& frame_source() { return *frame_source_; }
  bool FrameProductionThrottled() { return throttle_frame_production_; }

  bool MainThreadMissedLastDeadline() const {
    return state_machine_.main_thread_missed_last_deadline();
  }

  bool begin_frames_expected() const { return observing_frame_source_; }

  ~TestScheduler() override;

  base::TimeDelta BeginImplFrameInterval() {
    return begin_impl_frame_tracker_.Interval();
  }

  // Note: This setting will be overriden on the next BeginFrame in the
  // scheduler. To control the value it gets on the next BeginFrame
  // Pass in a fake CompositorTimingHistory that indicates BeginMainFrame
  // to Activation is fast.
  void SetCriticalBeginMainFrameToActivateIsFast(bool is_fast) {
    state_machine_.SetCriticalBeginMainFrameToActivateIsFast(is_fast);
  }

 protected:
  // Overridden from Scheduler.
  base::TimeTicks Now() const override;

 private:
  TestScheduler(
      base::SimpleTestTickClock* now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      OrderedSimpleTaskRunner* task_runner,
      BeginFrameSource* external_frame_source,
      scoped_ptr<TestSyntheticBeginFrameSource> synthetic_frame_source,
      scoped_ptr<TestBackToBackBeginFrameSource> unthrottled_frame_source,
      scoped_ptr<CompositorTimingHistory> compositor_timing_history);

  base::SimpleTestTickClock* now_src_;

  DISALLOW_COPY_AND_ASSIGN(TestScheduler);
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
