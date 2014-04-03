// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <algorithm>
#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "ui/gfx/frame_time.h"

namespace cc {

Scheduler::Scheduler(
    SchedulerClient* client,
    const SchedulerSettings& scheduler_settings,
    int layer_tree_host_id,
    const scoped_refptr<base::SequencedTaskRunner>& impl_task_runner)
    : settings_(scheduler_settings),
      client_(client),
      layer_tree_host_id_(layer_tree_host_id),
      impl_task_runner_(impl_task_runner),
      last_set_needs_begin_impl_frame_(false),
      state_machine_(scheduler_settings),
      inside_process_scheduled_actions_(false),
      inside_action_(SchedulerStateMachine::ACTION_NONE),
      weak_factory_(this) {
  DCHECK(client_);
  DCHECK(!state_machine_.BeginImplFrameNeeded());
  if (settings_.main_frame_before_activation_enabled) {
    DCHECK(settings_.main_frame_before_draw_enabled);
  }
}

Scheduler::~Scheduler() {}

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

void Scheduler::ActivatePendingTree() {
  client_->ScheduledActionActivatePendingTree();
}

void Scheduler::SetNeedsCommit() {
  state_machine_.SetNeedsCommit();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsForcedCommitForReadback() {
  state_machine_.SetNeedsForcedCommitForReadback();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsRedraw() {
  state_machine_.SetNeedsRedraw();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsManageTiles() {
  DCHECK(!IsInsideAction(SchedulerStateMachine::ACTION_MANAGE_TILES));
  state_machine_.SetNeedsManageTiles();
  ProcessScheduledActions();
}

void Scheduler::SetSwapUsedIncompleteTile(bool used_incomplete_tile) {
  state_machine_.SetSwapUsedIncompleteTile(used_incomplete_tile);
  ProcessScheduledActions();
}

void Scheduler::SetSmoothnessTakesPriority(bool smoothness_takes_priority) {
  state_machine_.SetSmoothnessTakesPriority(smoothness_takes_priority);
  ProcessScheduledActions();
}

void Scheduler::SetMainThreadNeedsLayerTextures() {
  state_machine_.SetMainThreadNeedsLayerTextures();
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToCommit() {
  TRACE_EVENT0("cc", "Scheduler::NotifyReadyToCommit");
  state_machine_.NotifyReadyToCommit();
  ProcessScheduledActions();
}

void Scheduler::BeginMainFrameAborted(bool did_handle) {
  TRACE_EVENT0("cc", "Scheduler::BeginMainFrameAborted");
  state_machine_.BeginMainFrameAborted(did_handle);
  ProcessScheduledActions();
}

void Scheduler::DidManageTiles() {
  state_machine_.DidManageTiles();
}

void Scheduler::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidLoseOutputSurface");
  state_machine_.DidLoseOutputSurface();
  last_set_needs_begin_impl_frame_ = false;
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeOutputSurface");
  DCHECK(!last_set_needs_begin_impl_frame_);
  DCHECK(begin_impl_frame_deadline_closure_.IsCancelled());
  state_machine_.DidCreateAndInitializeOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::NotifyBeginMainFrameStarted() {
  TRACE_EVENT0("cc", "Scheduler::NotifyBeginMainFrameStarted");
  state_machine_.NotifyBeginMainFrameStarted();
}

base::TimeTicks Scheduler::AnticipatedDrawTime() const {
  if (!last_set_needs_begin_impl_frame_ ||
      last_begin_impl_frame_args_.interval <= base::TimeDelta())
    return base::TimeTicks();

  base::TimeTicks now = gfx::FrameTime::Now();
  base::TimeTicks timebase = std::max(last_begin_impl_frame_args_.frame_time,
                                      last_begin_impl_frame_args_.deadline);
  int64 intervals =
      1 + ((now - timebase) / last_begin_impl_frame_args_.interval);
  return timebase + (last_begin_impl_frame_args_.interval * intervals);
}

base::TimeTicks Scheduler::LastBeginImplFrameTime() {
  return last_begin_impl_frame_args_.frame_time;
}

void Scheduler::SetupNextBeginImplFrameIfNeeded() {
  bool needs_begin_impl_frame =
      state_machine_.BeginImplFrameNeeded();

  bool at_end_of_deadline =
      state_machine_.begin_impl_frame_state() ==
          SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE;

  bool should_call_set_needs_begin_impl_frame =
      // Always request the BeginImplFrame immediately if it wasn't needed
      // before.
      (needs_begin_impl_frame && !last_set_needs_begin_impl_frame_) ||
      // We always need to explicitly request our next BeginImplFrame.
      at_end_of_deadline;

  if (should_call_set_needs_begin_impl_frame) {
    client_->SetNeedsBeginImplFrame(needs_begin_impl_frame);
    last_set_needs_begin_impl_frame_ = needs_begin_impl_frame;
  }

  bool needs_advance_commit_state_timer = false;
  // Setup PollForAnticipatedDrawTriggers if we need to monitor state but
  // aren't expecting any more BeginImplFrames. This should only be needed by
  // the synchronous compositor when BeginImplFrameNeeded is false.
  if (state_machine_.ShouldPollForAnticipatedDrawTriggers()) {
    DCHECK(!state_machine_.SupportsProactiveBeginImplFrame());
    DCHECK(!needs_begin_impl_frame);
    if (poll_for_draw_triggers_closure_.IsCancelled()) {
      poll_for_draw_triggers_closure_.Reset(
          base::Bind(&Scheduler::PollForAnticipatedDrawTriggers,
                     weak_factory_.GetWeakPtr()));
      base::TimeDelta delay = last_begin_impl_frame_args_.IsValid()
                                  ? last_begin_impl_frame_args_.interval
                                  : BeginFrameArgs::DefaultInterval();
      impl_task_runner_->PostDelayedTask(
          FROM_HERE, poll_for_draw_triggers_closure_.callback(), delay);
    }
  } else {
    poll_for_draw_triggers_closure_.Cancel();

    // At this point we'd prefer to advance through the commit flow by
    // drawing a frame, however it's possible that the frame rate controller
    // will not give us a BeginImplFrame until the commit completes.  See
    // crbug.com/317430 for an example of a swap ack being held on commit. Thus
    // we set a repeating timer to poll on ProcessScheduledActions until we
    // successfully reach BeginImplFrame. Synchronous compositor does not use
    // frame rate controller or have the circular wait in the bug.
    if (IsBeginMainFrameSentOrStarted() &&
        !settings_.using_synchronous_renderer_compositor) {
      needs_advance_commit_state_timer = true;
    }
  }
  if (needs_advance_commit_state_timer !=
      advance_commit_state_timer_.IsRunning()) {
    if (needs_advance_commit_state_timer &&
        last_begin_impl_frame_args_.IsValid()) {
      // Since we'd rather get a BeginImplFrame by the normal mechanism, we
      // set the interval to twice the interval from the previous frame.
      advance_commit_state_timer_.Start(
          FROM_HERE,
          last_begin_impl_frame_args_.interval * 2,
          base::Bind(&Scheduler::ProcessScheduledActions,
                     base::Unretained(this)));
    } else {
      advance_commit_state_timer_.Stop();
    }
  }
}

void Scheduler::BeginImplFrame(const BeginFrameArgs& args) {
  TRACE_EVENT0("cc", "Scheduler::BeginImplFrame");
  DCHECK(state_machine_.begin_impl_frame_state() ==
         SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  DCHECK(state_machine_.HasInitializedOutputSurface());

  last_begin_impl_frame_args_ = args;
  last_begin_impl_frame_args_.deadline -= client_->DrawDurationEstimate();

  if (!state_machine_.smoothness_takes_priority() &&
      state_machine_.MainThreadIsInHighLatencyMode() &&
      CanCommitAndActivateBeforeDeadline()) {
    state_machine_.SetSkipNextBeginMainFrameToReduceLatency();
  }

  state_machine_.OnBeginImplFrame(last_begin_impl_frame_args_);
  devtools_instrumentation::DidBeginFrame(layer_tree_host_id_);

  ProcessScheduledActions();

  if (!state_machine_.HasInitializedOutputSurface())
    return;

  state_machine_.OnBeginImplFrameDeadlinePending();
  base::TimeTicks adjusted_deadline = AdjustedBeginImplFrameDeadline();
  ScheduleBeginImplFrameDeadline(adjusted_deadline);
}

base::TimeTicks Scheduler::AdjustedBeginImplFrameDeadline() const {
  if (settings_.using_synchronous_renderer_compositor) {
    // The synchronous compositor needs to draw right away.
    return base::TimeTicks();
  } else if (state_machine_.ShouldTriggerBeginImplFrameDeadlineEarly()) {
    // We are ready to draw a new active tree immediately.
    return base::TimeTicks();
  } else if (state_machine_.needs_redraw()) {
    // We have an animation or fast input path on the impl thread that wants
    // to draw, so don't wait too long for a new active tree.
    return last_begin_impl_frame_args_.deadline;
  } else {
    // The impl thread doesn't have anything it wants to draw and we are just
    // waiting for a new active tree, so post the deadline for the next
    // expected BeginImplFrame start. This allows us to draw immediately when
    // there is a new active tree, instead of waiting for the next
    // BeginImplFrame.
    // TODO(brianderson): Handle long deadlines (that are past the next frame's
    // frame time) properly instead of using this hack.
    return last_begin_impl_frame_args_.frame_time +
           last_begin_impl_frame_args_.interval;
  }
}

void Scheduler::ScheduleBeginImplFrameDeadline(base::TimeTicks deadline) {
  if (settings_.using_synchronous_renderer_compositor) {
    // The synchronous renderer compositor has to make its GL calls
    // within this call.
    // TODO(brianderson): Have the OutputSurface initiate the deadline tasks
    // so the sychronous renderer compositor can take advantage of splitting
    // up the BeginImplFrame and deadline as well.
    OnBeginImplFrameDeadline();
    return;
  }
  begin_impl_frame_deadline_closure_.Cancel();
  begin_impl_frame_deadline_closure_.Reset(
      base::Bind(&Scheduler::OnBeginImplFrameDeadline,
                 weak_factory_.GetWeakPtr()));

  base::TimeDelta delta = deadline - gfx::FrameTime::Now();
  if (delta <= base::TimeDelta())
    delta = base::TimeDelta();
  impl_task_runner_->PostDelayedTask(
      FROM_HERE, begin_impl_frame_deadline_closure_.callback(), delta);
}

void Scheduler::OnBeginImplFrameDeadline() {
  TRACE_EVENT0("cc", "Scheduler::OnBeginImplFrameDeadline");
  begin_impl_frame_deadline_closure_.Cancel();

  // We split the deadline actions up into two phases so the state machine
  // has a chance to trigger actions that should occur durring and after
  // the deadline separately. For example:
  // * Sending the BeginMainFrame will not occur after the deadline in
  //     order to wait for more user-input before starting the next commit.
  // * Creating a new OuputSurface will not occur during the deadline in
  //     order to allow the state machine to "settle" first.
  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();
  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();

  client_->DidBeginImplFrameDeadline();
}

void Scheduler::PollForAnticipatedDrawTriggers() {
  TRACE_EVENT0("cc", "Scheduler::PollForAnticipatedDrawTriggers");
  poll_for_draw_triggers_closure_.Cancel();
  state_machine_.DidEnterPollForAnticipatedDrawTriggers();
  ProcessScheduledActions();
  state_machine_.DidLeavePollForAnticipatedDrawTriggers();
}

bool Scheduler::IsBeginMainFrameSent() const {
  return state_machine_.commit_state() ==
         SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT;
}

void Scheduler::DrawAndSwapIfPossible() {
  DrawSwapReadbackResult result =
      client_->ScheduledActionDrawAndSwapIfPossible();
  state_machine_.DidDrawIfPossibleCompleted(result.draw_result);
}

void Scheduler::DrawAndSwapForced() {
  client_->ScheduledActionDrawAndSwapForced();
}

void Scheduler::DrawAndReadback() {
  DrawSwapReadbackResult result = client_->ScheduledActionDrawAndReadback();
  DCHECK(!result.did_swap);
}

void Scheduler::ProcessScheduledActions() {
  // We do not allow ProcessScheduledActions to be recursive.
  // The top-level call will iteratively execute the next action for us anyway.
  if (inside_process_scheduled_actions_)
    return;

  base::AutoReset<bool> mark_inside(&inside_process_scheduled_actions_, true);

  SchedulerStateMachine::Action action;
  do {
    state_machine_.CheckInvariants();
    action = state_machine_.NextAction();
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "SchedulerStateMachine",
                 "state",
                 TracedValue::FromValue(StateAsValue().release()));
    state_machine_.UpdateState(action);
    base::AutoReset<SchedulerStateMachine::Action>
        mark_inside_action(&inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::ACTION_NONE:
        break;
      case SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME:
        client_->ScheduledActionSendBeginMainFrame();
        break;
      case SchedulerStateMachine::ACTION_COMMIT:
        client_->ScheduledActionCommit();
        break;
      case SchedulerStateMachine::ACTION_UPDATE_VISIBLE_TILES:
        client_->ScheduledActionUpdateVisibleTiles();
        break;
      case SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE:
        ActivatePendingTree();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE:
        DrawAndSwapIfPossible();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_FORCED:
        DrawAndSwapForced();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT:
        // No action is actually performed, but this allows the state machine to
        // advance out of its waiting to draw state without actually drawing.
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_READBACK:
        DrawAndReadback();
        break;
      case SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
        client_->ScheduledActionBeginOutputSurfaceCreation();
        break;
      case SchedulerStateMachine::ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
        client_->ScheduledActionAcquireLayerTexturesForMainThread();
        break;
      case SchedulerStateMachine::ACTION_MANAGE_TILES:
        client_->ScheduledActionManageTiles();
        break;
    }
  } while (action != SchedulerStateMachine::ACTION_NONE);

  SetupNextBeginImplFrameIfNeeded();
  client_->DidAnticipatedDrawTimeChange(AnticipatedDrawTime());

  if (state_machine_.ShouldTriggerBeginImplFrameDeadlineEarly()) {
    DCHECK(!settings_.using_synchronous_renderer_compositor);
    ScheduleBeginImplFrameDeadline(base::TimeTicks());
  }
}

bool Scheduler::WillDrawIfNeeded() const {
  return !state_machine_.PendingDrawsShouldBeAborted();
}

scoped_ptr<base::Value> Scheduler::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);
  state->Set("state_machine", state_machine_.AsValue().release());

  scoped_ptr<base::DictionaryValue> scheduler_state(new base::DictionaryValue);
  scheduler_state->SetDouble(
      "time_until_anticipated_draw_time_ms",
      (AnticipatedDrawTime() - base::TimeTicks::Now()).InMillisecondsF());
  scheduler_state->SetBoolean("last_set_needs_begin_impl_frame_",
                              last_set_needs_begin_impl_frame_);
  scheduler_state->SetBoolean(
      "begin_impl_frame_deadline_closure_",
      !begin_impl_frame_deadline_closure_.IsCancelled());
  scheduler_state->SetBoolean("poll_for_draw_triggers_closure_",
                              !poll_for_draw_triggers_closure_.IsCancelled());
  scheduler_state->SetBoolean("advance_commit_state_timer_",
                              advance_commit_state_timer_.IsRunning());
  state->Set("scheduler_state", scheduler_state.release());

  scoped_ptr<base::DictionaryValue> client_state(new base::DictionaryValue);
  client_state->SetDouble("draw_duration_estimate_ms",
                          client_->DrawDurationEstimate().InMillisecondsF());
  client_state->SetDouble(
      "begin_main_frame_to_commit_duration_estimate_ms",
      client_->BeginMainFrameToCommitDurationEstimate().InMillisecondsF());
  client_state->SetDouble(
      "commit_to_activate_duration_estimate_ms",
      client_->CommitToActivateDurationEstimate().InMillisecondsF());
  state->Set("client_state", client_state.release());
  return state.PassAs<base::Value>();
}

bool Scheduler::CanCommitAndActivateBeforeDeadline() const {
  // Check if the main thread computation and commit can be finished before the
  // impl thread's deadline.
  base::TimeTicks estimated_draw_time =
      last_begin_impl_frame_args_.frame_time +
      client_->BeginMainFrameToCommitDurationEstimate() +
      client_->CommitToActivateDurationEstimate();

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
               "CanCommitAndActivateBeforeDeadline",
               "time_left_after_drawing_ms",
               (last_begin_impl_frame_args_.deadline - estimated_draw_time)
                   .InMillisecondsF(),
               "state",
               TracedValue::FromValue(StateAsValue().release()));

  return estimated_draw_time < last_begin_impl_frame_args_.deadline;
}

bool Scheduler::IsBeginMainFrameSentOrStarted() const {
  return (state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT ||
          state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED);
}

}  // namespace cc
