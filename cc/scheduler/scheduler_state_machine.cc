// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_state_machine.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace cc {

SchedulerStateMachine::SchedulerStateMachine(const SchedulerSettings& settings)
    : settings_(settings),
      output_surface_state_(OUTPUT_SURFACE_LOST),
      commit_state_(COMMIT_STATE_IDLE),
      texture_state_(LAYER_TEXTURE_STATE_UNLOCKED),
      forced_redraw_state_(FORCED_REDRAW_STATE_IDLE),
      readback_state_(READBACK_STATE_IDLE),
      commit_count_(0),
      current_frame_number_(0),
      last_frame_number_where_begin_frame_sent_to_main_thread_(-1),
      last_frame_number_swap_performed_(-1),
      last_frame_number_where_update_visible_tiles_was_called_(-1),
      consecutive_failed_draws_(0),
      needs_redraw_(false),
      swap_used_incomplete_tile_(false),
      needs_commit_(false),
      main_thread_needs_layer_textures_(false),
      inside_begin_frame_(false),
      visible_(false),
      can_start_(false),
      can_draw_(false),
      has_pending_tree_(false),
      pending_tree_is_ready_for_activation_(false),
      active_tree_needs_first_draw_(false),
      draw_if_possible_failed_(false),
      did_create_and_initialize_first_output_surface_(false) {}

