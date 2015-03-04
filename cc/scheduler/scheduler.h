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
namespace trace_event {
class ConvertableToTraceFormat;
}
class SingleThreadTaskRunner;
}

namespace cc {

class SchedulerClient {
 public:
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) = 0;
  virtual void ScheduledActionSendBeginMainFrame() = 0;
  virtual DrawResult ScheduledActionDrawAndSwapIfPossible() = 0;
  virtual DrawResult ScheduledActionDrawAndSwapForced() = 0;
  virtual void ScheduledActionAnimate() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionActivateSyncTree() = 0;
  virtual void ScheduledActionBeginOutputSurfaceCreation() = 0;
  virtual void ScheduledActionPrepareTiles() = 0;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) = 0;
  virtual base::TimeDelta DrawDurationEstimate() = 0;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() = 0;
  virtual base::TimeDelta CommitToActivateDurationEstimate() = 0;
  virtual void DidBeginImplFrameDeadline() = 0;
  virtual void SendBeginFramesToChildren(const BeginFrameArgs& args) = 0;
  virtual void SendBeginMainFrameNotExpectedSoon() = 0;

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
  virtual BeginFrameSource* ConstructUnthrottledFrameSource(
      Scheduler* scheduler);

 protected:
  SchedulerFrameSourcesConstructor() {}

  friend class Scheduler;
};

class CC_EXPORT Scheduler : public BeginFrameObserverMixIn {
 public:
  static scoped_ptr<Scheduler> Create(
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      scoped_ptr<BeginFrameSource> external_begin_frame_source) {
    SchedulerFrameSourcesConstructor frame_sources_constructor;
    return make_scoped_ptr(new Scheduler(client,
                                         scheduler_settings,
                                         layer_tree_host_id,
                                         task_runner,
                                         external_begin_frame_source.Pass(),
                                         &frame_sources_constructor));
  }

  ~Scheduler() override;

  // BeginFrameObserverMixin
  bool OnBeginFrameMixInDelegate(const BeginFrameArgs& args) override;

  const SchedulerSettings& settings() const { return settings_; }

  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);
  void SetEstimatedParentDrawTime(base::TimeDelta draw_time);

  void SetCanStart();

  void SetVisible(bool visible);
  void SetCanDraw(bool can_draw);
  void NotifyReadyToActivate();
  void NotifyReadyToDraw();
  void SetThrottleFrameProduction(bool throttle);

  void SetNeedsCommit();

  void SetNeedsRedraw();

  void SetNeedsAnimate();

  void SetNeedsPrepareTiles();

  void SetMaxSwapsPending(int max);
  void DidSwapBuffers();
  void DidSwapBuffersComplete();

  void SetImplLatencyTakesPriority(bool impl_latency_takes_priority);

  void NotifyReadyToCommit();
  void BeginMainFrameAborted(CommitEarlyOutReason reason);

  void DidPrepareTiles();
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
  bool PrepareTilesPending() const {
    return state_machine_.PrepareTilesPending();
  }
  bool MainThreadIsInHighLatencyMode() const {
    return state_machine_.MainThreadIsInHighLatencyMode();
  }
  bool BeginImplFrameDeadlinePending() const {
    return !begin_impl_frame_deadline_task_.IsCancelled();
  }

  base::TimeTicks AnticipatedDrawTime() const;

  void NotifyBeginMainFrameStarted();

  base::TimeTicks LastBeginImplFrameTime();

  void SetDeferCommits(bool defer_commits);

  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
  void AsValueInto(base::trace_event::TracedValue* value) const override;

  void SetContinuousPainting(bool continuous_painting) {
    state_machine_.SetContinuousPainting(continuous_painting);
  }

  void SetChildrenNeedBeginFrames(bool children_need_begin_frames);

 protected:
  Scheduler(SchedulerClient* client,
            const SchedulerSettings& scheduler_settings,
            int layer_tree_host_id,
            const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
            scoped_ptr<BeginFrameSource> external_begin_frame_source,
            SchedulerFrameSourcesConstructor* frame_sources_constructor);

  // virtual for testing - Don't call these in the constructor or
  // destructor!
  virtual base::TimeTicks Now() const;

  scoped_ptr<BeginFrameSourceMultiplexer> frame_source_;
  BeginFrameSource* primary_frame_source_;
  BeginFrameSource* background_frame_source_;
  BeginFrameSource* unthrottled_frame_source_;

  // Storage when frame sources are internal
  scoped_ptr<BeginFrameSource> primary_frame_source_internal_;
  scoped_ptr<SyntheticBeginFrameSource> background_frame_source_internal_;
  scoped_ptr<BeginFrameSource> unthrottled_frame_source_internal_;

  VSyncParameterObserver* vsync_observer_;
  bool throttle_frame_production_;

  const SchedulerSettings settings_;
  SchedulerClient* client_;
  int layer_tree_host_id_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::TimeDelta estimated_parent_draw_time_;

  std::deque<BeginFrameArgs> begin_retro_frame_args_;
  BeginFrameArgs begin_impl_frame_args_;
  SchedulerStateMachine::BeginImplFrameDeadlineMode
      begin_impl_frame_deadline_mode_;

  base::Closure begin_retro_frame_closure_;
  base::Closure begin_impl_frame_deadline_closure_;
  base::Closure poll_for_draw_triggers_closure_;
  base::Closure advance_commit_state_closure_;
  base::CancelableClosure begin_retro_frame_task_;
  base::CancelableClosure begin_impl_frame_deadline_task_;
  base::CancelableClosure poll_for_draw_triggers_task_;
  base::CancelableClosure advance_commit_state_task_;

  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_;
  SchedulerStateMachine::Action inside_action_;

 private:
  void ScheduleBeginImplFrameDeadline();
  void RescheduleBeginImplFrameDeadlineIfNeeded();
  void SetupNextBeginFrameIfNeeded();
  void PostBeginRetroFrameIfNeeded();
  void SetupPollingMechanisms();
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
  void UpdateActiveFrameSource();

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
