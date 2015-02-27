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
#include "ui/gfx/frame_time.h"

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
SchedulerFrameSourcesConstructor::ConstructBackgroundFrameSource(
    Scheduler* scheduler) {
  TRACE_EVENT1("cc",
               "Scheduler::Scheduler()",
               "BackgroundFrameSource",
               "SyntheticBeginFrameSource");
  DCHECK(!(scheduler->background_frame_source_internal_));
  scheduler->background_frame_source_internal_ =
      SyntheticBeginFrameSource::Create(
          scheduler->task_runner_.get(), scheduler->Now(),
          scheduler->settings_.background_frame_interval);
  return scheduler->background_frame_source_internal_.get();
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
    base::PowerMonitor* power_monitor,
    scoped_ptr<BeginFrameSource> external_begin_frame_source,
    SchedulerFrameSourcesConstructor* frame_sources_constructor)
    : frame_source_(),
      primary_frame_source_(NULL),
      background_frame_source_(NULL),
      primary_frame_source_internal_(external_begin_frame_source.Pass()),
      background_frame_source_internal_(),
      vsync_observer_(NULL),
      throttle_frame_production_(scheduler_settings.throttle_frame_production),
      settings_(scheduler_settings),
      client_(client),
      layer_tree_host_id_(layer_tree_host_id),
      task_runner_(task_runner),
      power_monitor_(power_monitor),
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
  poll_for_draw_triggers_closure_ = base::Bind(
      &Scheduler::PollForAnticipatedDrawTriggers, weak_factory_.GetWeakPtr());
  advance_commit_state_closure_ = base::Bind(
      &Scheduler::PollToAdvanceCommitState, weak_factory_.GetWeakPtr());

  frame_source_ = BeginFrameSourceMultiplexer::Create();
  frame_source_->AddObserver(this);

  // Primary frame source
  primary_frame_source_ =
      frame_sources_constructor->ConstructPrimaryFrameSource(this);
  frame_source_->AddSource(primary_frame_source_);
  primary_frame_source_->SetClientReady();

  // Background ticking frame source
  background_frame_source_ =
      frame_sources_constructor->ConstructBackgroundFrameSource(this);
  frame_source_->AddSource(background_frame_source_);

  // Unthrottled frame source
  unthrottled_frame_source_ =
      frame_sources_constructor->ConstructUnthrottledFrameSource(this);
  frame_source_->AddSource(unthrottled_frame_source_);

  SetupPowerMonitoring();
}

Scheduler::~Scheduler() {
  TeardownPowerMonitoring();
  if (frame_source_->NeedsBeginFrames())
    frame_source_->SetNeedsBeginFrames(false);
}

base::TimeTicks Scheduler::Now() const {
  base::TimeTicks now = gfx::FrameTime::Now();
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.now"),
               "Scheduler::Now",
               "now",
               now);
  return now;
}

void Scheduler::SetupPowerMonitoring() {
  if (settings_.disable_hi_res_timer_tasks_on_battery) {
    DCHECK(power_monitor_);
    power_monitor_->AddObserver(this);
    state_machine_.SetImplLatencyTakesPriorityOnBattery(
        power_monitor_->IsOnBatteryPower());
  }
}

void Scheduler::TeardownPowerMonitoring() {
  if (settings_.disable_hi_res_timer_tasks_on_battery) {
    DCHECK(power_monitor_);
    power_monitor_->RemoveObserver(this);
  }
}

void Scheduler::OnPowerStateChange(bool on_battery_power) {
  DCHECK(settings_.disable_hi_res_timer_tasks_on_battery);
  state_machine_.SetImplLatencyTakesPriorityOnBattery(on_battery_power);
}