const char* SchedulerStateMachine::OutputSurfaceStateToString(
    OutputSurfaceState state) {
  switch (state) {
    case OUTPUT_SURFACE_ACTIVE:
      return "OUTPUT_SURFACE_ACTIVE";
    case OUTPUT_SURFACE_LOST:
      return "OUTPUT_SURFACE_LOST";
    case OUTPUT_SURFACE_CREATING:
      return "OUTPUT_SURFACE_CREATING";
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT:
      return "OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT";
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION:
      return "OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::CommitStateToString(CommitState state) {
  switch (state) {
    case COMMIT_STATE_IDLE:
      return "COMMIT_STATE_IDLE";
    case COMMIT_STATE_FRAME_IN_PROGRESS:
      return "COMMIT_STATE_FRAME_IN_PROGRESS";
    case COMMIT_STATE_READY_TO_COMMIT:
      return "COMMIT_STATE_READY_TO_COMMIT";
    case COMMIT_STATE_WAITING_FOR_FIRST_DRAW:
      return "COMMIT_STATE_WAITING_FOR_FIRST_DRAW";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::TextureStateToString(TextureState state) {
  switch (state) {
    case LAYER_TEXTURE_STATE_UNLOCKED:
      return "LAYER_TEXTURE_STATE_UNLOCKED";
    case LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD:
      return "LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD";
    case LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD:
      return "LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::SynchronousReadbackStateToString(
    SynchronousReadbackState state) {
  switch (state) {
    case READBACK_STATE_IDLE:
      return "READBACK_STATE_IDLE";
    case READBACK_STATE_NEEDS_BEGIN_FRAME:
      return "READBACK_STATE_NEEDS_BEGIN_FRAME";
    case READBACK_STATE_WAITING_FOR_COMMIT:
      return "READBACK_STATE_WAITING_FOR_COMMIT";
    case READBACK_STATE_WAITING_FOR_ACTIVATION:
      return "READBACK_STATE_WAITING_FOR_ACTIVATION";
    case READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK:
      return "READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK";
    case READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT:
      return "READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT";
    case READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION:
      return "READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::ForcedRedrawOnTimeoutStateToString(
    ForcedRedrawOnTimeoutState state) {
  switch (state) {
    case FORCED_REDRAW_STATE_IDLE:
      return "FORCED_REDRAW_STATE_IDLE";
    case FORCED_REDRAW_STATE_WAITING_FOR_COMMIT:
      return "FORCED_REDRAW_STATE_WAITING_FOR_COMMIT";
    case FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION:
      return "FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION";
    case FORCED_REDRAW_STATE_WAITING_FOR_DRAW:
      return "FORCED_REDRAW_STATE_WAITING_FOR_DRAW";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::ActionToString(Action action) {
  switch (action) {
    case ACTION_NONE:
      return "ACTION_NONE";
    case ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD:
      return "ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD";
    case ACTION_COMMIT:
      return "ACTION_COMMIT";
    case ACTION_UPDATE_VISIBLE_TILES:
      return "ACTION_UPDATE_VISIBLE_TILES";
    case ACTION_ACTIVATE_PENDING_TREE:
      return "ACTION_ACTIVATE_PENDING_TREE";
    case ACTION_DRAW_AND_SWAP_IF_POSSIBLE:
      return "ACTION_DRAW_AND_SWAP_IF_POSSIBLE";
    case ACTION_DRAW_AND_SWAP_FORCED:
      return "ACTION_DRAW_AND_SWAP_FORCED";
    case ACTION_DRAW_AND_SWAP_ABORT:
      return "ACTION_DRAW_AND_SWAP_ABORT";
    case ACTION_DRAW_AND_READBACK:
      return "ACTION_DRAW_AND_READBACK";
    case ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
      return "ACTION_BEGIN_OUTPUT_SURFACE_CREATION";
    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
      return "ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD";
  }
  NOTREACHED();
  return "???";
}

scoped_ptr<base::Value> SchedulerStateMachine::AsValue() const  {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  scoped_ptr<base::DictionaryValue> major_state(new base::DictionaryValue);
  major_state->SetString("next_action", ActionToString(NextAction()));
  major_state->SetString("commit_state", CommitStateToString(commit_state_));
  major_state->SetString("texture_state_",
                         TextureStateToString(texture_state_));
  major_state->SetString("output_surface_state_",
                         OutputSurfaceStateToString(output_surface_state_));
  major_state->SetString(
      "forced_redraw_state",
      ForcedRedrawOnTimeoutStateToString(forced_redraw_state_));
  major_state->SetString("readback_state",
                         SynchronousReadbackStateToString(readback_state_));
  state->Set("major_state", major_state.release());

  scoped_ptr<base::DictionaryValue> timestamps_state(new base::DictionaryValue);
  base::TimeTicks now = base::TimeTicks::Now();
  timestamps_state->SetDouble(
      "0_interval", last_begin_frame_args_.interval.InMicroseconds() / 1000.0L);
  timestamps_state->SetDouble(
      "1_now_to_deadline",
      (last_begin_frame_args_.deadline - now).InMicroseconds() / 1000.0L);
  timestamps_state->SetDouble(
      "2_frame_time_to_now",
      (now - last_begin_frame_args_.frame_time).InMicroseconds() / 1000.0L);
  timestamps_state->SetDouble(
      "3_frame_time_to_deadline",
      (last_begin_frame_args_.deadline - last_begin_frame_args_.frame_time)
              .InMicroseconds() /
          1000.0L);
  timestamps_state->SetDouble(
      "4_now", (now - base::TimeTicks()).InMicroseconds() / 1000.0L);
  timestamps_state->SetDouble(
      "5_frame_time",
      (last_begin_frame_args_.frame_time - base::TimeTicks()).InMicroseconds() /
          1000.0L);
  timestamps_state->SetDouble(
      "6_deadline",
      (last_begin_frame_args_.deadline - base::TimeTicks()).InMicroseconds() /
          1000.0L);
  state->Set("major_timestamps_in_ms", timestamps_state.release());

  scoped_ptr<base::DictionaryValue> minor_state(new base::DictionaryValue);
  minor_state->SetInteger("commit_count", commit_count_);
  minor_state->SetInteger("current_frame_number", current_frame_number_);
  minor_state->SetInteger(
      "last_frame_number_where_begin_frame_sent_to_main_thread",
      last_frame_number_where_begin_frame_sent_to_main_thread_);
  minor_state->SetInteger("last_frame_number_swap_performed_",
                          last_frame_number_swap_performed_);
  minor_state->SetInteger(
      "last_frame_number_where_update_visible_tiles_was_called",
      last_frame_number_where_update_visible_tiles_was_called_);
  minor_state->SetInteger("consecutive_failed_draws",
                          consecutive_failed_draws_);
  minor_state->SetBoolean("needs_redraw", needs_redraw_);
  minor_state->SetBoolean("swap_used_incomplete_tile",
                          swap_used_incomplete_tile_);
  minor_state->SetBoolean("needs_commit", needs_commit_);
  minor_state->SetBoolean("main_thread_needs_layer_textures",
                          main_thread_needs_layer_textures_);
  minor_state->SetBoolean("inside_begin_frame", inside_begin_frame_);
  minor_state->SetBoolean("visible", visible_);
  minor_state->SetBoolean("can_start", can_start_);
  minor_state->SetBoolean("can_draw", can_draw_);
  minor_state->SetBoolean("has_pending_tree", has_pending_tree_);
  minor_state->SetBoolean("pending_tree_is_ready_for_activation_",
                          pending_tree_is_ready_for_activation_);
  minor_state->SetBoolean("active_tree_needs_first_draw_",
                          active_tree_needs_first_draw_);
  minor_state->SetBoolean("draw_if_possible_failed", draw_if_possible_failed_);
  minor_state->SetBoolean("did_create_and_initialize_first_output_surface",
                          did_create_and_initialize_first_output_surface_);
  state->Set("minor_state", minor_state.release());

  return state.PassAs<base::Value>();
}

bool SchedulerStateMachine::HasDrawnAndSwappedThisFrame() const {
  return current_frame_number_ == last_frame_number_swap_performed_;
}

bool SchedulerStateMachine::HasUpdatedVisibleTilesThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_where_update_visible_tiles_was_called_;
}

bool SchedulerStateMachine::HasSentBeginFrameToMainThreadThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_where_begin_frame_sent_to_main_thread_;
}

bool SchedulerStateMachine::PendingDrawsShouldBeAborted() const {
  // These are all the cases where we normally cannot or do not want to draw
  // but, if needs_redraw_ is true and we do not draw to make forward progress,
  // we might deadlock with the main thread.
  // This should be a superset of PendingActivationsShouldBeForced() since
  // activation of the pending tree is blocked by drawing of the active tree and
  // the main thread might be blocked on activation of the most recent commit.
  if (PendingActivationsShouldBeForced())
    return true;

  // Additional states where we should abort draws.
  // Note: We don't force activation in these cases because doing so would
  // result in checkerboarding on resize, becoming visible, etc.
  if (!can_draw_)
    return true;
  if (!visible_)
    return true;
  return false;
}

bool SchedulerStateMachine::PendingActivationsShouldBeForced() const {
  // These are all the cases where, if we do not force activations to make
  // forward progress, we might deadlock with the main thread.

  // The impl thread cannot lock layer textures unless the pending
  // tree can be activated to unblock the commit.
  if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD)
    return true;

  // There is no output surface to trigger our activations.
  if (output_surface_state_ == OUTPUT_SURFACE_LOST)
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldBeginOutputSurfaceCreation() const {
  // Don't try to initialize too early.
  if (!can_start_)
    return false;

  // We only want to start output surface initialization after the
  // previous commit is complete.
  if (commit_state_ != COMMIT_STATE_IDLE)
    return false;

  // We want to clear the pipline of any pending draws and activations
  // before starting output surface initialization. This allows us to avoid
  // weird corner cases where we abort draws or force activation while we
  // are initializing the output surface and can potentially have a pending
  // readback.
  if (active_tree_needs_first_draw_ || has_pending_tree_)
    return false;

  // We need to create the output surface if we don't have one and we haven't
  // started creating one yet.
  return output_surface_state_ == OUTPUT_SURFACE_LOST;
}

bool SchedulerStateMachine::ShouldDraw() const {
  // After a readback, make sure not to draw again until we've replaced the
  // readback commit with a real one.
  if (readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT ||
      readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION)
    return false;

  // Draw immediately for readbacks to unblock the main thread quickly.
  if (readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK) {
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    return true;
  }

  // If we need to abort draws, we should do so ASAP since the draw could
  // be blocking other important actions (like output surface initialization),
  // from occuring. If we are waiting for the first draw, then perfom the
  // aborted draw to keep things moving. If we are not waiting for the first
  // draw however, we don't want to abort for no reason.
  if (PendingDrawsShouldBeAborted())
    return active_tree_needs_first_draw_;

  // After this line, we only want to draw once per frame.
  if (HasDrawnAndSwappedThisFrame())
    return false;

  // We currently only draw within the BeginFrame.
  if (!inside_begin_frame_)
    return false;

  // Only handle forced redraws due to timeouts on the regular deadline.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW) {
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    return true;
  }

  return needs_redraw_;
}

bool SchedulerStateMachine::ShouldAcquireLayerTexturesForMainThread() const {
  if (!main_thread_needs_layer_textures_)
    return false;
  if (texture_state_ == LAYER_TEXTURE_STATE_UNLOCKED)
    return true;
  DCHECK_EQ(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD);
  return false;
}

bool SchedulerStateMachine::ShouldActivatePendingTree() const {
  // There is nothing to activate.
  if (!has_pending_tree_)
    return false;

  // We should not activate a second tree before drawing the first one.
  // Even if we need to force activation of the pending tree, we should abort
  // drawing the active tree first.
  if (active_tree_needs_first_draw_)
    return false;

  // If we want to force activation, do so ASAP.
  if (PendingActivationsShouldBeForced())
    return true;

  // At this point, only activate if we are ready to activate.
  return pending_tree_is_ready_for_activation_;
}

bool SchedulerStateMachine::ShouldUpdateVisibleTiles() const {
  if (!settings_.impl_side_painting)
    return false;
  if (HasUpdatedVisibleTilesThisFrame())
    return false;

  // There's no reason to check for tiles if we don't have an output surface.
  if (!HasInitializedOutputSurface())
    return false;

  // We always want to update the most recent visible tiles before drawing
  // so we draw with fewer missing tiles.
  if (ShouldDraw())
    return true;

  // If the last swap drew with checkerboard or missing tiles, we should
  // poll for any new visible tiles so we can be notified to draw again
  // when there are.
  if (swap_used_incomplete_tile_)
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldSendBeginFrameToMainThread() const {
  if (!needs_commit_)
    return false;

  // Only send BeginFrame to the main thread when idle.
  if (commit_state_ != COMMIT_STATE_IDLE)
    return false;

  // We can't accept a commit if we have a pending tree.
  if (has_pending_tree_)
    return false;

  // We want to handle readback commits immediately to unblock the main thread.
  // Note: This BeginFrame will correspond to the replacement commit that comes
  // after the readback commit itself, so we only send the BeginFrame if a
  // commit isn't already pending behind the readback.
  if (readback_state_ == READBACK_STATE_NEEDS_BEGIN_FRAME)
    return !CommitPending();

  // We do not need commits if we are not visible, unless there's a
  // request for a forced commit.
  if (!visible_)
    return false;

  // We want to start the first commit after we get a new output surface ASAP.
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT)
    return true;

  // We need a new commit for the forced redraw. This honors the
  // single commit per interval because the result will be swapped to screen.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_COMMIT)
    return true;

  // After this point, we only start a commit once per frame.
  if (HasSentBeginFrameToMainThreadThisFrame())
    return false;

  // We shouldn't normally accept commits if there isn't an OutputSurface.
  if (!HasInitializedOutputSurface())
    return false;

  return true;
}

bool SchedulerStateMachine::ShouldCommit() const {
  return commit_state_ == COMMIT_STATE_READY_TO_COMMIT;
}

SchedulerStateMachine::Action SchedulerStateMachine::NextAction() const {
  if (ShouldAcquireLayerTexturesForMainThread())
    return ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD;
  if (ShouldUpdateVisibleTiles())
    return ACTION_UPDATE_VISIBLE_TILES;
  if (ShouldActivatePendingTree())
    return ACTION_ACTIVATE_PENDING_TREE;
  if (ShouldCommit())
    return ACTION_COMMIT;
  if (ShouldDraw()) {
    if (readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK)
      return ACTION_DRAW_AND_READBACK;
    else if (PendingDrawsShouldBeAborted())
      return ACTION_DRAW_AND_SWAP_ABORT;
    else if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
      return ACTION_DRAW_AND_SWAP_FORCED;
    else
      return ACTION_DRAW_AND_SWAP_IF_POSSIBLE;
  }
  if (ShouldSendBeginFrameToMainThread())
    return ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD;
  if (ShouldBeginOutputSurfaceCreation())
    return ACTION_BEGIN_OUTPUT_SURFACE_CREATION;
  return ACTION_NONE;
}

void SchedulerStateMachine::CheckInvariants() {
  // We should never try to perform a draw for readback and forced draw due to
  // timeout simultaneously.
  DCHECK(!(forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW &&
           readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK));
}

void SchedulerStateMachine::UpdateState(Action action) {
  switch (action) {
    case ACTION_NONE:
      return;

    case ACTION_UPDATE_VISIBLE_TILES:
      last_frame_number_where_update_visible_tiles_was_called_ =
          current_frame_number_;
      return;

    case ACTION_ACTIVATE_PENDING_TREE:
      UpdateStateOnActivation();
      return;

    case ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD:
      DCHECK(!has_pending_tree_);
      DCHECK(visible_ || readback_state_ == READBACK_STATE_NEEDS_BEGIN_FRAME);
      commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
      needs_commit_ = false;
      if (readback_state_ == READBACK_STATE_NEEDS_BEGIN_FRAME)
        readback_state_ = READBACK_STATE_WAITING_FOR_COMMIT;
      last_frame_number_where_begin_frame_sent_to_main_thread_ =
          current_frame_number_;
      return;

    case ACTION_COMMIT: {
      bool commit_was_aborted = false;
      UpdateStateOnCommit(commit_was_aborted);
      return;
    }

    case ACTION_DRAW_AND_SWAP_FORCED:
    case ACTION_DRAW_AND_SWAP_IF_POSSIBLE: {
      bool did_swap = true;
      UpdateStateOnDraw(did_swap);
      return;
    }

    case ACTION_DRAW_AND_SWAP_ABORT:
    case ACTION_DRAW_AND_READBACK: {
      bool did_swap = false;
      UpdateStateOnDraw(did_swap);
      return;
    }

    case ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
      DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_LOST);
      output_surface_state_ = OUTPUT_SURFACE_CREATING;

      // The following DCHECKs make sure we are in the proper quiescent state.
      // The pipeline should be flushed entirely before we start output
      // surface creation to avoid complicated corner cases.
      DCHECK_EQ(commit_state_, COMMIT_STATE_IDLE);
      DCHECK(!has_pending_tree_);
      DCHECK(!active_tree_needs_first_draw_);
      return;

    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
      texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD;
      main_thread_needs_layer_textures_ = false;
      return;
  }
}

void SchedulerStateMachine::UpdateStateOnCommit(bool commit_was_aborted) {
  commit_count_++;

  // If we are impl-side-painting but the commit was aborted, then we behave
  // mostly as if we are not impl-side-painting since there is no pending tree.
  has_pending_tree_ = settings_.impl_side_painting && !commit_was_aborted;

  // Update state related to readbacks.
  if (readback_state_ == READBACK_STATE_WAITING_FOR_COMMIT) {
    // Update the state if this is the readback commit.
    readback_state_ = has_pending_tree_
                          ? READBACK_STATE_WAITING_FOR_ACTIVATION
                          : READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK;
  } else if (readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT) {
    // Update the state if this is the commit replacing the readback commit.
    readback_state_ = has_pending_tree_
                          ? READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION
                          : READBACK_STATE_IDLE;
  } else {
    DCHECK(readback_state_ == READBACK_STATE_IDLE);
  }

  // Readbacks can interrupt output surface initialization and forced draws,
  // so we do not want to advance those states if we are in the middle of a
  // readback. Note: It is possible for the readback's replacement commit to
  // be the output surface's first commit and/or the forced redraw's commit.
  if (readback_state_ == READBACK_STATE_IDLE ||
      readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION) {
    // Update state related to forced draws.
    if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_COMMIT) {
      forced_redraw_state_ = has_pending_tree_
                                 ? FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION
                                 : FORCED_REDRAW_STATE_WAITING_FOR_DRAW;
    }

    // Update the output surface state.
    DCHECK_NE(output_surface_state_,
              OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION);
    if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT) {
      if (has_pending_tree_) {
        output_surface_state_ = OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION;
      } else {
        output_surface_state_ = OUTPUT_SURFACE_ACTIVE;
        needs_redraw_ = true;
      }
    }
  }

  // Update the commit state. We expect and wait for a draw if the commit
  // was not aborted or if we are in a readback or forced draw.
  if (!commit_was_aborted)
    commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
  else if (readback_state_ != READBACK_STATE_IDLE ||
           forced_redraw_state_ != FORCED_REDRAW_STATE_IDLE)
    commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
  else
    commit_state_ = COMMIT_STATE_IDLE;

  // Update state if we have a new active tree to draw, or if the active tree
  // was unchanged but we need to do a readback or forced draw.
  if (!has_pending_tree_ &&
      (!commit_was_aborted ||
       readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK ||
       forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)) {
    needs_redraw_ = true;
    active_tree_needs_first_draw_ = true;
  }

  // This post-commit work is common to both completed and aborted commits.
  pending_tree_is_ready_for_activation_ = false;

  if (draw_if_possible_failed_)
    last_frame_number_swap_performed_ = -1;

  // If we are planing to draw with the new commit, lock the layer textures for
  // use on the impl thread. Otherwise, leave them unlocked.
  if (has_pending_tree_ || needs_redraw_)
    texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD;
  else
    texture_state_ = LAYER_TEXTURE_STATE_UNLOCKED;
}

void SchedulerStateMachine::UpdateStateOnActivation() {
  // Update output surface state.
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION)
    output_surface_state_ = OUTPUT_SURFACE_ACTIVE;

  // Update readback state
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION)
    forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_DRAW;

  // Update forced redraw state
  if (readback_state_ == READBACK_STATE_WAITING_FOR_ACTIVATION)
    readback_state_ = READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK;
  else if (readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION)
    readback_state_ = READBACK_STATE_IDLE;

  has_pending_tree_ = false;
  pending_tree_is_ready_for_activation_ = false;
  active_tree_needs_first_draw_ = true;
  needs_redraw_ = true;
}

