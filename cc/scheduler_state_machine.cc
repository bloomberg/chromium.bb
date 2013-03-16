// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler_state_machine.h"

#include "base/logging.h"
#include "base/stringprintf.h"

namespace cc {

SchedulerStateMachine::SchedulerStateMachine(const SchedulerSettings& settings)
    : settings_(settings),
      commit_state_(COMMIT_STATE_IDLE),
      current_frame_number_(0),
      last_frame_number_where_draw_was_called_(-1),
      last_frame_number_where_tree_activation_attempted_(-1),
      last_frame_number_where_check_for_completed_tile_uploads_called_(-1),
      consecutive_failed_draws_(0),
      maximum_number_of_failed_draws_before_draw_is_forced_(3),
      needs_redraw_(false),
      swap_used_incomplete_tile_(false),
      needs_forced_redraw_(false),
      needs_forced_redraw_after_next_commit_(false),
      needs_commit_(false),
      needs_forced_commit_(false),
      expect_immediate_begin_frame_(false),
      main_thread_needs_layer_textures_(false),
      inside_vsync_(false),
      visible_(false),
      can_begin_frame_(false),
      can_draw_(false),
      has_pending_tree_(false),
      draw_if_possible_failed_(false),
      texture_state_(LAYER_TEXTURE_STATE_UNLOCKED),
      output_surface_state_(OUTPUT_SURFACE_ACTIVE) {}

std::string SchedulerStateMachine::ToString() {
  std::string str;
  base::StringAppendF(&str,
                      "settings_.impl_side_painting = %d; ",
                      settings_.impl_side_painting);
  base::StringAppendF(&str, "commit_state_ = %d; ", commit_state_);
  base::StringAppendF(
      &str, "current_frame_number_ = %d; ", current_frame_number_);
  base::StringAppendF(&str,
                      "last_frame_number_where_draw_was_called_ = %d; ",
                      last_frame_number_where_draw_was_called_);
  base::StringAppendF(
      &str,
      "last_frame_number_where_tree_activation_attempted_ = %d; ",
      last_frame_number_where_tree_activation_attempted_);
  base::StringAppendF(
      &str,
      "last_frame_number_where_check_for_completed_tile_uploads_called_ = %d; ",
      last_frame_number_where_check_for_completed_tile_uploads_called_);
  base::StringAppendF(
      &str, "consecutive_failed_draws_ = %d; ", consecutive_failed_draws_);
  base::StringAppendF(
      &str,
      "maximum_number_of_failed_draws_before_draw_is_forced_ = %d; ",
      maximum_number_of_failed_draws_before_draw_is_forced_);
  base::StringAppendF(&str, "needs_redraw_ = %d; ", needs_redraw_);
  base::StringAppendF(
      &str, "swap_used_incomplete_tile_ = %d; ", swap_used_incomplete_tile_);
  base::StringAppendF(
      &str, "needs_forced_redraw_ = %d; ", needs_forced_redraw_);
  base::StringAppendF(&str,
                      "needs_forced_redraw_after_next_commit_ = %d; ",
                      needs_forced_redraw_after_next_commit_);
  base::StringAppendF(&str, "needs_commit_ = %d; ", needs_commit_);
  base::StringAppendF(
      &str, "needs_forced_commit_ = %d; ", needs_forced_commit_);
  base::StringAppendF(&str,
                      "expect_immediate_begin_frame_ = %d; ",
                      expect_immediate_begin_frame_);
  base::StringAppendF(&str,
                      "main_thread_needs_layer_textures_ = %d; ",
                      main_thread_needs_layer_textures_);
  base::StringAppendF(&str, "inside_vsync_ = %d; ", inside_vsync_);
  base::StringAppendF(&str, "visible_ = %d; ", visible_);
  base::StringAppendF(&str, "can_begin_frame_ = %d; ", can_begin_frame_);
  base::StringAppendF(&str, "can_draw_ = %d; ", can_draw_);
  base::StringAppendF(
      &str, "draw_if_possible_failed_ = %d; ", draw_if_possible_failed_);
  base::StringAppendF(&str, "has_pending_tree_ = %d; ", has_pending_tree_);
  base::StringAppendF(&str, "texture_state_ = %d; ", texture_state_);
  base::StringAppendF(
      &str, "output_surface_state_ = %d; ", output_surface_state_);
  return str;
}

bool SchedulerStateMachine::HasDrawnThisFrame() const {
  return current_frame_number_ == last_frame_number_where_draw_was_called_;
}

bool SchedulerStateMachine::HasAttemptedTreeActivationThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_where_tree_activation_attempted_;
}

bool SchedulerStateMachine::HasCheckedForCompletedTileUploadsThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_where_check_for_completed_tile_uploads_called_;
}

