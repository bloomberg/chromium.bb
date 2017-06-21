// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include <deque>
#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/begin_frame_tracker.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"
#include "cc/tiles/tile_priority.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
class SingleThreadTaskRunner;
}

namespace cc {

class CompositorTimingHistory;

class SchedulerClient {
 public:
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) = 0;
  virtual void ScheduledActionSendBeginMainFrame(
      const BeginFrameArgs& args) = 0;
  virtual DrawResult ScheduledActionDrawIfPossible() = 0;
  virtual DrawResult ScheduledActionDrawForced() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionActivateSyncTree() = 0;
  virtual void ScheduledActionBeginLayerTreeFrameSinkCreation() = 0;
  virtual void ScheduledActionPrepareTiles() = 0;
  virtual void ScheduledActionInvalidateLayerTreeFrameSink() = 0;
  virtual void ScheduledActionPerformImplSideInvalidation() = 0;
  virtual void DidFinishImplFrame() = 0;
  virtual void DidNotProduceFrame(const BeginFrameAck& ack) = 0;
  virtual void SendBeginMainFrameNotExpectedSoon() = 0;
  virtual void ScheduledActionBeginMainFrameNotExpectedUntil(
      base::TimeTicks time) = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class CC_EXPORT Scheduler : public BeginFrameObserverBase {
 public:
  Scheduler(SchedulerClient* client,
            const SchedulerSettings& scheduler_settings,
            int layer_tree_host_id,
            base::SingleThreadTaskRunner* task_runner,
            std::unique_ptr<CompositorTimingHistory> compositor_timing_history);
  ~Scheduler() override;

  // This is needed so that the scheduler doesn't perform spurious actions while
  // the compositor is being torn down.
  void Stop();

  // BeginFrameObserverBase
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override;

  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw);

  const SchedulerSettings& settings() const { return settings_; }

  void SetVisible(bool visible);
  bool visible() { return state_machine_.visible(); }
  void SetCanDraw(bool can_draw);
  void NotifyReadyToActivate();
  void NotifyReadyToDraw();
  void SetBeginFrameSource(BeginFrameSource* source);

  void SetNeedsBeginMainFrame();
  // Requests a single impl frame (after the current frame if there is one
  // active).
  void SetNeedsOneBeginImplFrame();

  void SetNeedsRedraw();

  void SetNeedsPrepareTiles();

  // Requests a pending tree should be created to invalidate content on the impl
  // thread, after the current tree is activated, if any. If the request
  // necessitates creating a pending tree only for impl-side invalidations, the
  // |client_| is informed to perform this action using
  // ScheduledActionRunImplSideInvalidation.
  // If ScheduledActionCommit is performed, the impl-side invalidations should
  // be merged with the main frame and the request is assumed to be completed.
  void SetNeedsImplSideInvalidation();

  // Drawing should result in submitting a CompositorFrame to the
  // LayerTreeFrameSink and then calling this.
  void DidSubmitCompositorFrame();
  // The LayerTreeFrameSink acks when it is ready for a new frame which
  // should result in this getting called to unblock the next draw.
  void DidReceiveCompositorFrameAck();

  void SetTreePrioritiesAndScrollState(TreePriority tree_priority,
                                       ScrollHandlerState scroll_handler_state);

  void NotifyReadyToCommit();
  void BeginMainFrameAborted(CommitEarlyOutReason reason);
  void DidCommit();

  void WillPrepareTiles();
  void DidPrepareTiles();
  void DidLoseLayerTreeFrameSink();
  void DidCreateAndInitializeLayerTreeFrameSink();

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
  bool ImplLatencyTakesPriority() const {
    return state_machine_.ImplLatencyTakesPriority();
  }

  // Pass in a main_thread_start_time of base::TimeTicks() if it is not
  // known or not trustworthy (e.g. blink is running on a remote channel)
  // to signal that the start time isn't known and should not be used for
  // scheduling or statistics purposes.
  void NotifyBeginMainFrameStarted(base::TimeTicks main_thread_start_time);

  base::TimeTicks LastBeginImplFrameTime();

  void SetDeferCommits(bool defer_commits);

  // Controls whether the BeginMainFrameNotExpected messages should be sent to
  // the main thread by the cc scheduler.
  void SetMainThreadWantsBeginMainFrameNotExpected(bool new_state);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  void SetVideoNeedsBeginFrames(bool video_needs_begin_frames);

  const BeginFrameSource* begin_frame_source() const {
    return begin_frame_source_;
  }

  BeginFrameAck CurrentBeginFrameAckForActiveTree() const;

 protected:
  // Virtual for testing.
  virtual base::TimeTicks Now() const;

  const SchedulerSettings settings_;
  SchedulerClient* const client_;
  const int layer_tree_host_id_;
  base::SingleThreadTaskRunner* task_runner_;

  BeginFrameSource* begin_frame_source_ = nullptr;
  bool observing_begin_frame_source_ = false;

  bool skipped_last_frame_missed_exceeded_deadline_ = false;
  bool skipped_last_frame_to_reduce_latency_ = false;

  std::unique_ptr<CompositorTimingHistory> compositor_timing_history_;

  SchedulerStateMachine::BeginImplFrameDeadlineMode
      begin_impl_frame_deadline_mode_ =
          SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE;
  base::TimeTicks deadline_;
  base::TimeTicks deadline_scheduled_at_;

  BeginFrameTracker begin_impl_frame_tracker_;
  BeginFrameArgs begin_main_frame_args_;

  base::Closure begin_impl_frame_deadline_closure_;
  base::CancelableClosure begin_impl_frame_deadline_task_;
  base::CancelableClosure missed_begin_frame_task_;

  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_ = false;
  SchedulerStateMachine::Action inside_action_ =
      SchedulerStateMachine::ACTION_NONE;

  bool stopped_ = false;

 private:
  void ScheduleBeginImplFrameDeadline();
  void ScheduleBeginImplFrameDeadlineIfNeeded();
  void BeginImplFrameNotExpectedSoon();
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time);
  void SetupNextBeginFrameIfNeeded();
  void DrawIfPossible();
  void DrawForced();
  void ProcessScheduledActions();
  void UpdateCompositorTimingHistoryRecordingEnabled();
  bool ShouldRecoverMainLatency(const BeginFrameArgs& args,
                                bool can_activate_before_deadline) const;
  bool ShouldRecoverImplLatency(const BeginFrameArgs& args,
                                bool can_activate_before_deadline) const;
  bool CanBeginMainFrameAndActivateBeforeDeadline(
      const BeginFrameArgs& args,
      base::TimeDelta bmf_to_activate_estimate,
      base::TimeTicks now) const;
  void AdvanceCommitStateIfPossible();
  bool IsBeginMainFrameSentOrStarted() const;
  void BeginImplFrameWithDeadline(const BeginFrameArgs& args);
  void BeginImplFrameSynchronous(const BeginFrameArgs& args);
  void BeginImplFrame(const BeginFrameArgs& args, base::TimeTicks now);
  void FinishImplFrame();
  enum BeginFrameResult { kBeginFrameSkipped, kBeginFrameFinished };
  void SendBeginFrameAck(const BeginFrameArgs& args, BeginFrameResult result);
  void OnBeginImplFrameDeadline();
  void PollToAdvanceCommitState();

  bool IsInsideAction(SchedulerStateMachine::Action action) {
    return inside_action_ == action;
  }

  base::WeakPtrFactory<Scheduler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