void SchedulerStateMachine::UpdateStateOnDraw(bool did_swap) {
  DCHECK(readback_state_ != READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT &&
         readback_state_ != READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION)
      << *AsValue();

  if (readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK) {
    // The draw correspons to a readback commit.
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    // We are blocking commits from the main thread until after this draw, so
    // we should not have a pending tree.
    DCHECK(!has_pending_tree_);
    // We transition to COMMIT_STATE_FRAME_IN_PROGRESS because there is a
    // pending BeginFrame on the main thread behind the readback request.
    commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
    readback_state_ = READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT;
  } else if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW) {
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    commit_state_ = COMMIT_STATE_IDLE;
    forced_redraw_state_ = FORCED_REDRAW_STATE_IDLE;
  } else if (commit_state_ == COMMIT_STATE_WAITING_FOR_FIRST_DRAW &&
             !has_pending_tree_) {
    commit_state_ = COMMIT_STATE_IDLE;
  }

  if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD)
    texture_state_ = LAYER_TEXTURE_STATE_UNLOCKED;

  needs_redraw_ = false;
  draw_if_possible_failed_ = false;
  active_tree_needs_first_draw_ = false;

  if (did_swap)
    last_frame_number_swap_performed_ = current_frame_number_;
}

void SchedulerStateMachine::SetMainThreadNeedsLayerTextures() {
  DCHECK(!main_thread_needs_layer_textures_);
  DCHECK_NE(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD);
  main_thread_needs_layer_textures_ = true;
}

