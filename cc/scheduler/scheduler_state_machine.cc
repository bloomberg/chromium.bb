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
      commit_count_(0),
      current_frame_number_(0),
      last_frame_number_where_begin_frame_sent_to_main_thread_(-1),
      last_frame_number_where_draw_was_called_(-1),
      last_frame_number_where_tree_activation_attempted_(-1),
      last_frame_number_where_update_visible_tiles_was_called_(-1),
      consecutive_failed_draws_(0),
      maximum_number_of_failed_draws_before_draw_is_forced_(3),
      needs_redraw_(false),
      swap_used_incomplete_tile_(false),
      needs_forced_redraw_(false),
      needs_forced_redraw_after_next_commit_(false),
      needs_commit_(false),
      needs_forced_commit_(false),
      expect_immediate_begin_frame_for_main_thread_(false),
      main_thread_needs_layer_textures_(false),
      inside_begin_frame_(false),
      visible_(false),
      can_start_(false),
      can_draw_(false),
      has_pending_tree_(false),
      draw_if_possible_failed_(false),
      texture_state_(LAYER_TEXTURE_STATE_UNLOCKED),
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
    case COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW:
      return "COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW";
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
    case ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED:
      return "ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED";
    case ACTION_DRAW_IF_POSSIBLE:
      return "ACTION_DRAW_IF_POSSIBLE";
    case ACTION_DRAW_FORCED:
      return "ACTION_DRAW_FORCED";
    case ACTION_DRAW_AND_SWAP_ABORT:
      return "ACTION_DRAW_AND_SWAP_ABORT";
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
      (last_begin_frame_args_.deadline - now).InMicroseconds() / 1000.0L);
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
  minor_state->SetInteger("last_frame_number_where_draw_was_called",
                          last_frame_number_where_draw_was_called_);
  minor_state->SetInteger("last_frame_number_where_tree_activation_attempted",
                          last_frame_number_where_tree_activation_attempted_);
  minor_state->SetInteger(
      "last_frame_number_where_update_visible_tiles_was_called",
      last_frame_number_where_update_visible_tiles_was_called_);
  minor_state->SetInteger("consecutive_failed_draws",
                          consecutive_failed_draws_);
  minor_state->SetInteger(
      "maximum_number_of_failed_draws_before_draw_is_forced",
      maximum_number_of_failed_draws_before_draw_is_forced_);
  minor_state->SetBoolean("needs_redraw", needs_redraw_);
  minor_state->SetBoolean("swap_used_incomplete_tile",
                          swap_used_incomplete_tile_);
  minor_state->SetBoolean("needs_forced_redraw", needs_forced_redraw_);
  minor_state->SetBoolean("needs_forced_redraw_after_next_commit",
                          needs_forced_redraw_after_next_commit_);
  minor_state->SetBoolean("needs_commit", needs_commit_);
  minor_state->SetBoolean("needs_forced_commit", needs_forced_commit_);
  minor_state->SetBoolean("expect_immediate_begin_frame_for_main_thread",
                          expect_immediate_begin_frame_for_main_thread_);
  minor_state->SetBoolean("main_thread_needs_layer_textures",
                          main_thread_needs_layer_textures_);
  minor_state->SetBoolean("inside_begin_frame", inside_begin_frame_);
  minor_state->SetBoolean("visible", visible_);
  minor_state->SetBoolean("can_start", can_start_);
  minor_state->SetBoolean("can_draw", can_draw_);
  minor_state->SetBoolean("has_pending_tree", has_pending_tree_);
  minor_state->SetBoolean("draw_if_possible_failed", draw_if_possible_failed_);
  minor_state->SetBoolean("did_create_and_initialize_first_output_surface",
                          did_create_and_initialize_first_output_surface_);
  state->Set("minor_state", minor_state.release());

  return state.PassAs<base::Value>();
}

bool SchedulerStateMachine::HasDrawnThisFrame() const {
  return current_frame_number_ == last_frame_number_where_draw_was_called_;
}

bool SchedulerStateMachine::HasAttemptedTreeActivationThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_where_tree_activation_attempted_;
}

bool SchedulerStateMachine::HasUpdatedVisibleTilesThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_where_update_visible_tiles_was_called_;
}

bool SchedulerStateMachine::HasSentBeginFrameToMainThreadThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_where_begin_frame_sent_to_main_thread_;
}

