// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/scheduler/delay_based_time_source.h"

namespace cc {

BeginFrameSource* SchedulerFrameSourcesConstructor::ConstructPrimaryFrameSource(
    Scheduler* scheduler) {
  if (scheduler->settings_.use_external_begin_frame_source) {
    TRACE_EVENT1("cc",
                 "Scheduler::Scheduler()",
                 "PrimaryFrameSource",
                 "ExternalBeginFrameSource");
    DCHECK(scheduler->primary_frame_source_internal_)
        << "Need external BeginFrameSource";
    return scheduler->primary_frame_source_internal_.get();
  } else {
    TRACE_EVENT1("cc",
                 "Scheduler::Scheduler()",
                 "PrimaryFrameSource",
                 "SyntheticBeginFrameSource");
    scoped_ptr<SyntheticBeginFrameSource> synthetic_source =
        SyntheticBeginFrameSource::Create(scheduler->task_runner_.get(),
                                          scheduler->Now(),
                                          BeginFrameArgs::DefaultInterval());

    DCHECK(!scheduler->vsync_observer_);
    scheduler->vsync_observer_ = synthetic_source.get();

    DCHECK(!scheduler->primary_frame_source_internal_);
    scheduler->primary_frame_source_internal_ = synthetic_source.Pass();
    return scheduler->primary_frame_source_internal_.get();
  }
}

BeginFrameSource*
SchedulerFrameSourcesConstructor::ConstructUnthrottledFrameSource(
    Scheduler* scheduler) {
  TRACE_EVENT1("cc", "Scheduler::Scheduler()", "UnthrottledFrameSource",
               "BackToBackBeginFrameSource");
  DCHECK(!scheduler->unthrottled_frame_source_internal_);
  scheduler->unthrottled_frame_source_internal_ =
      BackToBackBeginFrameSource::Create(scheduler->task_runner_.get());
  return scheduler->unthrottled_frame_source_internal_.get();
}

Scheduler::Scheduler(
    SchedulerClient* client,
    const SchedulerSettings& scheduler_settings,
    int layer_tree_host_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_ptr<BeginFrameSource> external_begin_frame_source,
    SchedulerFrameSourcesConstructor* frame_sources_constructor)
    : frame_source_(),
      primary_frame_source_(NULL),
      primary_frame_source_internal_(external_begin_frame_source.Pass()),
      vsync_observer_(NULL),
      authoritative_vsync_interval_(base::TimeDelta()),
      last_vsync_timebase_(base::TimeTicks()),
      throttle_frame_production_(false),
      settings_(scheduler_settings),
      client_(client),
      layer_tree_host_id_(layer_tree_host_id),
      task_runner_(task_runner),
      begin_impl_frame_deadline_mode_(
          SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE),
      begin_impl_frame_tracker_(BEGINFRAMETRACKER_FROM_HERE),
      state_machine_(scheduler_settings),
      inside_process_scheduled_actions_(false),
      inside_action_(SchedulerStateMachine::ACTION_NONE),
      weak_factory_(this) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
               "Scheduler::Scheduler",
               "settings",
               settings_.AsValue());
  DCHECK(client_);
  DCHECK(!state_machine_.BeginFrameNeeded());

  begin_retro_frame_closure_ =
      base::Bind(&Scheduler::BeginRetroFrame, weak_factory_.GetWeakPtr());
  begin_impl_frame_deadline_closure_ = base::Bind(
      &Scheduler::OnBeginImplFrameDeadline, weak_factory_.GetWeakPtr());

  frame_source_ = BeginFrameSourceMultiplexer::Create();
  frame_source_->AddObserver(this);

  // Primary frame source
  primary_frame_source_ =
      frame_sources_constructor->ConstructPrimaryFrameSource(this);
  frame_source_->AddSource(primary_frame_source_);
  primary_frame_source_->SetClientReady();

  // Unthrottled frame source
  unthrottled_frame_source_ =
      frame_sources_constructor->ConstructUnthrottledFrameSource(this);
  frame_source_->AddSource(unthrottled_frame_source_);

  SetThrottleFrameProduction(scheduler_settings.throttle_frame_production);
}

Scheduler::~Scheduler() {
  if (frame_source_->NeedsBeginFrames())
    frame_source_->SetNeedsBeginFrames(false);
  frame_source_->SetActiveSource(nullptr);
}

base::TimeTicks Scheduler::Now() const {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.now"),
               "Scheduler::Now",
               "now",
               now);
  return now;
}

