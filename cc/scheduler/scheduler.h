// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/vsync_parameter_observer.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"

namespace base {
namespace debug {
class ConvertableToTraceFormat;
}
class SingleThreadTaskRunner;
}

namespace cc {

class SchedulerClient {
 public:
  virtual BeginFrameSource* ExternalBeginFrameSource() = 0;
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) = 0;
  virtual void ScheduledActionSendBeginMainFrame() = 0;
  virtual DrawResult ScheduledActionDrawAndSwapIfPossible() = 0;
  virtual DrawResult ScheduledActionDrawAndSwapForced() = 0;
  virtual void ScheduledActionAnimate() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionUpdateVisibleTiles() = 0;
  virtual void ScheduledActionActivateSyncTree() = 0;
  virtual void ScheduledActionBeginOutputSurfaceCreation() = 0;
  virtual void ScheduledActionManageTiles() = 0;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) = 0;
  virtual base::TimeDelta DrawDurationEstimate() = 0;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() = 0;
  virtual base::TimeDelta CommitToActivateDurationEstimate() = 0;
  virtual void DidBeginImplFrameDeadline() = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class Scheduler;
// This class exists to allow tests to override the frame source construction.
// A virtual method can't be used as this needs to happen in the constructor
// (see C++ FAQ / Section 23 - http://goo.gl/fnrwom for why).
// This class exists solely long enough to construct the frame sources.
class CC_EXPORT SchedulerFrameSourcesConstructor {
 public:
  virtual ~SchedulerFrameSourcesConstructor() {}
  virtual BeginFrameSource* ConstructPrimaryFrameSource(Scheduler* scheduler);
  virtual BeginFrameSource* ConstructBackgroundFrameSource(
      Scheduler* scheduler);

 protected:
  SchedulerFrameSourcesConstructor() {}

  friend class Scheduler;
};