void SchedulerStateMachine::HandleCommitInternal(bool commit_was_aborted) {
  commit_count_++;

  // If we are impl-side-painting but the commit was aborted, then we behave
  // mostly as if we are not impl-side-painting since there is no pending tree.
  bool commit_results_in_pending_tree =
      settings_.impl_side_painting && !commit_was_aborted;

  // Update the commit state.
  if (expect_immediate_begin_frame_for_main_thread_)
    commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW;
  else if (!commit_was_aborted)
    commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
  else
    commit_state_ = COMMIT_STATE_IDLE;

  // Update the output surface state.
  DCHECK_NE(output_surface_state_,
            OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION);
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT) {
    if (commit_results_in_pending_tree) {
      output_surface_state_ = OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION;
    } else {
      output_surface_state_ = OUTPUT_SURFACE_ACTIVE;
      needs_redraw_ = true;
    }
  }

  // if we don't have to wait for activation, update needs_redraw now.
  if (!commit_results_in_pending_tree) {
    if (!commit_was_aborted)
      needs_redraw_ = true;
    if (expect_immediate_begin_frame_for_main_thread_)
      needs_redraw_ = true;
  }

  // This post-commit work is common to both completed and aborted commits.
  if (draw_if_possible_failed_)
    last_frame_number_where_draw_was_called_ = -1;

  if (needs_forced_redraw_after_next_commit_) {
    needs_forced_redraw_after_next_commit_ = false;
    needs_forced_redraw_ = true;
  }

  // If we are planing to draw with the new commit, lock the layer textures for
  // use on the impl thread. Otherwise, leave them unlocked.
  if (commit_results_in_pending_tree || needs_redraw_ || needs_forced_redraw_)
    texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD;
  else
    texture_state_ = LAYER_TEXTURE_STATE_UNLOCKED;
}

bool SchedulerStateMachine::PendingDrawsShouldBeAborted() const {
  // These are all the cases where, if we do not abort draws to make
  // forward progress, we might deadlock with the main thread.
  if (!can_draw_)
    return true;
  if (!visible_)
    return true;
  if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE)
    return true;
  if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD)
    return true;
  return false;
}

bool SchedulerStateMachine::ShouldDraw() const {
  // Always handle forced draws ASAP.
  if (needs_forced_redraw_)
    return true;

  // If we are going to abort draws, we should do so ASAP.
  if (PendingDrawsShouldBeAborted()) {
    // TODO(brianderson): Remove the !has_pending_tree_ condition once
    // the Scheduler controls activation. It's dangerous for us to rely on
    // an eventual activation if we've lost the output surface.
    return commit_state_ == COMMIT_STATE_WAITING_FOR_FIRST_DRAW &&
           !has_pending_tree_;
  }

  // After this line, we only want to draw once per frame.
  if (HasDrawnThisFrame())
    return false;

  // We currently only draw within the BeginFrame.
  if (!inside_begin_frame_)
    return false;

  return needs_redraw_;
}

bool SchedulerStateMachine::ShouldAttemptTreeActivation() const {
  return has_pending_tree_ && inside_begin_frame_ &&
         !HasAttemptedTreeActivationThisFrame();
}

bool SchedulerStateMachine::ShouldUpdateVisibleTiles() const {
  if (!settings_.impl_side_painting)
    return false;
  if (HasUpdatedVisibleTilesThisFrame())
    return false;

  return ShouldAttemptTreeActivation() || ShouldDraw() ||
         swap_used_incomplete_tile_;
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

  // We want to start the first commit after we get a new output surface ASAP.
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT)
    return true;

  // We want to start forced commits ASAP.
  if (needs_forced_commit_)
    return true;

  // After this point, we only start a commit once per frame.
  if (HasSentBeginFrameToMainThreadThisFrame())
    return false;

  // We do not need commits if we are not visible, unless there's a
  // request for a forced commit.
  if (!visible_)
    return false;

  // We shouldn't normally accept commits if there isn't an OutputSurface.
  if (!HasInitializedOutputSurface())
    return false;

  return true;
}

bool SchedulerStateMachine::ShouldAcquireLayerTexturesForMainThread() const {
  if (!main_thread_needs_layer_textures_)
    return false;
  if (texture_state_ == LAYER_TEXTURE_STATE_UNLOCKED)
    return true;
  DCHECK_EQ(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD);
  return false;
}