void Scheduler::CommitVSyncParameters(base::TimeTicks timebase,
                                      base::TimeDelta interval) {
  if (authoritative_vsync_interval_ != base::TimeDelta()) {
    interval = authoritative_vsync_interval_;
  } else if (interval == base::TimeDelta()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = BeginFrameArgs::DefaultInterval();
  }

  last_vsync_timebase_ = timebase;

  if (vsync_observer_)
    vsync_observer_->OnUpdateVSyncParameters(timebase, interval);
}

void Scheduler::SetEstimatedParentDrawTime(base::TimeDelta draw_time) {
  DCHECK_GE(draw_time.ToInternalValue(), 0);
  estimated_parent_draw_time_ = draw_time;
}

void Scheduler::SetCanStart() {
  state_machine_.SetCanStart();
  ProcessScheduledActions();
}

void Scheduler::SetVisible(bool visible) {
  state_machine_.SetVisible(visible);
  ProcessScheduledActions();
}

void Scheduler::SetCanDraw(bool can_draw) {
  state_machine_.SetCanDraw(can_draw);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToActivate() {
  state_machine_.NotifyReadyToActivate();
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToDraw() {
  // Future work might still needed for crbug.com/352894.
  state_machine_.NotifyReadyToDraw();
  ProcessScheduledActions();
}

void Scheduler::SetThrottleFrameProduction(bool throttle) {
  throttle_frame_production_ = throttle;
  if (throttle) {
    frame_source_->SetActiveSource(primary_frame_source_);
  } else {
    frame_source_->SetActiveSource(unthrottled_frame_source_);
  }
  ProcessScheduledActions();
}

void Scheduler::SetNeedsCommit() {
  state_machine_.SetNeedsCommit();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsRedraw() {
  state_machine_.SetNeedsRedraw();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsAnimate() {
  state_machine_.SetNeedsAnimate();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsPrepareTiles() {
  DCHECK(!IsInsideAction(SchedulerStateMachine::ACTION_PREPARE_TILES));
  state_machine_.SetNeedsPrepareTiles();
  ProcessScheduledActions();
}

void Scheduler::SetWaitForReadyToDraw() {
  state_machine_.SetWaitForReadyToDraw();
  ProcessScheduledActions();
}

void Scheduler::SetMaxSwapsPending(int max) {
  state_machine_.SetMaxSwapsPending(max);
}

void Scheduler::DidSwapBuffers() {
  state_machine_.DidSwapBuffers();

  // There is no need to call ProcessScheduledActions here because
  // swapping should not trigger any new actions.
  if (!inside_process_scheduled_actions_) {
    DCHECK_EQ(state_machine_.NextAction(), SchedulerStateMachine::ACTION_NONE);
  }
}

void Scheduler::DidSwapBuffersComplete() {
  DCHECK_GT(state_machine_.pending_swaps(), 0) << AsValue()->ToString();
  state_machine_.DidSwapBuffersComplete();
  ProcessScheduledActions();
}

void Scheduler::SetImplLatencyTakesPriority(bool impl_latency_takes_priority) {
  state_machine_.SetImplLatencyTakesPriority(impl_latency_takes_priority);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToCommit() {
  TRACE_EVENT0("cc", "Scheduler::NotifyReadyToCommit");
  state_machine_.NotifyReadyToCommit();
  ProcessScheduledActions();
}

void Scheduler::BeginMainFrameAborted(CommitEarlyOutReason reason) {
  TRACE_EVENT1("cc", "Scheduler::BeginMainFrameAborted", "reason",
               CommitEarlyOutReasonToString(reason));
  state_machine_.BeginMainFrameAborted(reason);
  ProcessScheduledActions();
}

void Scheduler::DidPrepareTiles() {
  state_machine_.DidPrepareTiles();
}

void Scheduler::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidLoseOutputSurface");
  begin_retro_frame_args_.clear();
  begin_retro_frame_task_.Cancel();
  state_machine_.DidLoseOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeOutputSurface");
  DCHECK(!frame_source_->NeedsBeginFrames());
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());
  state_machine_.DidCreateAndInitializeOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::NotifyBeginMainFrameStarted() {
  TRACE_EVENT0("cc", "Scheduler::NotifyBeginMainFrameStarted");
  state_machine_.NotifyBeginMainFrameStarted();
}

base::TimeTicks Scheduler::LastBeginImplFrameTime() {
  return begin_impl_frame_tracker_.Current().frame_time;
}

void Scheduler::SetupNextBeginFrameIfNeeded() {
  // Never call SetNeedsBeginFrames if the frame source already has the right
  // value.
  if (frame_source_->NeedsBeginFrames() != state_machine_.BeginFrameNeeded()) {
    if (state_machine_.BeginFrameNeeded()) {
      // Call SetNeedsBeginFrames(true) as soon as possible.
      frame_source_->SetNeedsBeginFrames(true);
      devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_,
                                                       true);
    } else if (state_machine_.begin_impl_frame_state() ==
               SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE) {
      // Call SetNeedsBeginFrames(false) in between frames only.
      frame_source_->SetNeedsBeginFrames(false);
      client_->SendBeginMainFrameNotExpectedSoon();
      devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_,
                                                       false);
    }
  }

  PostBeginRetroFrameIfNeeded();
}

// BeginFrame is the mechanism that tells us that now is a good time to start
// making a frame. Usually this means that user input for the frame is complete.
// If the scheduler is busy, we queue the BeginFrame to be handled later as
// a BeginRetroFrame.
bool Scheduler::OnBeginFrameDerivedImpl(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginFrame", "args", args.AsValue());

  // Trace this begin frame time through the Chrome stack
  TRACE_EVENT_FLOW_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"), "BeginFrameArgs",
      args.frame_time.ToInternalValue());

  // TODO(brianderson): Adjust deadline in the DisplayScheduler.
  BeginFrameArgs adjusted_args(args);
  adjusted_args.deadline -= EstimatedParentDrawTime();
  adjusted_args.on_critical_path = !ImplLatencyTakesPriority();

  // Deliver BeginFrames to children.
  // TODO(brianderson): Move this responsibility to the DisplayScheduler.
  if (state_machine_.children_need_begin_frames())
    client_->SendBeginFramesToChildren(adjusted_args);

  if (settings_.using_synchronous_renderer_compositor) {
    BeginImplFrameSynchronous(adjusted_args);
    return true;
  }

  // We have just called SetNeedsBeginFrame(true) and the BeginFrameSource has
  // sent us the last BeginFrame we have missed. As we might not be able to
  // actually make rendering for this call, handle it like a "retro frame".
  // TODO(brainderson): Add a test for this functionality ASAP!
  if (adjusted_args.type == BeginFrameArgs::MISSED) {
    begin_retro_frame_args_.push_back(adjusted_args);
    PostBeginRetroFrameIfNeeded();
    return true;
  }

  bool should_defer_begin_frame =
      !begin_retro_frame_args_.empty() ||
      !begin_retro_frame_task_.IsCancelled() ||
      !frame_source_->NeedsBeginFrames() ||
      (state_machine_.begin_impl_frame_state() !=
       SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);

  if (should_defer_begin_frame) {
    begin_retro_frame_args_.push_back(adjusted_args);
    TRACE_EVENT_INSTANT0(
        "cc", "Scheduler::BeginFrame deferred", TRACE_EVENT_SCOPE_THREAD);
    // Queuing the frame counts as "using it", so we need to return true.
  } else {
    BeginImplFrameWithDeadline(adjusted_args);
  }
  return true;
}

void Scheduler::SetChildrenNeedBeginFrames(bool children_need_begin_frames) {
  state_machine_.SetChildrenNeedBeginFrames(children_need_begin_frames);
  ProcessScheduledActions();
}

void Scheduler::SetAuthoritativeVSyncInterval(const base::TimeDelta& interval) {
  authoritative_vsync_interval_ = interval;
  if (vsync_observer_)
    vsync_observer_->OnUpdateVSyncParameters(last_vsync_timebase_, interval);
}

void Scheduler::SetVideoNeedsBeginFrames(bool video_needs_begin_frames) {
  state_machine_.SetVideoNeedsBeginFrames(video_needs_begin_frames);
  ProcessScheduledActions();
}

void Scheduler::OnDrawForOutputSurface() {
  DCHECK(settings_.using_synchronous_renderer_compositor);
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  DCHECK(!BeginImplFrameDeadlinePending());

  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();

  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();
}

// BeginRetroFrame is called for BeginFrames that we've deferred because
// the scheduler was in the middle of processing a previous BeginFrame.
void Scheduler::BeginRetroFrame() {
  TRACE_EVENT0("cc,benchmark", "Scheduler::BeginRetroFrame");
  DCHECK(!settings_.using_synchronous_renderer_compositor);
  DCHECK(!begin_retro_frame_args_.empty());
  DCHECK(!begin_retro_frame_task_.IsCancelled());
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);

  begin_retro_frame_task_.Cancel();

  // Discard expired BeginRetroFrames
  // Today, we should always end up with at most one un-expired BeginRetroFrame
  // because deadlines will not be greater than the next frame time. We don't
  // DCHECK though because some systems don't always have monotonic timestamps.
  // TODO(brianderson): In the future, long deadlines could result in us not
  // draining the queue if we don't catch up. If we consistently can't catch
  // up, our fallback should be to lower our frame rate.
  base::TimeTicks now = Now();

  while (!begin_retro_frame_args_.empty()) {
    const BeginFrameArgs& args = begin_retro_frame_args_.front();
    base::TimeTicks expiration_time = args.deadline;
    if (now <= expiration_time)
      break;
    TRACE_EVENT_INSTANT2(
        "cc", "Scheduler::BeginRetroFrame discarding", TRACE_EVENT_SCOPE_THREAD,
        "expiration_time - now", (expiration_time - now).InMillisecondsF(),
        "BeginFrameArgs", begin_retro_frame_args_.front().AsValue());
    begin_retro_frame_args_.pop_front();
    frame_source_->DidFinishFrame(begin_retro_frame_args_.size());
  }

  if (begin_retro_frame_args_.empty()) {
    TRACE_EVENT_INSTANT0("cc",
                         "Scheduler::BeginRetroFrames all expired",
                         TRACE_EVENT_SCOPE_THREAD);
  } else {
    BeginFrameArgs front = begin_retro_frame_args_.front();
    begin_retro_frame_args_.pop_front();
    BeginImplFrameWithDeadline(front);
  }
}

// There could be a race between the posted BeginRetroFrame and a new
// BeginFrame arriving via the normal mechanism. Scheduler::BeginFrame
// will check if there is a pending BeginRetroFrame to ensure we handle
// BeginFrames in FIFO order.
void Scheduler::PostBeginRetroFrameIfNeeded() {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
               "Scheduler::PostBeginRetroFrameIfNeeded",
               "state",
               AsValue());
  if (!frame_source_->NeedsBeginFrames())
    return;

  if (begin_retro_frame_args_.empty() || !begin_retro_frame_task_.IsCancelled())
    return;

  // begin_retro_frame_args_ should always be empty for the
  // synchronous compositor.
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE)
    return;

  begin_retro_frame_task_.Reset(begin_retro_frame_closure_);

  task_runner_->PostTask(FROM_HERE, begin_retro_frame_task_.callback());
}

