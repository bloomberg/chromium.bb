// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"

namespace cc {

Scheduler::Scheduler(SchedulerClient* client,
                     scoped_ptr<FrameRateController> frame_rate_controller,
                     const SchedulerSettings& scheduler_settings)
    : settings_(scheduler_settings),
      client_(client),
      frame_rate_controller_(frame_rate_controller.Pass()),
      state_machine_(scheduler_settings),
      inside_process_scheduled_actions_(false) {
  DCHECK(client_);
  frame_rate_controller_->SetClient(this);
  DCHECK(!state_machine_.VSyncCallbackNeeded());
}

Scheduler::~Scheduler() { frame_rate_controller_->SetActive(false); }

void Scheduler::SetCanBeginFrame(bool can) {
  state_machine_.SetCanBeginFrame(can);
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

void Scheduler::SetHasPendingTree(bool has_pending_tree) {
  state_machine_.SetHasPendingTree(has_pending_tree);
  ProcessScheduledActions();
}

void Scheduler::SetNeedsCommit() {
  state_machine_.SetNeedsCommit();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsForcedCommit() {
  state_machine_.SetNeedsCommit();
  state_machine_.SetNeedsForcedCommit();
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

void Scheduler::SetNeedsForcedRedraw() {
  state_machine_.SetNeedsForcedRedraw();
  ProcessScheduledActions();
}

void Scheduler::SetMainThreadNeedsLayerTextures() {
  state_machine_.SetMainThreadNeedsLayerTextures();
  ProcessScheduledActions();
}

void Scheduler::BeginFrameComplete() {
  TRACE_EVENT0("cc", "Scheduler::beginFrameComplete");
  state_machine_.BeginFrameComplete();
  ProcessScheduledActions();
}

void Scheduler::BeginFrameAborted() {
  TRACE_EVENT0("cc", "Scheduler::beginFrameAborted");
  state_machine_.BeginFrameAborted();
  ProcessScheduledActions();
}

void Scheduler::SetMaxFramesPending(int max_frames_pending) {
  frame_rate_controller_->SetMaxFramesPending(max_frames_pending);
}

int Scheduler::MaxFramesPending() const {
  return frame_rate_controller_->MaxFramesPending();
}

void Scheduler::SetSwapBuffersCompleteSupported(bool supported) {
  frame_rate_controller_->SetSwapBuffersCompleteSupported(supported);
}

void Scheduler::DidSwapBuffersComplete() {
  TRACE_EVENT0("cc", "Scheduler::didSwapBuffersComplete");
  frame_rate_controller_->DidFinishFrame();
}

void Scheduler::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::didLoseOutputSurface");
  state_machine_.DidLoseOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::DidRecreateOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::didRecreateOutputSurface");
  frame_rate_controller_->DidAbortAllPendingFrames();
  state_machine_.DidRecreateOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::SetTimebaseAndInterval(base::TimeTicks timebase,
                                       base::TimeDelta interval) {
  frame_rate_controller_->SetTimebaseAndInterval(timebase, interval);
}

base::TimeTicks Scheduler::AnticipatedDrawTime() {
  return frame_rate_controller_->NextTickTime();
}

base::TimeTicks Scheduler::LastVSyncTime() {
  return frame_rate_controller_->LastTickTime();
}

void Scheduler::VSyncTick(bool throttled) {
  TRACE_EVENT1("cc", "Scheduler::VSyncTick", "throttled", throttled);
  if (!throttled)
    state_machine_.DidEnterVSync();
  ProcessScheduledActions();
  if (!throttled)
    state_machine_.DidLeaveVSync();
}

void Scheduler::ProcessScheduledActions() {
  // We do not allow ProcessScheduledActions to be recursive.
  // The top-level call will iteratively execute the next action for us anyway.
  if (inside_process_scheduled_actions_)
    return;

  base::AutoReset<bool> mark_inside(&inside_process_scheduled_actions_, true);

  SchedulerStateMachine::Action action = state_machine_.NextAction();
  while (action != SchedulerStateMachine::ACTION_NONE) {
    state_machine_.UpdateState(action);
    TRACE_EVENT1(
        "cc", "Scheduler::ProcessScheduledActions()", "action", action);

    switch (action) {
      case SchedulerStateMachine::ACTION_NONE:
        break;
      case SchedulerStateMachine::ACTION_BEGIN_FRAME:
        client_->ScheduledActionBeginFrame();
        break;
      case SchedulerStateMachine::ACTION_COMMIT:
        client_->ScheduledActionCommit();
        break;
      case SchedulerStateMachine::ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS:
        client_->ScheduledActionCheckForCompletedTileUploads();
        break;
      case SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED:
        client_->ScheduledActionActivatePendingTreeIfNeeded();
        break;
      case SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE: {
        ScheduledActionDrawAndSwapResult result =
            client_->ScheduledActionDrawAndSwapIfPossible();
        state_machine_.DidDrawIfPossibleCompleted(result.did_draw);
        if (result.did_swap)
          frame_rate_controller_->DidBeginFrame();
        break;
      }
      case SchedulerStateMachine::ACTION_DRAW_FORCED: {
        ScheduledActionDrawAndSwapResult result =
            client_->ScheduledActionDrawAndSwapForced();
        if (result.did_swap)
          frame_rate_controller_->DidBeginFrame();
        break;
      }
      case SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION:
        client_->ScheduledActionBeginContextRecreation();
        break;
      case SchedulerStateMachine::ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
        client_->ScheduledActionAcquireLayerTexturesForMainThread();
        break;
    }
    action = state_machine_.NextAction();
  }

  // Activate or deactivate the frame rate controller.
  frame_rate_controller_->SetActive(state_machine_.VSyncCallbackNeeded());
  client_->DidAnticipatedDrawTimeChange(frame_rate_controller_->NextTickTime());
}

}  // namespace cc