class CC_EXPORT Scheduler : public BeginFrameObserverMixIn,
                            public base::PowerObserver {
 public:
  static scoped_ptr<Scheduler> Create(
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::PowerMonitor* power_monitor) {
    SchedulerFrameSourcesConstructor frame_sources_constructor;
    return make_scoped_ptr(new Scheduler(client,
                                         scheduler_settings,
                                         layer_tree_host_id,
                                         task_runner,
                                         power_monitor,
                                         &frame_sources_constructor));
  }

  ~Scheduler() override;

  // base::PowerObserver method.
  void OnPowerStateChange(bool on_battery_power) override;

  const SchedulerSettings& settings() const { return settings_; }

  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);
  void SetEstimatedParentDrawTime(base::TimeDelta draw_time);

  void SetCanStart();

  void SetVisible(bool visible);
  void SetCanDraw(bool can_draw);
  void NotifyReadyToActivate();

  void SetNeedsCommit();

  void SetNeedsRedraw();

  void SetNeedsAnimate();

  void SetNeedsManageTiles();

  void SetMaxSwapsPending(int max);
  void DidSwapBuffers();
  void SetSwapUsedIncompleteTile(bool used_incomplete_tile);
  void DidSwapBuffersComplete();

  void SetImplLatencyTakesPriority(bool impl_latency_takes_priority);

  void NotifyReadyToCommit();
  void BeginMainFrameAborted(bool did_handle);

  void DidManageTiles();
  void DidLoseOutputSurface();
  void DidCreateAndInitializeOutputSurface();

  // Tests do not want to shut down until all possible BeginMainFrames have
  // occured to prevent flakiness.
  bool MainFrameForTestingWillHappen() const {
    return state_machine_.CommitPending() ||
           state_machine_.CouldSendBeginMainFrame();
  }

  bool CommitPending() const { return state_machine_.CommitPending(); }
  bool RedrawPending() const { return state_machine_.RedrawPending(); }
  bool ManageTilesPending() const {
    return state_machine_.ManageTilesPending();
  }
  bool MainThreadIsInHighLatencyMode() const {
    return state_machine_.MainThreadIsInHighLatencyMode();
  }
  bool BeginImplFrameDeadlinePending() const {
    return !begin_impl_frame_deadline_task_.IsCancelled();
  }

  bool WillDrawIfNeeded() const;

  base::TimeTicks AnticipatedDrawTime() const;

  void NotifyBeginMainFrameStarted();

  base::TimeTicks LastBeginImplFrameTime();

  scoped_refptr<base::debug::ConvertableToTraceFormat> AsValue() const;
  void AsValueInto(base::debug::TracedValue* value) const override;

  void SetContinuousPainting(bool continuous_painting) {
    state_machine_.SetContinuousPainting(continuous_painting);
  }

  // BeginFrameObserverMixin
  bool OnBeginFrameMixInDelegate(const BeginFrameArgs& args) override;

 protected:
  Scheduler(SchedulerClient* client,
            const SchedulerSettings& scheduler_settings,
            int layer_tree_host_id,
            const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
            base::PowerMonitor* power_monitor,
            SchedulerFrameSourcesConstructor* frame_sources_constructor);

  // virtual for testing - Don't call these in the constructor or
  // destructor!
  virtual base::TimeTicks Now() const;

  scoped_ptr<BeginFrameSourceMultiplexer> frame_source_;
  BeginFrameSource* primary_frame_source_;
  BeginFrameSource* background_frame_source_;

  // Storage when frame sources are internal
  scoped_ptr<BeginFrameSource> primary_frame_source_internal_;
  scoped_ptr<SyntheticBeginFrameSource> background_frame_source_internal_;

  VSyncParameterObserver* vsync_observer_;

  const SchedulerSettings settings_;
  SchedulerClient* client_;
  int layer_tree_host_id_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::PowerMonitor* power_monitor_;

  base::TimeDelta estimated_parent_draw_time_;

  bool begin_retro_frame_posted_;
  std::deque<BeginFrameArgs> begin_retro_frame_args_;
  BeginFrameArgs begin_impl_frame_args_;

  base::Closure begin_retro_frame_closure_;
  base::Closure begin_unthrottled_frame_closure_;

  base::Closure begin_impl_frame_deadline_closure_;
  base::Closure poll_for_draw_triggers_closure_;
  base::Closure advance_commit_state_closure_;
  base::CancelableClosure begin_impl_frame_deadline_task_;
  base::CancelableClosure poll_for_draw_triggers_task_;
  base::CancelableClosure advance_commit_state_task_;

  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_;
  SchedulerStateMachine::Action inside_action_;

 private:
  base::TimeTicks AdjustedBeginImplFrameDeadline(
      const BeginFrameArgs& args,
      base::TimeDelta draw_duration_estimate) const;
  void ScheduleBeginImplFrameDeadline(base::TimeTicks deadline);
  void SetupNextBeginFrameIfNeeded();
  void PostBeginRetroFrameIfNeeded();
  void SetupPollingMechanisms(bool needs_begin_frame);
  void DrawAndSwapIfPossible();
  void ProcessScheduledActions();
  bool CanCommitAndActivateBeforeDeadline() const;
  void AdvanceCommitStateIfPossible();
  bool IsBeginMainFrameSentOrStarted() const;
  void BeginRetroFrame();
  void BeginImplFrame(const BeginFrameArgs& args);
  void OnBeginImplFrameDeadline();
  void PollForAnticipatedDrawTriggers();
  void PollToAdvanceCommitState();
  void SetupPowerMonitoring();
  void TeardownPowerMonitoring();

  base::TimeDelta EstimatedParentDrawTime() {
    return estimated_parent_draw_time_;
  }

  bool IsInsideAction(SchedulerStateMachine::Action action) {
    return inside_action_ == action;
  }

  base::WeakPtrFactory<Scheduler> weak_factory_;

  friend class SchedulerFrameSourcesConstructor;
  friend class TestSchedulerFrameSourcesConstructor;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