// These are the cases where we definitely (or almost definitely) have a
// new frame to draw and can draw.
bool SchedulerStateMachine::BeginFrameNeededToDrawByImplThread() const {
  // The output surface is the provider of BeginFrames for the impl thread,
  // so we are not going to get them even if we ask for them.
  if (!HasInitializedOutputSurface())
    return false;

  // If we can't draw, don't tick until we are notified that we can draw again.
  if (!can_draw_)
    return false;

  // The forced draw respects our normal draw scheduling, so we need to
  // request a BeginFrame on the impl thread for it.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
    return true;

  // There's no need to produce frames if we are not visible.
  if (!visible_)
    return false;

  // We need to draw a more complete frame than we did the last BeginFrame,
  // so request another BeginFrame in anticipation that we will have
  // additional visible tiles.
  if (swap_used_incomplete_tile_)
    return true;

  return needs_redraw_;
}

// These are cases where we are very likely to draw soon, but might not
// actually have a new frame to draw when we receive the next BeginFrame.
// Proactively requesting the BeginFrame helps hide the round trip latency of
// the SetNeedsBeginFrame request that has to go to the Browser.
// However, this is bad for the synchronous compositor because we have to
// draw when we get the BeginFrame and could end up drawing many duplicate
// frames.
bool SchedulerStateMachine::ProactiveBeginFrameWantedByImplThread() const {
  // The output surface is the provider of BeginFrames for the impl thread,
  // so we are not going to get them even if we ask for them.
  if (!HasInitializedOutputSurface())
    return false;

  // Do not be proactive when invisible.
  if (!visible_)
    return false;

  // We should proactively request a BeginFrame if a commit is pending
  // because we will want to draw if the commit completes quickly.
  if (needs_commit_ || commit_state_ != COMMIT_STATE_IDLE)
    return true;

  // If the pending tree activates quickly, we'll want a BeginFrame soon
  // to draw the new active tree.
  if (has_pending_tree_)
    return true;

  return false;
}