void Scheduler::CommitVSyncParameters(base::TimeTicks timebase,
                                      base::TimeDelta interval) {
  // TODO(brianderson): We should not be receiving 0 intervals.
  if (interval == base::TimeDelta())
    interval = BeginFrameArgs::DefaultInterval();

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

void Scheduler::UpdateActiveFrameSource() {
  if (state_machine_.visible()) {
    if (throttle_frame_production_) {
      frame_source_->SetActiveSource(primary_frame_source_);
    } else {
      frame_source_->SetActiveSource(unthrottled_frame_source_);
    }
  } else {
    frame_source_->SetActiveSource(background_frame_source_);
  }
  ProcessScheduledActions();
}

void Scheduler::SetVisible(bool visible) {
  state_machine_.SetVisible(visible);
  UpdateActiveFrameSource();
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
  // Empty for now, until we take action based on the notification as part of
  // crbugs 352894, 383157, 421923.
}

void Scheduler::SetThrottleFrameProduction(bool throttle) {
  throttle_frame_production_ = throttle;
  UpdateActiveFrameSource();
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

base::TimeTicks Scheduler::AnticipatedDrawTime() const {
  if (!frame_source_->NeedsBeginFrames() ||
      begin_impl_frame_args_.interval <= base::TimeDelta())
    return base::TimeTicks();

  base::TimeTicks now = Now();
  base::TimeTicks timebase = std::max(begin_impl_frame_args_.frame_time,
                                      begin_impl_frame_args_.deadline);
  int64 intervals = 1 + ((now - timebase) / begin_impl_frame_args_.interval);
  return timebase + (begin_impl_frame_args_.interval * intervals);
}

base::TimeTicks Scheduler::LastBeginImplFrameTime() {
  return begin_impl_frame_args_.frame_time;
}

void Scheduler::SetupNextBeginFrameIfNeeded() {
  if (!task_runner_.get())
    return;

  if (state_machine_.ShouldSetNeedsBeginFrames(
          frame_source_->NeedsBeginFrames())) {
    frame_source_->SetNeedsBeginFrames(state_machine_.BeginFrameNeeded());
    if (!frame_source_->NeedsBeginFrames()) {
      client_->SendBeginMainFrameNotExpectedSoon();
    }
  }

  if (state_machine_.begin_impl_frame_state() ==
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE) {
    frame_source_->DidFinishFrame(begin_retro_frame_args_.size());
  }

  PostBeginRetroFrameIfNeeded();
  SetupPollingMechanisms();
}

// We may need to poll when we can't rely on BeginFrame to advance certain
// state or to avoid deadlock.
void Scheduler::SetupPollingMechanisms() {
  bool needs_advance_commit_state_timer = false;
  // Setup PollForAnticipatedDrawTriggers if we need to monitor state but
  // aren't expecting any more BeginFrames. This should only be needed by
  // the synchronous compositor when BeginFrameNeeded is false.
  if (state_machine_.ShouldPollForAnticipatedDrawTriggers()) {
    DCHECK(!state_machine_.SupportsProactiveBeginFrame());
    if (poll_for_draw_triggers_task_.IsCancelled()) {
      poll_for_draw_triggers_task_.Reset(poll_for_draw_triggers_closure_);
      base::TimeDelta delay = begin_impl_frame_args_.IsValid()
                                  ? begin_impl_frame_args_.interval
                                  : BeginFrameArgs::DefaultInterval();
      task_runner_->PostDelayedTask(
          FROM_HERE, poll_for_draw_triggers_task_.callback(), delay);
    }
  } else {
    poll_for_draw_triggers_task_.Cancel();

    // At this point we'd prefer to advance through the commit flow by
    // drawing a frame, however it's possible that the frame rate controller
    // will not give us a BeginFrame until the commit completes.  See
    // crbug.com/317430 for an example of a swap ack being held on commit. Thus
    // we set a repeating timer to poll on ProcessScheduledActions until we
    // successfully reach BeginFrame. Synchronous compositor does not use
    // frame rate controller or have the circular wait in the bug.
    if (IsBeginMainFrameSentOrStarted() &&
        !settings_.using_synchronous_renderer_compositor) {
      needs_advance_commit_state_timer = true;
    }
  }

  if (needs_advance_commit_state_timer) {
    if (advance_commit_state_task_.IsCancelled() &&
        begin_impl_frame_args_.IsValid()) {
      // Since we'd rather get a BeginImplFrame by the normal mechanism, we
      // set the interval to twice the interval from the previous frame.
      advance_commit_state_task_.Reset(advance_commit_state_closure_);
      task_runner_->PostDelayedTask(FROM_HERE,
                                    advance_commit_state_task_.callback(),
                                    begin_impl_frame_args_.interval * 2);
    }
  } else {
    advance_commit_state_task_.Cancel();
  }
}

// BeginFrame is the mechanism that tells us that now is a good time to start
// making a frame. Usually this means that user input for the frame is complete.
// If the scheduler is busy, we queue the BeginFrame to be handled later as
// a BeginRetroFrame.
bool Scheduler::OnBeginFrameMixInDelegate(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginFrame", "args", args.AsValue());

  // Deliver BeginFrames to children.
  if (settings_.forward_begin_frames_to_children &&
      state_machine_.children_need_begin_frames()) {
    BeginFrameArgs adjusted_args_for_children(args);
    // Adjust a deadline for child schedulers.
    // TODO(simonhong): Once we have commitless update, we can get rid of
    // BeginMainFrameToCommitDurationEstimate() +
    // CommitToActivateDurationEstimate().
    adjusted_args_for_children.deadline -=
        (client_->BeginMainFrameToCommitDurationEstimate() +
         client_->CommitToActivateDurationEstimate() +
         client_->DrawDurationEstimate() + EstimatedParentDrawTime());
    client_->SendBeginFramesToChildren(adjusted_args_for_children);
  }

  // We have just called SetNeedsBeginFrame(true) and the BeginFrameSource has
  // sent us the last BeginFrame we have missed. As we might not be able to
  // actually make rendering for this call, handle it like a "retro frame".
  // TODO(brainderson): Add a test for this functionality ASAP!
  if (args.type == BeginFrameArgs::MISSED) {
    begin_retro_frame_args_.push_back(args);
    PostBeginRetroFrameIfNeeded();
    return true;
  }

  BeginFrameArgs adjusted_args(args);
  adjusted_args.deadline -= EstimatedParentDrawTime();

  bool should_defer_begin_frame;
  if (settings_.using_synchronous_renderer_compositor) {
    should_defer_begin_frame = false;
  } else {
    should_defer_begin_frame =
        !begin_retro_frame_args_.empty() ||
        !begin_retro_frame_task_.IsCancelled() ||
        !frame_source_->NeedsBeginFrames() ||
        (state_machine_.begin_impl_frame_state() !=
         SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  }

  if (should_defer_begin_frame) {
    begin_retro_frame_args_.push_back(adjusted_args);
    TRACE_EVENT_INSTANT0(
        "cc", "Scheduler::BeginFrame deferred", TRACE_EVENT_SCOPE_THREAD);
    // Queuing the frame counts as "using it", so we need to return true.
  } else {
    BeginImplFrame(adjusted_args);
  }
  return true;
}

void Scheduler::SetChildrenNeedBeginFrames(bool children_need_begin_frames) {
  DCHECK(settings_.forward_begin_frames_to_children);
  state_machine_.SetChildrenNeedBeginFrames(children_need_begin_frames);
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
    base::TimeTicks expiration_time = args.frame_time + args.interval;
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
    BeginImplFrame(front);
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

// BeginImplFrame starts a compositor frame that will wait up until a deadline
// for a BeginMainFrame+activation to complete before it times out and draws
// any asynchronous animation and scroll/pinch updates.
void Scheduler::BeginImplFrame(const BeginFrameArgs& args) {
  bool main_thread_is_in_high_latency_mode =
      state_machine_.MainThreadIsInHighLatencyMode();
  TRACE_EVENT2("cc,benchmark",
               "Scheduler::BeginImplFrame",
               "args",
               args.AsValue(),
               "main_thread_is_high_latency",
               main_thread_is_in_high_latency_mode);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "MainThreadLatency",
                 main_thread_is_in_high_latency_mode);
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  DCHECK(state_machine_.HasInitializedOutputSurface());

  advance_commit_state_task_.Cancel();

  begin_impl_frame_args_ = args;
  begin_impl_frame_args_.deadline -= client_->DrawDurationEstimate();

  if (!state_machine_.impl_latency_takes_priority() &&
      main_thread_is_in_high_latency_mode &&
      CanCommitAndActivateBeforeDeadline()) {
    state_machine_.SetSkipNextBeginMainFrameToReduceLatency();
  }

  state_machine_.OnBeginImplFrame(begin_impl_frame_args_);
  devtools_instrumentation::DidBeginFrame(layer_tree_host_id_);
  client_->WillBeginImplFrame(begin_impl_frame_args_);

  ProcessScheduledActions();

  state_machine_.OnBeginImplFrameDeadlinePending();

  if (settings_.using_synchronous_renderer_compositor) {
    // The synchronous renderer compositor has to make its GL calls
    // within this call.
    // TODO(brianderson): Have the OutputSurface initiate the deadline tasks
    // so the synchronous renderer compositor can take advantage of splitting
    // up the BeginImplFrame and deadline as well.
    OnBeginImplFrameDeadline();
  } else {
    ScheduleBeginImplFrameDeadline();
  }
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
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_IMMEDIATE:
      // We are ready to draw a new active tree immediately.
      // We don't use Now() here because it's somewhat expensive to call.
      deadline = base::TimeTicks();
      break;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_REGULAR:
      // We are animating on the impl thread but we can wait for some time.
      deadline = begin_impl_frame_args_.deadline;
      break;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_LATE:
      // We are blocked for one reason or another and we should wait.
      // TODO(brianderson): Handle long deadlines (that are past the next
      // frame's frame time) properly instead of using this hack.
      deadline =
          begin_impl_frame_args_.frame_time + begin_impl_frame_args_.interval;
      break;
  }

  TRACE_EVENT1(
      "cc", "Scheduler::ScheduleBeginImplFrameDeadline", "deadline", deadline);

  base::TimeDelta delta = deadline - Now();
  if (delta <= base::TimeDelta())
    delta = base::TimeDelta();
  task_runner_->PostDelayedTask(
      FROM_HERE, begin_impl_frame_deadline_task_.callback(), delta);
}

void Scheduler::RescheduleBeginImplFrameDeadlineIfNeeded() {
  if (settings_.using_synchronous_renderer_compositor)
    return;

  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME)
    return;

  if (begin_impl_frame_deadline_mode_ !=
      state_machine_.CurrentBeginImplFrameDeadlineMode())
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

  // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461509 Scheduler::OnBeginImplFrameDeadline2"));
  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();

  // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461509 Scheduler::OnBeginImplFrameDeadline3"));
  client_->DidBeginImplFrameDeadline();
}

void Scheduler::PollForAnticipatedDrawTriggers() {
  TRACE_EVENT0("cc", "Scheduler::PollForAnticipatedDrawTriggers");
  poll_for_draw_triggers_task_.Cancel();
  state_machine_.DidEnterPollForAnticipatedDrawTriggers();
  ProcessScheduledActions();
  state_machine_.DidLeavePollForAnticipatedDrawTriggers();
}

void Scheduler::PollToAdvanceCommitState() {
  TRACE_EVENT0("cc", "Scheduler::PollToAdvanceCommitState");
  advance_commit_state_task_.Cancel();
  ProcessScheduledActions();
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
    // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is fixed.
    tracked_objects::ScopedTracker tracking_profile1(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "461509 Scheduler::ProcessScheduledActions1"));
    action = state_machine_.NextAction();
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "SchedulerStateMachine",
                 "state",
                 AsValue());
    VLOG(2) << "Scheduler::ProcessScheduledActions: "
            << SchedulerStateMachine::ActionToString(action) << " "
            << state_machine_.GetStatesForDebugging();
    state_machine_.UpdateState(action);
    base::AutoReset<SchedulerStateMachine::Action>
        mark_inside_action(&inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::ACTION_NONE:
        break;
      case SchedulerStateMachine::ACTION_ANIMATE: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile2(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions2"));
        client_->ScheduledActionAnimate();
        break;
      }
      case SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile3(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions3"));
        client_->ScheduledActionSendBeginMainFrame();
        break;
      }
      case SchedulerStateMachine::ACTION_COMMIT: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile4(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions4"));
        client_->ScheduledActionCommit();
        break;
      }
      case SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile5(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions5"));
        client_->ScheduledActionActivateSyncTree();
        break;
      }
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile6(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions6"));
        DrawAndSwapIfPossible();
        break;
      }
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_FORCED: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile7(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions7"));
        client_->ScheduledActionDrawAndSwapForced();
        break;
      }
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT:
        // No action is actually performed, but this allows the state machine to
        // advance out of its waiting to draw state without actually drawing.
        break;
      case SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile8(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions8"));
        client_->ScheduledActionBeginOutputSurfaceCreation();
        break;
      }
      case SchedulerStateMachine::ACTION_PREPARE_TILES: {
        // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is
        // fixed.
        tracked_objects::ScopedTracker tracking_profile9(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "461509 Scheduler::ProcessScheduledActions9"));
        client_->ScheduledActionPrepareTiles();
        break;
      }
    }
  } while (action != SchedulerStateMachine::ACTION_NONE);

  // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is fixed.
  tracked_objects::ScopedTracker tracking_profile10(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461509 Scheduler::ProcessScheduledActions10"));
  SetupNextBeginFrameIfNeeded();
  client_->DidAnticipatedDrawTimeChange(AnticipatedDrawTime());

  // TODO(robliao): Remove ScopedTracker below once crbug.com/461509 is fixed.
  tracked_objects::ScopedTracker tracking_profile11(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461509 Scheduler::ProcessScheduledActions11"));
  RescheduleBeginImplFrameDeadlineIfNeeded();
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
  state_machine_.AsValueInto(state, Now());
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
  state->SetDouble("time_until_anticipated_draw_time_ms",
                   (AnticipatedDrawTime() - Now()).InMillisecondsF());
  state->SetDouble("estimated_parent_draw_time_ms",
                   estimated_parent_draw_time_.InMillisecondsF());
  state->SetBoolean("last_set_needs_begin_frame_",
                    frame_source_->NeedsBeginFrames());
  state->SetInteger("begin_retro_frame_args_", begin_retro_frame_args_.size());
  state->SetBoolean("begin_retro_frame_task_",
                    !begin_retro_frame_task_.IsCancelled());
  state->SetBoolean("begin_impl_frame_deadline_task_",
                    !begin_impl_frame_deadline_task_.IsCancelled());
  state->SetBoolean("poll_for_draw_triggers_task_",
                    !poll_for_draw_triggers_task_.IsCancelled());
  state->SetBoolean("advance_commit_state_task_",
                    !advance_commit_state_task_.IsCancelled());
  state->BeginDictionary("begin_impl_frame_args");
  begin_impl_frame_args_.AsValueInto(state);
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
  // Check if the main thread computation and commit can be finished before the
  // impl thread's deadline.
  base::TimeTicks estimated_draw_time =
      begin_impl_frame_args_.frame_time +
      client_->BeginMainFrameToCommitDurationEstimate() +
      client_->CommitToActivateDurationEstimate();

  TRACE_EVENT2(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
      "CanCommitAndActivateBeforeDeadline",
      "time_left_after_drawing_ms",
      (begin_impl_frame_args_.deadline - estimated_draw_time).InMillisecondsF(),
      "state",
      AsValue());

  return estimated_draw_time < begin_impl_frame_args_.deadline;
}

bool Scheduler::IsBeginMainFrameSentOrStarted() const {
  return (state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT ||
          state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED);
}

}  // namespace cc