bool SchedulerStateMachine::DrawSuspendedUntilCommit() const {
  if (!can_draw_)
    return true;
  if (!visible_)
    return true;
  if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD)
    return true;
  return false;
}

bool SchedulerStateMachine::ScheduledToDraw() const {
  if (!needs_redraw_)
    return false;
  if (DrawSuspendedUntilCommit())
    return false;
  return true;
}

bool SchedulerStateMachine::ShouldDraw() const {
  if (needs_forced_redraw_)
    return true;

  if (!ScheduledToDraw())
    return false;
  if (!inside_vsync_)
    return false;
  if (HasDrawnThisFrame())
    return false;
  if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE)
    return false;
  return true;
}

bool SchedulerStateMachine::ShouldAttemptTreeActivation() const {
  return has_pending_tree_ && inside_vsync_ &&
         !HasAttemptedTreeActivationThisFrame();
}

bool SchedulerStateMachine::ShouldCheckForCompletedTileUploads() const {
  if (!settings_.impl_side_painting)
    return false;
  if (HasCheckedForCompletedTileUploadsThisFrame())
    return false;

  return ShouldAttemptTreeActivation() || ShouldDraw() ||
         swap_used_incomplete_tile_;
}

bool SchedulerStateMachine::ShouldAcquireLayerTexturesForMainThread() const {
  if (!main_thread_needs_layer_textures_)
    return false;
  if (texture_state_ == LAYER_TEXTURE_STATE_UNLOCKED)
    return true;
  DCHECK_EQ(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD);
  // Transfer the lock from impl thread to main thread immediately if the
  // impl thread is not even scheduled to draw. Guards against deadlocking.
  if (!ScheduledToDraw())
    return true;
  if (!VSyncCallbackNeeded())
    return true;
  return false;
}

SchedulerStateMachine::Action SchedulerStateMachine::NextAction() const {
  if (ShouldAcquireLayerTexturesForMainThread())
    return ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD;

  switch (commit_state_) {
    case COMMIT_STATE_IDLE:
      if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE &&
          needs_forced_redraw_)
        return ACTION_DRAW_FORCED;
      if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE &&
          needs_forced_commit_)
        // TODO(enne): Should probably drop the active tree on force commit.
        return has_pending_tree_ ? ACTION_NONE : ACTION_BEGIN_FRAME;
      if (output_surface_state_ == OUTPUT_SURFACE_LOST)
        return ACTION_BEGIN_OUTPUT_SURFACE_RECREATION;
      if (output_surface_state_ == OUTPUT_SURFACE_RECREATING)
        return ACTION_NONE;
      if (ShouldCheckForCompletedTileUploads())
        return ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS;
      if (ShouldAttemptTreeActivation())
        return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
      if (ShouldDraw()) {
        return needs_forced_redraw_ ? ACTION_DRAW_FORCED
                                    : ACTION_DRAW_IF_POSSIBLE;
      }
      if (needs_commit_ &&
          ((visible_ && can_begin_frame_) || needs_forced_commit_))
        // TODO(enne): Should probably drop the active tree on force commit.
        return has_pending_tree_ ? ACTION_NONE : ACTION_BEGIN_FRAME;
      return ACTION_NONE;

    case COMMIT_STATE_FRAME_IN_PROGRESS:
      if (ShouldCheckForCompletedTileUploads())
        return ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS;
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
      if (ShouldCheckForCompletedTileUploads())
        return ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS;
      if (ShouldAttemptTreeActivation())
        return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
      if (ShouldDraw() || output_surface_state_ == OUTPUT_SURFACE_LOST) {
        return needs_forced_redraw_ ? ACTION_DRAW_FORCED
                                    : ACTION_DRAW_IF_POSSIBLE;
      }
      // COMMIT_STATE_WAITING_FOR_FIRST_DRAW wants to enforce a draw. If
      // can_draw_ is false or textures are not available, proceed to the next
      // step (similar as in COMMIT_STATE_IDLE).
      bool can_commit = visible_ || needs_forced_commit_;
      if (needs_commit_ && can_commit && DrawSuspendedUntilCommit())
        return has_pending_tree_ ? ACTION_NONE : ACTION_BEGIN_FRAME;
      return ACTION_NONE;
    }

    case COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW:
      if (ShouldCheckForCompletedTileUploads())
        return ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS;
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

    case ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS:
      last_frame_number_where_check_for_completed_tile_uploads_called_ =
          current_frame_number_;
      return;

    case ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED:
      last_frame_number_where_tree_activation_attempted_ =
          current_frame_number_;
      return;

    case ACTION_BEGIN_FRAME:
      DCHECK(!has_pending_tree_);
      DCHECK(visible_ || needs_forced_commit_);
      commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
      needs_commit_ = false;
      needs_forced_commit_ = false;
      return;

    case ACTION_COMMIT:
      if (expect_immediate_begin_frame_)
        commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW;
      else
        commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
      // When impl-side painting, we draw on activation instead of on commit.
      if (!settings_.impl_side_painting)
        needs_redraw_ = true;
      if (draw_if_possible_failed_)
        last_frame_number_where_draw_was_called_ = -1;

      if (needs_forced_redraw_after_next_commit_) {
        needs_forced_redraw_after_next_commit_ = false;
        needs_forced_redraw_ = true;
      }

      texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD;
      return;

    case ACTION_DRAW_FORCED:
    case ACTION_DRAW_IF_POSSIBLE:
      needs_redraw_ = false;
      needs_forced_redraw_ = false;
      draw_if_possible_failed_ = false;
      swap_used_incomplete_tile_ = false;
      if (inside_vsync_)
        last_frame_number_where_draw_was_called_ = current_frame_number_;
      if (commit_state_ == COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW) {
        DCHECK(expect_immediate_begin_frame_);
        commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
        expect_immediate_begin_frame_ = false;
      } else if (commit_state_ == COMMIT_STATE_WAITING_FOR_FIRST_DRAW) {
        commit_state_ = COMMIT_STATE_IDLE;
      }
      if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD)
        texture_state_ = LAYER_TEXTURE_STATE_UNLOCKED;
      return;

    case ACTION_BEGIN_OUTPUT_SURFACE_RECREATION:
      DCHECK_EQ(commit_state_, COMMIT_STATE_IDLE);
      DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_LOST);
      output_surface_state_ = OUTPUT_SURFACE_RECREATING;
      return;

    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
      texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD;
      main_thread_needs_layer_textures_ = false;
      if (commit_state_ != COMMIT_STATE_FRAME_IN_PROGRESS)
        needs_commit_ = true;
      return;
  }
}