void SchedulerStateMachine::DidEnterBeginFrame(const BeginFrameArgs& args) {
  current_frame_number_++;
  inside_begin_frame_ = true;
  last_begin_frame_args_ = args;
}

void SchedulerStateMachine::DidLeaveBeginFrame() {
  inside_begin_frame_ = false;
}

void SchedulerStateMachine::SetVisible(bool visible) { visible_ = visible; }

void SchedulerStateMachine::SetNeedsRedraw() { needs_redraw_ = true; }

void SchedulerStateMachine::SetSwapUsedIncompleteTile(
    bool used_incomplete_tile) {
  swap_used_incomplete_tile_ = used_incomplete_tile;
}

void SchedulerStateMachine::DidDrawIfPossibleCompleted(bool success) {
  draw_if_possible_failed_ = !success;
  if (draw_if_possible_failed_) {
    needs_redraw_ = true;
    needs_commit_ = true;
    consecutive_failed_draws_++;
    if (settings_.timeout_and_draw_when_animation_checkerboards &&
        consecutive_failed_draws_ >=
            settings_.maximum_number_of_failed_draws_before_draw_is_forced_) {
      consecutive_failed_draws_ = 0;
      // We need to force a draw, but it doesn't make sense to do this until
      // we've committed and have new textures.
      forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_COMMIT;
    }
  } else {
    consecutive_failed_draws_ = 0;
  }
}

