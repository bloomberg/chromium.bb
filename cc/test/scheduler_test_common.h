// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCHEDULER_TEST_COMMON_H_
#define CC_TEST_SCHEDULER_TEST_COMMON_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
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
  explicit FakeDelayBasedTimeSource(base::SingleThreadTaskRunner* task_runner)
      : DelayBasedTimeSource(task_runner) {}
  ~FakeDelayBasedTimeSource() override {}

  void SetNow(base::TimeTicks time) { now_ = time; }
  base::TimeTicks Now() const override;

 protected:
  base::TimeTicks now_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDelayBasedTimeSource);
};

class TestDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  TestDelayBasedTimeSource(base::SimpleTestTickClock* now_src,
                           OrderedSimpleTaskRunner* task_runner);
  ~TestDelayBasedTimeSource() override;

 protected:
  // Overridden from DelayBasedTimeSource
  base::TimeTicks Now() const override;
  std::string TypeString() const override;

  // Not owned.
  base::SimpleTestTickClock* now_src_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDelayBasedTimeSource);
};

class FakeCompositorTimingHistory : public CompositorTimingHistory {
 public:
  static std::unique_ptr<FakeCompositorTimingHistory> Create(
      bool using_synchronous_renderer_compositor);
  ~FakeCompositorTimingHistory() override;

  void SetAllEstimatesTo(base::TimeDelta duration);

  void SetBeginMainFrameQueueDurationCriticalEstimate(base::TimeDelta duration);
  void SetBeginMainFrameQueueDurationNotCriticalEstimate(
      base::TimeDelta duration);
  void SetBeginMainFrameStartToCommitDurationEstimate(base::TimeDelta duration);
  void SetCommitToReadyToActivateDurationEstimate(base::TimeDelta duration);
  void SetPrepareTilesDurationEstimate(base::TimeDelta duration);
  void SetActivateDurationEstimate(base::TimeDelta duration);
  void SetDrawDurationEstimate(base::TimeDelta duration);

  base::TimeDelta BeginMainFrameQueueDurationCriticalEstimate() const override;
  base::TimeDelta BeginMainFrameQueueDurationNotCriticalEstimate()
      const override;
  base::TimeDelta BeginMainFrameStartToCommitDurationEstimate() const override;
  base::TimeDelta CommitToReadyToActivateDurationEstimate() const override;
  base::TimeDelta PrepareTilesDurationEstimate() const override;
  base::TimeDelta ActivateDurationEstimate() const override;
  base::TimeDelta DrawDurationEstimate() const override;

 protected:
  FakeCompositorTimingHistory(bool using_synchronous_renderer_compositor,
                              std::unique_ptr<RenderingStatsInstrumentation>
                                  rendering_stats_instrumentation_owned);

  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_owned_;

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
  TestScheduler(
      base::SimpleTestTickClock* now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      OrderedSimpleTaskRunner* task_runner,
      std::unique_ptr<CompositorTimingHistory> compositor_timing_history);

  bool IsDrawThrottled() const { return state_machine_.IsDrawThrottled(); }

  bool NeedsBeginMainFrame() const {
    return state_machine_.needs_begin_main_frame();
  }

  BeginFrameSource& frame_source() { return *begin_frame_source_; }

  bool MainThreadMissedLastDeadline() const {
    return state_machine_.main_thread_missed_last_deadline();
  }

  bool begin_frames_expected() const {
    return begin_frame_source_ && observing_begin_frame_source_;
  }

  bool BeginFrameNeeded() const { return state_machine_.BeginFrameNeeded(); }

  int current_frame_number() const {
    return state_machine_.current_frame_number();
  }

  bool needs_impl_side_invalidation() const {
    return state_machine_.needs_impl_side_invalidation();
  }

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
  base::SimpleTestTickClock* now_src_;

  DISALLOW_COPY_AND_ASSIGN(TestScheduler);
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