void SchedulerStateMachine::SetMainThreadNeedsLayerTextures() {
  DCHECK(!main_thread_needs_layer_textures_);
  DCHECK_NE(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD);
  main_thread_needs_layer_textures_ = true;
}

bool SchedulerStateMachine::VSyncCallbackNeeded() const {
  // If we have a pending tree, need to keep getting notifications until
  // the tree is ready to be swapped.
  if (has_pending_tree_)
    return true;

  // If we can't draw, don't tick until we are notified that we can draw again.
  if (!can_draw_)
    return false;

  if (needs_forced_redraw_)
    return true;

  if (visible_ && swap_used_incomplete_tile_)
    return true;

  return needs_redraw_ && visible_ &&
         output_surface_state_ == OUTPUT_SURFACE_ACTIVE;
}

void SchedulerStateMachine::DidEnterVSync() { inside_vsync_ = true; }

void SchedulerStateMachine::DidLeaveVSync() {
  current_frame_number_++;
  inside_vsync_ = false;
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
    if (consecutive_failed_draws_ >=
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
  expect_immediate_begin_frame_ = true;
}

void SchedulerStateMachine::BeginFrameComplete() {
  DCHECK(commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS ||
         (expect_immediate_begin_frame_ && commit_state_ != COMMIT_STATE_IDLE))
      << ToString();
  commit_state_ = COMMIT_STATE_READY_TO_COMMIT;
}

void SchedulerStateMachine::BeginFrameAborted() {
  DCHECK_EQ(commit_state_, COMMIT_STATE_FRAME_IN_PROGRESS);
  if (expect_immediate_begin_frame_) {
    expect_immediate_begin_frame_ = false;
  } else {
    commit_state_ = COMMIT_STATE_IDLE;
    SetNeedsCommit();
  }
}

void SchedulerStateMachine::DidLoseOutputSurface() {
  if (output_surface_state_ == OUTPUT_SURFACE_LOST ||
      output_surface_state_ == OUTPUT_SURFACE_RECREATING)
    return;
  output_surface_state_ = OUTPUT_SURFACE_LOST;
}

void SchedulerStateMachine::SetHasPendingTree(bool has_pending_tree) {
  has_pending_tree_ = has_pending_tree;
}

void SchedulerStateMachine::SetCanDraw(bool can) { can_draw_ = can; }

void SchedulerStateMachine::DidRecreateOutputSurface() {
  DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_RECREATING);
  output_surface_state_ = OUTPUT_SURFACE_ACTIVE;
  SetNeedsCommit();
}

void SchedulerStateMachine::SetMaximumNumberOfFailedDrawsBeforeDrawIsForced(
    int num_draws) {
  maximum_number_of_failed_draws_before_draw_is_forced_ = num_draws;
}

}  // namespace cc