void SchedulerStateMachine::SetNeedsCommit() { needs_commit_ = true; }

void SchedulerStateMachine::SetNeedsForcedCommitForReadback() {
  // If this is called in READBACK_STATE_IDLE, this is a "first" readback
  // request.
  // If this is called in READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT, this
  // is a back-to-back readback request that started before the replacement
  // commit had a chance to land.
  DCHECK(readback_state_ == READBACK_STATE_IDLE ||
         readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT);

  // If there is already a commit in progress when we get the readback request
  // (we are in COMMIT_STATE_FRAME_IN_PROGRESS), then we don't need to send a
  // BeginFrame for the replacement commit, since there's already a BeginFrame
  // behind the readback request. In that case, we can skip
  // READBACK_STATE_NEEDS_BEGIN_FRAME and go directly to
  // READBACK_STATE_WAITING_FOR_COMMIT
  if (commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS)
    readback_state_ = READBACK_STATE_WAITING_FOR_COMMIT;
  else
    readback_state_ = READBACK_STATE_NEEDS_BEGIN_FRAME;
}

void SchedulerStateMachine::FinishCommit() {
  DCHECK(commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS) << *AsValue();
  commit_state_ = COMMIT_STATE_READY_TO_COMMIT;
}

void SchedulerStateMachine::BeginFrameAbortedByMainThread(bool did_handle) {
  DCHECK_EQ(commit_state_, COMMIT_STATE_FRAME_IN_PROGRESS);
  if (did_handle) {
    bool commit_was_aborted = true;
    UpdateStateOnCommit(commit_was_aborted);
  } else {
    DCHECK_NE(readback_state_, READBACK_STATE_WAITING_FOR_COMMIT);
    commit_state_ = COMMIT_STATE_IDLE;
    SetNeedsCommit();
  }
}

