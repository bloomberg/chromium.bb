// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

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
      has_pending_begin_frame_(false),
      safe_to_expect_begin_frame_(false),
      state_machine_(scheduler_settings),
      inside_process_scheduled_actions_(false) {
  DCHECK(client_);
  DCHECK(!state_machine_.BeginFrameNeededToDrawByImplThread());
}

Scheduler::~Scheduler() {
  client_->SetNeedsBeginFrameOnImplThread(false);
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

void Scheduler::DidSwapUseIncompleteTile() {
  state_machine_.DidSwapUseIncompleteTile();
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
}

void Scheduler::BeginFrameAbortedByMainThread(bool did_handle) {
  TRACE_EVENT0("cc", "Scheduler::BeginFrameAbortedByMainThread");
  state_machine_.BeginFrameAbortedByMainThread(did_handle);
  ProcessScheduledActions();
}

void Scheduler::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidLoseOutputSurface");
  state_machine_.DidLoseOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeOutputSurface");
  state_machine_.DidCreateAndInitializeOutputSurface();
  has_pending_begin_frame_ = false;
  last_set_needs_begin_frame_ = false;
  safe_to_expect_begin_frame_ = false;
  ProcessScheduledActions();
}

base::TimeTicks Scheduler::AnticipatedDrawTime() {
  TRACE_EVENT0("cc", "Scheduler::AnticipatedDrawTime");

  if (!last_set_needs_begin_frame_ ||
      last_begin_frame_args_.interval <= base::TimeDelta())
    return base::TimeTicks();

  // TODO(brianderson): Express this in terms of the deadline.
  base::TimeTicks now = base::TimeTicks::Now();
  int64 intervals = 1 + ((now - last_begin_frame_args_.frame_time) /
                         last_begin_frame_args_.interval);
  return last_begin_frame_args_.frame_time +
      (last_begin_frame_args_.interval * intervals);
}

base::TimeTicks Scheduler::LastBeginFrameOnImplThreadTime() {
  return last_begin_frame_args_.frame_time;
}

void Scheduler::SetupNextBeginFrameIfNeeded() {
  bool needs_begin_frame_to_draw =
      state_machine_.BeginFrameNeededToDrawByImplThread();
  // We want to avoid proactive begin frames with the synchronous compositor
  // because every SetNeedsBeginFrame will force a redraw.
  bool proactive_begin_frame_wanted =
      state_machine_.ProactiveBeginFrameWantedByImplThread() &&
      !settings_.using_synchronous_renderer_compositor &&
      settings_.throttle_frame_production;
  bool needs_begin_frame = needs_begin_frame_to_draw ||
                           proactive_begin_frame_wanted;
  bool immediate_disables_needed =
      settings_.using_synchronous_renderer_compositor;

  if (needs_begin_frame_to_draw)
    safe_to_expect_begin_frame_ = true;

  // Determine if we need BeginFrame notifications.
  // If we do, always request the BeginFrame immediately.
  // If not, only disable on the next BeginFrame to avoid unnecessary toggles.
  // The synchronous renderer compositor requires immediate disables though.
  if ((needs_begin_frame ||
       state_machine_.inside_begin_frame() ||
       immediate_disables_needed) &&
      (needs_begin_frame != last_set_needs_begin_frame_)) {
    has_pending_begin_frame_ = false;
    client_->SetNeedsBeginFrameOnImplThread(needs_begin_frame);
    if (safe_to_expect_begin_frame_)
      last_set_needs_begin_frame_ = needs_begin_frame;
  }

  // Request another BeginFrame if we haven't drawn for now until we have
  // deadlines implemented.
  if (state_machine_.inside_begin_frame() && has_pending_begin_frame_) {
    has_pending_begin_frame_ = false;
    client_->SetNeedsBeginFrameOnImplThread(true);
    return;
  }
}

void Scheduler::BeginFrame(const BeginFrameArgs& args) {
  TRACE_EVENT0("cc", "Scheduler::BeginFrame");
  DCHECK(!has_pending_begin_frame_);
  has_pending_begin_frame_ = true;
  safe_to_expect_begin_frame_ = true;
  last_begin_frame_args_ = args;
  state_machine_.DidEnterBeginFrame(args);
  ProcessScheduledActions();
  state_machine_.DidLeaveBeginFrame();
}

void Scheduler::DrawAndSwapIfPossible() {
  DrawSwapReadbackResult result =
      client_->ScheduledActionDrawAndSwapIfPossible();
  state_machine_.DidDrawIfPossibleCompleted(result.did_draw);
  if (result.did_swap)
    has_pending_begin_frame_ = false;
}

void Scheduler::DrawAndSwapForced() {
  DrawSwapReadbackResult result = client_->ScheduledActionDrawAndSwapForced();
  if (result.did_swap)
    has_pending_begin_frame_ = false;
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
        client_->ScheduledActionActivatePendingTree();
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
    }
  } while (action != SchedulerStateMachine::ACTION_NONE);

  SetupNextBeginFrameIfNeeded();
  client_->DidAnticipatedDrawTimeChange(AnticipatedDrawTime());
}

bool Scheduler::WillDrawIfNeeded() const {
  return !state_machine_.PendingDrawsShouldBeAborted();
}

}  // namespace cc