SchedulerStateMachine::Action SchedulerStateMachine::NextAction() const {
  if (ShouldAcquireLayerTexturesForMainThread())
    return ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD;

  switch (commit_state_) {
    case COMMIT_STATE_IDLE: {
      if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE &&
          needs_forced_redraw_)
        return ACTION_DRAW_FORCED;
      if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE &&
          needs_forced_commit_)
        // TODO(enne): Should probably drop the active tree on force commit.
        return has_pending_tree_ ? ACTION_NONE
                                 : ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD;
      if (output_surface_state_ == OUTPUT_SURFACE_LOST && can_start_)
        return ACTION_BEGIN_OUTPUT_SURFACE_CREATION;
      if (output_surface_state_ == OUTPUT_SURFACE_CREATING)
        return ACTION_NONE;
      if (ShouldUpdateVisibleTiles())
        return ACTION_UPDATE_VISIBLE_TILES;
      if (ShouldAttemptTreeActivation())
        return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
      if (ShouldDraw()) {
        return needs_forced_redraw_ ? ACTION_DRAW_FORCED
                                    : ACTION_DRAW_IF_POSSIBLE;
      }
      if (ShouldSendBeginFrameToMainThread())
        return ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD;
      return ACTION_NONE;
    }
    case COMMIT_STATE_FRAME_IN_PROGRESS:
      if (ShouldUpdateVisibleTiles())
        return ACTION_UPDATE_VISIBLE_TILES;
      if (ShouldAttemptTreeActivation())
        return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
      if (ShouldDraw()) {
        return needs_forced_redraw_ ? ACTION_DRAW_FORCED
                                    : ACTION_DRAW_IF_POSSIBLE;
      }
      return ACTION_NONE;

    case COMMIT_STATE_READY_TO_COMMIT:
      return ACTION_COMMIT;

    case COMMIT_STATE_WAITING_FOR_FIRST_DRAW: {
      if (ShouldUpdateVisibleTiles())
        return ACTION_UPDATE_VISIBLE_TILES;
      if (ShouldAttemptTreeActivation())
        return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
      if (ShouldDraw()) {
        if (needs_forced_redraw_)
          return ACTION_DRAW_FORCED;
        else if (PendingDrawsShouldBeAborted())
          return ACTION_DRAW_AND_SWAP_ABORT;
        else
          return ACTION_DRAW_IF_POSSIBLE;
      }
      return ACTION_NONE;
    }

    case COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW:
      if (ShouldUpdateVisibleTiles())
        return ACTION_UPDATE_VISIBLE_TILES;
      if (ShouldAttemptTreeActivation())
        return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
      if (needs_forced_redraw_)
        return ACTION_DRAW_FORCED;
      return ACTION_NONE;
  }
  NOTREACHED();
  return ACTION_NONE;
}

void SchedulerStateMachine::UpdateState(Action action) {
  switch (action) {
    case ACTION_NONE:
      return;

    case ACTION_UPDATE_VISIBLE_TILES:
      last_frame_number_where_update_visible_tiles_was_called_ =
          current_frame_number_;
      return;

    case ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED:
      last_frame_number_where_tree_activation_attempted_ =
          current_frame_number_;
      return;

    case ACTION_SEND_BEGIN_FRAME_TO_MAIN_THREAD:
      DCHECK(!has_pending_tree_);
      if (!needs_forced_commit_ &&
          output_surface_state_ != OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT) {
        DCHECK(visible_);
        DCHECK_GT(current_frame_number_,
                  last_frame_number_where_begin_frame_sent_to_main_thread_);
      }
      commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
      needs_commit_ = false;
      needs_forced_commit_ = false;
      last_frame_number_where_begin_frame_sent_to_main_thread_ =
          current_frame_number_;
      return;

    case ACTION_COMMIT: {
      bool commit_was_aborted = false;
      HandleCommitInternal(commit_was_aborted);
      return;
    }

    case ACTION_DRAW_FORCED:
    case ACTION_DRAW_IF_POSSIBLE: {
      bool did_swap = true;
      UpdateStateOnDraw(did_swap);
      return;
    }

    case ACTION_DRAW_AND_SWAP_ABORT: {
      bool did_swap = false;
      UpdateStateOnDraw(did_swap);
      return;
    }

    case ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
      DCHECK_EQ(commit_state_, COMMIT_STATE_IDLE);
      DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_LOST);
      output_surface_state_ = OUTPUT_SURFACE_CREATING;
      return;

    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
      texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD;
      main_thread_needs_layer_textures_ = false;
      return;
  }
}

