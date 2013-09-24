// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <algorithm>
#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/debug/traced_value.h"

namespace cc {

Scheduler::Scheduler(SchedulerClient* client,
                     const SchedulerSettings& scheduler_settings)
    : settings_(scheduler_settings),
      client_(client),
      weak_factory_(this),
      last_set_needs_begin_frame_(false),
      state_machine_(scheduler_settings),
      inside_process_scheduled_actions_(false),
      inside_action_(SchedulerStateMachine::ACTION_NONE) {
  DCHECK(client_);
  DCHECK(!state_machine_.BeginFrameNeededByImplThread());
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
  if (state_machine_.ShouldTriggerBeginFrameDeadlineEarly())
    PostBeginFrameDeadline(base::TimeTicks());
}

void Scheduler::SetNeedsCommit() {
  state_machine_.SetNeedsCommit();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsForcedCommitForReadback() {
  state_machine_.SetNeedsCommit();
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

void Scheduler::SetMainThreadNeedsLayerTextures() {
  state_machine_.SetMainThreadNeedsLayerTextures();
  ProcessScheduledActions();
}

void Scheduler::FinishCommit() {
  TRACE_EVENT0("cc", "Scheduler::FinishCommit");
  state_machine_.FinishCommit();
  ProcessScheduledActions();

  if (state_machine_.ShouldTriggerBeginFrameDeadlineEarly())
    PostBeginFrameDeadline(base::TimeTicks());
}

void Scheduler::BeginFrameAbortedByMainThread(bool did_handle) {
  TRACE_EVENT0("cc", "Scheduler::BeginFrameAbortedByMainThread");
  state_machine_.BeginFrameAbortedByMainThread(did_handle);
  ProcessScheduledActions();
}

void Scheduler::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidLoseOutputSurface");
  last_set_needs_begin_frame_ = false;
  begin_frame_deadline_closure_.Cancel();
  state_machine_.DidLoseOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeOutputSurface");
  DCHECK(!last_set_needs_begin_frame_);
  DCHECK(begin_frame_deadline_closure_.IsCancelled());
  state_machine_.DidCreateAndInitializeOutputSurface();
  ProcessScheduledActions();
}

base::TimeTicks Scheduler::AnticipatedDrawTime() {
  TRACE_EVENT0("cc", "Scheduler::AnticipatedDrawTime");

  if (!last_set_needs_begin_frame_ ||
      last_begin_frame_args_.interval <= base::TimeDelta())
    return base::TimeTicks();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks timebase = std::max(last_begin_frame_args_.frame_time,
                                      last_begin_frame_args_.deadline);
  int64 intervals = 1 + ((now - timebase) / last_begin_frame_args_.interval);
  return timebase + (last_begin_frame_args_.interval * intervals);
}

base::TimeTicks Scheduler::LastBeginFrameOnImplThreadTime() {
  return last_begin_frame_args_.frame_time;
}

void Scheduler::SetupNextBeginFrameIfNeeded() {
  bool needs_begin_frame =
      state_machine_.BeginFrameNeededByImplThread();

  bool at_end_of_deadline =
      state_machine_.begin_frame_state() ==
          SchedulerStateMachine::BEGIN_FRAME_STATE_INSIDE_DEADLINE;

  bool should_call_set_needs_begin_frame =
      // Always request the BeginFrame immediately if it wasn't needed before.
      (needs_begin_frame && !last_set_needs_begin_frame_) ||
      // We always need to explicitly request our next BeginFrame.
      at_end_of_deadline;

  if (should_call_set_needs_begin_frame) {
    client_->SetNeedsBeginFrameOnImplThread(needs_begin_frame);
    last_set_needs_begin_frame_ = needs_begin_frame;
  }

  // Setup PollForAnticipatedDrawTriggers if we need to monitor state but
  // aren't expecting any more BeginFrames. This should only be needed by the
  // synchronous compositor when BeginFrameNeededByImplThread is false.
  if (state_machine_.ShouldPollForAnticipatedDrawTriggers()) {
    DCHECK(settings_.using_synchronous_renderer_compositor);
    DCHECK(!needs_begin_frame);
    if (poll_for_draw_triggers_closure_.IsCancelled()) {
      poll_for_draw_triggers_closure_.Reset(
          base::Bind(&Scheduler::PollForAnticipatedDrawTriggers,
                     weak_factory_.GetWeakPtr()));
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          poll_for_draw_triggers_closure_.callback(),
          last_begin_frame_args_.interval);
    }
  } else {
    poll_for_draw_triggers_closure_.Cancel();
  }
}

void Scheduler::BeginFrame(const BeginFrameArgs& args) {
  TRACE_EVENT0("cc", "Scheduler::BeginFrame");
  DCHECK(state_machine_.begin_frame_state() ==
         SchedulerStateMachine::BEGIN_FRAME_STATE_IDLE);
  DCHECK(state_machine_.HasInitializedOutputSurface());
  last_begin_frame_args_ = args;
  last_begin_frame_args_.deadline -= client_->DrawDurationEstimate();
  state_machine_.OnBeginFrame(last_begin_frame_args_);
  ProcessScheduledActions();

  if (!state_machine_.HasInitializedOutputSurface())
    return;

  state_machine_.OnBeginFrameDeadlinePending();

  if (settings_.using_synchronous_renderer_compositor) {
    // The synchronous renderer compositor has to make its GL calls
    // within this call to BeginFrame.
    // TODO(brianderson): Have the OutputSurface initiate the deadline tasks
    // so the sychronous renderer compoistor can take advantage of splitting
    // up the BeginFrame and deadline as well.
    OnBeginFrameDeadline();
  } else if (!settings_.deadline_scheduling_enabled) {
    // We emulate the old non-deadline scheduler here by posting the
    // deadline task without any delay.
    PostBeginFrameDeadline(base::TimeTicks());
  } else if (state_machine_.ShouldTriggerBeginFrameDeadlineEarly()) {
    // We are ready to draw a new active tree immediately.
    PostBeginFrameDeadline(base::TimeTicks());
  } else if (state_machine_.needs_redraw()) {
    // We have an animation or fast input path on the impl thread that wants
    // to draw, so don't wait too long for a new active tree.
    PostBeginFrameDeadline(last_begin_frame_args_.deadline);
  } else {
    // The impl thread doesn't have anything it wants to draw and we are just
    // waiting for a new active tree, so post the deadline for the next
    // expected BeginFrame start. This allows us to draw immediately when
    // there is a new active tree, instead of waiting for the next BeginFrame.
    // TODO(brianderson): Handle long deadlines (that are past the next frame's
    // frame time) properly instead of using this hack.
    PostBeginFrameDeadline(last_begin_frame_args_.frame_time +
                           last_begin_frame_args_.interval);
  }
}

void Scheduler::PostBeginFrameDeadline(base::TimeTicks deadline) {
  begin_frame_deadline_closure_.Cancel();
  begin_frame_deadline_closure_.Reset(
      base::Bind(&Scheduler::OnBeginFrameDeadline, weak_factory_.GetWeakPtr()));
  client_->PostBeginFrameDeadline(begin_frame_deadline_closure_.callback(),
                                  deadline);
}

void Scheduler::OnBeginFrameDeadline() {
  TRACE_EVENT0("cc", "Scheduler::OnBeginFrameDeadline");
  DCHECK(state_machine_.HasInitializedOutputSurface());
  begin_frame_deadline_closure_.Cancel();
  state_machine_.OnBeginFrameDeadline();
  ProcessScheduledActions();

  if (state_machine_.HasInitializedOutputSurface()) {
    // We only transition out of BEGIN_FRAME_STATE_INSIDE_DEADLINE when all
    // actions that occur back-to-back in response to entering
    // BEGIN_FRAME_STATE_INSIDE_DEADLINE have completed. This is important
    // because sending the BeginFrame to the main thread will not occur if
    // we transition to BEGIN_FRAME_STATE_IDLE too early.
    state_machine_.OnBeginFrameIdle();
  }

  client_->DidBeginFrameDeadlineOnImplThread();
}

void Scheduler::PollForAnticipatedDrawTriggers() {
  TRACE_EVENT0("cc", "Scheduler::PollForAnticipatedDrawTriggers");
  state_machine_.DidEnterPollForAnticipatedDrawTriggers();
  ProcessScheduledActions();
  state_machine_.DidLeavePollForAnticipatedDrawTriggers();

  poll_for_draw_triggers_closure_.Cancel();
}

void Scheduler::DrawAndSwapIfPossible() {
  DrawSwapReadbackResult result =
      client_->ScheduledActionDrawAndSwapIfPossible();
  state_machine_.DidDrawIfPossibleCompleted(result.did_draw);
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
                 TracedValue::FromValue(state_machine_.AsValue().release()));
    state_machine_.UpdateState(action);
    base::AutoReset<SchedulerStateMachine::Action>
        mark_inside_action(&inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::ACTION_NONE:
        break;
      case SchedulerStateMachine::ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD:
        client_->ScheduledActionSendBeginFrameToMainThread();
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

  SetupNextBeginFrameIfNeeded();
  client_->DidAnticipatedDrawTimeChange(AnticipatedDrawTime());
}

bool Scheduler::WillDrawIfNeeded() const {
  return !state_machine_.PendingDrawsShouldBeAborted();
}

}  // namespace cc