void Scheduler::BeginImplFrameWithDeadline(const BeginFrameArgs& args) {
  bool main_thread_is_in_high_latency_mode =
      state_machine_.MainThreadIsInHighLatencyMode();
  TRACE_EVENT2("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               args.AsValue(), "main_thread_is_high_latency",
               main_thread_is_in_high_latency_mode);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "MainThreadLatency", main_thread_is_in_high_latency_mode);

  BeginFrameArgs adjusted_args = args;
  adjusted_args.deadline -= client_->DrawDurationEstimate();

  if (!state_machine_.impl_latency_takes_priority() &&
      main_thread_is_in_high_latency_mode &&
      CanCommitAndActivateBeforeDeadline()) {
    state_machine_.SetSkipNextBeginMainFrameToReduceLatency();
  }

  BeginImplFrame(adjusted_args);

  // The deadline will be scheduled in ProcessScheduledActions.
  state_machine_.OnBeginImplFrameDeadlinePending();
  ProcessScheduledActions();
}

void Scheduler::BeginImplFrameSynchronous(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               args.AsValue());
  BeginImplFrame(args);
  FinishImplFrame();
}

void Scheduler::FinishImplFrame() {
  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();

  client_->DidFinishImplFrame();
  frame_source_->DidFinishFrame(begin_retro_frame_args_.size());
  begin_impl_frame_tracker_.Finish();
}