void SchedulerStateMachine::UpdateStateOnDraw(bool did_swap) {
  if (inside_begin_frame_)
    last_frame_number_where_draw_was_called_ = current_frame_number_;
  if (commit_state_ == COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW) {
    DCHECK(expect_immediate_begin_frame_for_main_thread_);
    commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
    expect_immediate_begin_frame_for_main_thread_ = false;
  } else if (commit_state_ == COMMIT_STATE_WAITING_FOR_FIRST_DRAW) {
    commit_state_ = COMMIT_STATE_IDLE;
  }
  if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD)
    texture_state_ = LAYER_TEXTURE_STATE_UNLOCKED;

  needs_redraw_ = false;
  needs_forced_redraw_ = false;
  draw_if_possible_failed_ = false;

  if (did_swap)
    swap_used_incomplete_tile_ = false;
}

void SchedulerStateMachine::SetMainThreadNeedsLayerTextures() {
  DCHECK(!main_thread_needs_layer_textures_);
  DCHECK_NE(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD);
  main_thread_needs_layer_textures_ = true;
}

bool SchedulerStateMachine::BeginFrameNeededToDrawByImplThread() const {
  // TODO(brianderson): Remove this hack once the Scheduler controls activation,
  // otherwise we can get stuck in OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION.
  // It lies that we actually need to draw, but we won't actually draw
  // if we can't. This will cause WebView to potentially have corruption
  // in the first few frames, but this workaround is being removed soon.
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION)
    return true;

  // If we can't draw, don't tick until we are notified that we can draw again.
  if (!can_draw_)
    return false;

  if (needs_forced_redraw_)
    return true;

  if (visible_ && swap_used_incomplete_tile_)
    return true;

  return needs_redraw_ && visible_ && HasInitializedOutputSurface();
}

bool SchedulerStateMachine::ProactiveBeginFrameWantedByImplThread() const {
  // Do not be proactive when invisible.
  if (!visible_ || output_surface_state_ != OUTPUT_SURFACE_ACTIVE)
    return false;

  // We should proactively request a BeginFrame if a commit or a tree activation
  // is pending.
  return (needs_commit_ || needs_forced_commit_ ||
          commit_state_ != COMMIT_STATE_IDLE || has_pending_tree_);
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

void SchedulerStateMachine::DidSwapUseIncompleteTile() {
  swap_used_incomplete_tile_ = true;
}

void SchedulerStateMachine::SetNeedsForcedRedraw() {
  needs_forced_redraw_ = true;
}

void SchedulerStateMachine::DidDrawIfPossibleCompleted(bool success) {
  draw_if_possible_failed_ = !success;
  if (draw_if_possible_failed_) {
    needs_redraw_ = true;
    needs_commit_ = true;
    consecutive_failed_draws_++;
    if (settings_.timeout_and_draw_when_animation_checkerboards &&
        consecutive_failed_draws_ >=
        maximum_number_of_failed_draws_before_draw_is_forced_) {
      consecutive_failed_draws_ = 0;
      // We need to force a draw, but it doesn't make sense to do this until
      // we've committed and have new textures.
      needs_forced_redraw_after_next_commit_ = true;
    }
  } else {
    consecutive_failed_draws_ = 0;
  }
}

void SchedulerStateMachine::SetNeedsCommit() { needs_commit_ = true; }

void SchedulerStateMachine::SetNeedsForcedCommit() {
  needs_forced_commit_ = true;
  expect_immediate_begin_frame_for_main_thread_ = true;
}

void SchedulerStateMachine::FinishCommit() {
  DCHECK(commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS ||
         (expect_immediate_begin_frame_for_main_thread_ &&
          commit_state_ != COMMIT_STATE_IDLE))
      << *AsValue();
  commit_state_ = COMMIT_STATE_READY_TO_COMMIT;
}

void SchedulerStateMachine::BeginFrameAbortedByMainThread(bool did_handle) {
  DCHECK_EQ(commit_state_, COMMIT_STATE_FRAME_IN_PROGRESS);
  if (did_handle) {
    bool commit_was_aborted = true;
    HandleCommitInternal(commit_was_aborted);
  } else if (expect_immediate_begin_frame_for_main_thread_) {
    expect_immediate_begin_frame_for_main_thread_ = false;
  } else {
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

void SchedulerStateMachine::SetHasPendingTree(bool has_pending_tree) {
  if (has_pending_tree_ && !has_pending_tree) {
    // There is a new active tree.
    if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION)
      output_surface_state_ = OUTPUT_SURFACE_ACTIVE;

    needs_redraw_ = true;
  }
  has_pending_tree_ = has_pending_tree;
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

void SchedulerStateMachine::SetMaximumNumberOfFailedDrawsBeforeDrawIsForced(
    int num_draws) {
  maximum_number_of_failed_draws_before_draw_is_forced_ = num_draws;
}

}  // namespace cc