void SchedulerStateMachine::DidLoseOutputSurface() {
  if (output_surface_state_ == OUTPUT_SURFACE_LOST ||
      output_surface_state_ == OUTPUT_SURFACE_CREATING)
    return;
  output_surface_state_ = OUTPUT_SURFACE_LOST;
  needs_redraw_ = false;
}

void SchedulerStateMachine::NotifyReadyToActivate() {
  if (has_pending_tree_)
    pending_tree_is_ready_for_activation_ = true;
}

void SchedulerStateMachine::SetCanDraw(bool can) { can_draw_ = can; }

void SchedulerStateMachine::DidCreateAndInitializeOutputSurface() {
  DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_CREATING);
  output_surface_state_ = OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT;

  if (did_create_and_initialize_first_output_surface_) {
    // TODO(boliu): See if we can remove this when impl-side painting is always
    // on. Does anything on the main thread need to update after recreate?
    needs_commit_ = true;
  }
  did_create_and_initialize_first_output_surface_ = true;
}

bool SchedulerStateMachine::HasInitializedOutputSurface() const {
  switch (output_surface_state_) {
    case OUTPUT_SURFACE_LOST:
    case OUTPUT_SURFACE_CREATING:
      return false;

    case OUTPUT_SURFACE_ACTIVE:
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT:
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION:
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace cc