// BeginImplFrame starts a compositor frame that will wait up until a deadline
// for a BeginMainFrame+activation to complete before it times out and draws
// any asynchronous animation and scroll/pinch updates.
void Scheduler::BeginImplFrame(const BeginFrameArgs& args) {
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  DCHECK(!BeginImplFrameDeadlinePending());
  DCHECK(state_machine_.HasInitializedOutputSurface());

  begin_impl_frame_tracker_.Start(args);
  state_machine_.OnBeginImplFrame();
  devtools_instrumentation::DidBeginFrame(layer_tree_host_id_);
  client_->WillBeginImplFrame(begin_impl_frame_tracker_.Current());

  ProcessScheduledActions();
}

void Scheduler::ScheduleBeginImplFrameDeadline() {
  // The synchronous compositor does not post a deadline task.
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  begin_impl_frame_deadline_task_.Cancel();
  begin_impl_frame_deadline_task_.Reset(begin_impl_frame_deadline_closure_);

  begin_impl_frame_deadline_mode_ =
      state_machine_.CurrentBeginImplFrameDeadlineMode();

  base::TimeTicks deadline;
  switch (begin_impl_frame_deadline_mode_) {
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE:
      // No deadline.
      return;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_IMMEDIATE:
      // We are ready to draw a new active tree immediately.
      // We don't use Now() here because it's somewhat expensive to call.
      deadline = base::TimeTicks();
      break;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_REGULAR:
      // We are animating on the impl thread but we can wait for some time.
      deadline = begin_impl_frame_tracker_.Current().deadline;
      break;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_LATE:
      // We are blocked for one reason or another and we should wait.
      // TODO(brianderson): Handle long deadlines (that are past the next
      // frame's frame time) properly instead of using this hack.
      deadline = begin_impl_frame_tracker_.Current().frame_time +
                 begin_impl_frame_tracker_.Current().interval;
      break;
    case SchedulerStateMachine::
        BEGIN_IMPL_FRAME_DEADLINE_MODE_BLOCKED_ON_READY_TO_DRAW:
      // We are blocked because we are waiting for ReadyToDraw signal. We would
      // post deadline after we received ReadyToDraw singal.
      TRACE_EVENT1("cc", "Scheduler::ScheduleBeginImplFrameDeadline",
                   "deadline_mode", "blocked_on_ready_to_draw");
      return;
  }

  TRACE_EVENT2("cc", "Scheduler::ScheduleBeginImplFrameDeadline", "mode",
               SchedulerStateMachine::BeginImplFrameDeadlineModeToString(
                   begin_impl_frame_deadline_mode_),
               "deadline", deadline);

  base::TimeDelta delta = std::max(deadline - Now(), base::TimeDelta());
  task_runner_->PostDelayedTask(
      FROM_HERE, begin_impl_frame_deadline_task_.callback(), delta);
}

