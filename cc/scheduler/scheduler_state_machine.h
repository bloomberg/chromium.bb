// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
#define CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_

#include <string>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/scheduler/scheduler_settings.h"

namespace cc {

// The SchedulerStateMachine decides how to coordinate main thread activites
// like painting/running javascript with rendering and input activities on the
// impl thread.
//
// The state machine tracks internal state but is also influenced by external
// state.  Internal state includes things like whether a frame has been
// requested, while external state includes things like the current time being
// near to the vblank time.
//
// The scheduler seperates "what to do next" from the updating of its internal
// state to make testing cleaner.
class CC_EXPORT SchedulerStateMachine {
 public:
  // settings must be valid for the lifetime of this class.
  explicit SchedulerStateMachine(const SchedulerSettings& settings);

  enum CommitState {
    COMMIT_STATE_IDLE,
    COMMIT_STATE_FRAME_IN_PROGRESS,
    COMMIT_STATE_READY_TO_COMMIT,
    COMMIT_STATE_WAITING_FOR_FIRST_DRAW,
    COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW,
  };

  enum TextureState {
    LAYER_TEXTURE_STATE_UNLOCKED,
    LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD,
    LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD,
  };

  enum OutputSurfaceState {
    OUTPUT_SURFACE_ACTIVE,
    OUTPUT_SURFACE_LOST,
    OUTPUT_SURFACE_RECREATING,
  };

  bool CommitPending() const {
    return commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS ||
           commit_state_ == COMMIT_STATE_READY_TO_COMMIT;
  }

  bool RedrawPending() const { return needs_redraw_; }

  enum Action {
    ACTION_NONE,
    ACTION_BEGIN_FRAME,
    ACTION_COMMIT,
    ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS,
    ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED,
    ACTION_DRAW_IF_POSSIBLE,
    ACTION_DRAW_FORCED,
    ACTION_BEGIN_OUTPUT_SURFACE_RECREATION,
    ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD,
  };
  Action NextAction() const;
  void UpdateState(Action action);

  // Indicates whether the scheduler needs a vsync callback in order to make
  // progress.
  bool VSyncCallbackNeeded() const;

  // Indicates that the system has entered and left a vsync callback.
  // The scheduler will not draw more than once in a given vsync callback.
  void DidEnterVSync();
  void DidLeaveVSync();

  // Indicates whether the LayerTreeHostImpl is visible.
  void SetVisible(bool visible);

  // Indicates that a redraw is required, either due to the impl tree changing
  // or the screen being damaged and simply needing redisplay.
  void SetNeedsRedraw();

  // As SetNeedsRedraw(), but ensures the draw will definitely happen even if
  // we are not visible.
  void SetNeedsForcedRedraw();

  // Indicates that a redraw is required because we are currently rendering
  // with a low resolution or checkerboarded tile.
  void DidSwapUseIncompleteTile();

  // Indicates whether ACTION_DRAW_IF_POSSIBLE drew to the screen or not.
  void DidDrawIfPossibleCompleted(bool success);

  // Indicates that a new commit flow needs to be performed, either to pull
  // updates from the main thread to the impl, or to push deltas from the impl
  // thread to main.
  void SetNeedsCommit();

  // As SetNeedsCommit(), but ensures the BeginFrame will definitely happen even
  // if we are not visible.  After this call we expect to go through the forced
  // commit flow and then return to waiting for a non-forced BeginFrame to
  // finish.
  void SetNeedsForcedCommit();

  // Call this only in response to receiving an ACTION_BEGIN_FRAME
  // from NextAction. Indicates that all painting is complete.
  void BeginFrameComplete();

  // Call this only in response to receiving an ACTION_BEGIN_FRAME
  // from NextAction if the client rejects the BeginFrame message.
  void BeginFrameAborted();

  // Request exclusive access to the textures that back single buffered
  // layers on behalf of the main thread. Upon acquisition,
  // ACTION_DRAW_IF_POSSIBLE will not draw until the main thread releases the
  // textures to the impl thread by committing the layers.
  void SetMainThreadNeedsLayerTextures();

  // Indicates whether we can successfully begin a frame at this time.
  void SetCanBeginFrame(bool can) { can_begin_frame_ = can; }

  // Indicates whether drawing would, at this time, make sense.
  // CanDraw can be used to supress flashes or checkerboarding
  // when such behavior would be undesirable.
  void SetCanDraw(bool can);

  // Indicates whether or not there is a pending tree.  This influences
  // whether or not we can succesfully commit at this time.  If the
  // last commit is still being processed (but not blocking), it may not
  // be possible to take another commit yet.  This overrides force commit,
  // as a commit is already still in flight.
  void SetHasPendingTree(bool has_pending_tree);
  bool has_pending_tree() const { return has_pending_tree_; }

  void DidLoseOutputSurface();
  void DidRecreateOutputSurface();

  // Exposed for testing purposes.
  void SetMaximumNumberOfFailedDrawsBeforeDrawIsForced(int num_draws);

  std::string ToString();

 protected:
  bool ShouldDrawForced() const;
  bool DrawSuspendedUntilCommit() const;
  bool ScheduledToDraw() const;
  bool ShouldDraw() const;
  bool ShouldAttemptTreeActivation() const;
  bool ShouldAcquireLayerTexturesForMainThread() const;
  bool ShouldCheckForCompletedTileUploads() const;
  bool HasDrawnThisFrame() const;
  bool HasAttemptedTreeActivationThisFrame() const;
  bool HasCheckedForCompletedTileUploadsThisFrame() const;

  const SchedulerSettings settings_;

  CommitState commit_state_;

  int current_frame_number_;
  int last_frame_number_where_draw_was_called_;
  int last_frame_number_where_tree_activation_attempted_;
  int last_frame_number_where_check_for_completed_tile_uploads_called_;
  int consecutive_failed_draws_;
  int maximum_number_of_failed_draws_before_draw_is_forced_;
  bool needs_redraw_;
  bool swap_used_incomplete_tile_;
  bool needs_forced_redraw_;
  bool needs_forced_redraw_after_next_commit_;
  bool needs_commit_;
  bool needs_forced_commit_;
  bool expect_immediate_begin_frame_;
  bool main_thread_needs_layer_textures_;
  bool inside_vsync_;
  bool visible_;
  bool can_begin_frame_;
  bool can_draw_;
  bool has_pending_tree_;
  bool draw_if_possible_failed_;
  TextureState texture_state_;
  OutputSurfaceState output_surface_state_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerStateMachine);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