void Scheduler::ScheduleBeginImplFrameDeadlineIfNeeded() {
  if (settings_.using_synchronous_renderer_compositor)
    return;

  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME)
    return;

  if (begin_impl_frame_deadline_mode_ ==
          state_machine_.CurrentBeginImplFrameDeadlineMode() &&
      BeginImplFrameDeadlinePending())
    return;

  ScheduleBeginImplFrameDeadline();
}

void Scheduler::OnBeginImplFrameDeadline() {
  TRACE_EVENT0("cc,benchmark", "Scheduler::OnBeginImplFrameDeadline");
  begin_impl_frame_deadline_task_.Cancel();
  // We split the deadline actions up into two phases so the state machine
  // has a chance to trigger actions that should occur durring and after
  // the deadline separately. For example:
  // * Sending the BeginMainFrame will not occur after the deadline in
  //     order to wait for more user-input before starting the next commit.
  // * Creating a new OuputSurface will not occur during the deadline in
  //     order to allow the state machine to "settle" first.

  // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461509 Scheduler::OnBeginImplFrameDeadline1"));
  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();
  FinishImplFrame();
}

void Scheduler::DrawAndSwapIfPossible() {
  DrawResult result = client_->ScheduledActionDrawAndSwapIfPossible();
  state_machine_.DidDrawIfPossibleCompleted(result);
}

void Scheduler::SetDeferCommits(bool defer_commits) {
  TRACE_EVENT1("cc", "Scheduler::SetDeferCommits",
                "defer_commits",
                defer_commits);
  state_machine_.SetDeferCommits(defer_commits);
  ProcessScheduledActions();
}

void Scheduler::ProcessScheduledActions() {
  // We do not allow ProcessScheduledActions to be recursive.
  // The top-level call will iteratively execute the next action for us anyway.
  if (inside_process_scheduled_actions_)
    return;

  base::AutoReset<bool> mark_inside(&inside_process_scheduled_actions_, true);

  SchedulerStateMachine::Action action;
  do {
    action = state_machine_.NextAction();
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "SchedulerStateMachine",
                 "state",
                 AsValue());
    DVLOG(2) << "Scheduler::ProcessScheduledActions: "
             << SchedulerStateMachine::ActionToString(action) << " "
             << state_machine_.GetStatesForDebugging();
    state_machine_.UpdateState(action);
    base::AutoReset<SchedulerStateMachine::Action>
        mark_inside_action(&inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::ACTION_NONE:
        break;
      case SchedulerStateMachine::ACTION_ANIMATE:
        client_->ScheduledActionAnimate();
        break;
      case SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME:
        client_->ScheduledActionSendBeginMainFrame();
        break;
      case SchedulerStateMachine::ACTION_COMMIT: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile4(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions4"));
        client_->ScheduledActionCommit();
        break;
      }
      case SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE:
        client_->ScheduledActionActivateSyncTree();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile6(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions6"));
        DrawAndSwapIfPossible();
        break;
      }
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_FORCED:
        client_->ScheduledActionDrawAndSwapForced();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT:
        // No action is actually performed, but this allows the state machine to
        // advance out of its waiting to draw state without actually drawing.
        break;
      case SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
        client_->ScheduledActionBeginOutputSurfaceCreation();
        break;
      case SchedulerStateMachine::ACTION_PREPARE_TILES:
        client_->ScheduledActionPrepareTiles();
        break;
      case SchedulerStateMachine::ACTION_INVALIDATE_OUTPUT_SURFACE: {
        client_->ScheduledActionInvalidateOutputSurface();
        break;
      }
    }
  } while (action != SchedulerStateMachine::ACTION_NONE);

  ScheduleBeginImplFrameDeadlineIfNeeded();
  SetupNextBeginFrameIfNeeded();
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat> Scheduler::AsValue()
    const {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();
  AsValueInto(state.get());
  return state;
}

void Scheduler::AsValueInto(base::trace_event::TracedValue* state) const {
  state->BeginDictionary("state_machine");
  state_machine_.AsValueInto(state);
  state->EndDictionary();

  // Only trace frame sources when explicitly enabled - http://crbug.com/420607
  bool frame_tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
      &frame_tracing_enabled);
  if (frame_tracing_enabled) {
    state->BeginDictionary("frame_source_");
    frame_source_->AsValueInto(state);
    state->EndDictionary();
  }

  state->BeginDictionary("scheduler_state");
  state->SetDouble("estimated_parent_draw_time_ms",
                   estimated_parent_draw_time_.InMillisecondsF());
  state->SetBoolean("last_set_needs_begin_frame_",
                    frame_source_->NeedsBeginFrames());
  state->SetInteger("begin_retro_frame_args",
                    static_cast<int>(begin_retro_frame_args_.size()));
  state->SetBoolean("begin_retro_frame_task",
                    !begin_retro_frame_task_.IsCancelled());
  state->SetBoolean("begin_impl_frame_deadline_task",
                    !begin_impl_frame_deadline_task_.IsCancelled());
  state->SetString("inside_action",
                   SchedulerStateMachine::ActionToString(inside_action_));
  state->BeginDictionary("begin_impl_frame_args");
  begin_impl_frame_tracker_.AsValueInto(Now(), state);
  state->EndDictionary();
  state->EndDictionary();

  state->BeginDictionary("client_state");
  state->SetDouble("draw_duration_estimate_ms",
                   client_->DrawDurationEstimate().InMillisecondsF());
  state->SetDouble(
      "begin_main_frame_to_commit_duration_estimate_ms",
      client_->BeginMainFrameToCommitDurationEstimate().InMillisecondsF());
  state->SetDouble(
      "commit_to_activate_duration_estimate_ms",
      client_->CommitToActivateDurationEstimate().InMillisecondsF());
  state->EndDictionary();
}

bool Scheduler::CanCommitAndActivateBeforeDeadline() const {
  BeginFrameArgs args =
      begin_impl_frame_tracker_.DangerousMethodCurrentOrLast();

  // Check if the main thread computation and commit can be finished before the
  // impl thread's deadline.
  base::TimeTicks estimated_draw_time =
      args.frame_time + client_->BeginMainFrameToCommitDurationEstimate() +
      client_->CommitToActivateDurationEstimate();

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
               "CanCommitAndActivateBeforeDeadline",
               "time_left_after_drawing_ms",
               (args.deadline - estimated_draw_time).InMillisecondsF(), "state",
               AsValue());

  return estimated_draw_time < args.deadline;
}

bool Scheduler::IsBeginMainFrameSentOrStarted() const {
  return (state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT ||
          state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED);
}

}  // namespace cc
