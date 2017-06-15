// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
#define CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/tiles/tile_priority.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {

enum class ScrollHandlerState {
  SCROLL_AFFECTS_SCROLL_HANDLER,
  SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER,
};
const char* ScrollHandlerStateToString(ScrollHandlerState state);

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
  ~SchedulerStateMachine();

  enum CompositorFrameSinkState {
    COMPOSITOR_FRAME_SINK_NONE,
    COMPOSITOR_FRAME_SINK_ACTIVE,
    COMPOSITOR_FRAME_SINK_CREATING,
    COMPOSITOR_FRAME_SINK_WAITING_FOR_FIRST_COMMIT,
    COMPOSITOR_FRAME_SINK_WAITING_FOR_FIRST_ACTIVATION,
  };
  static const char* CompositorFrameSinkStateToString(
      CompositorFrameSinkState state);

  // Note: BeginImplFrameState does not cycle through these states in a fixed
  // order on all platforms. It's up to the scheduler to set these correctly.
  enum BeginImplFrameState {
    BEGIN_IMPL_FRAME_STATE_IDLE,
    BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME,
    BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE,
  };
  static const char* BeginImplFrameStateToString(BeginImplFrameState state);

  enum BeginImplFrameDeadlineMode {
    BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE,
    BEGIN_IMPL_FRAME_DEADLINE_MODE_IMMEDIATE,
    BEGIN_IMPL_FRAME_DEADLINE_MODE_REGULAR,
    BEGIN_IMPL_FRAME_DEADLINE_MODE_LATE,
    BEGIN_IMPL_FRAME_DEADLINE_MODE_BLOCKED_ON_READY_TO_DRAW,
  };
  static const char* BeginImplFrameDeadlineModeToString(
      BeginImplFrameDeadlineMode mode);

  enum BeginMainFrameState {
    BEGIN_MAIN_FRAME_STATE_IDLE,
    BEGIN_MAIN_FRAME_STATE_SENT,
    BEGIN_MAIN_FRAME_STATE_STARTED,
    BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT,
  };
  static const char* BeginMainFrameStateToString(BeginMainFrameState state);

  enum ForcedRedrawOnTimeoutState {
    FORCED_REDRAW_STATE_IDLE,
    FORCED_REDRAW_STATE_WAITING_FOR_COMMIT,
    FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION,
    FORCED_REDRAW_STATE_WAITING_FOR_DRAW,
  };
  static const char* ForcedRedrawOnTimeoutStateToString(
      ForcedRedrawOnTimeoutState state);

  BeginMainFrameState begin_main_frame_state() const {
    return begin_main_frame_state_;
  }

  bool CommitPending() const {
    return begin_main_frame_state_ == BEGIN_MAIN_FRAME_STATE_SENT ||
           begin_main_frame_state_ == BEGIN_MAIN_FRAME_STATE_STARTED ||
           begin_main_frame_state_ == BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT;
  }

  bool NewActiveTreeLikely() const {
    return needs_begin_main_frame_ || CommitPending() || has_pending_tree_;
  }

  bool RedrawPending() const { return needs_redraw_; }
  bool PrepareTilesPending() const { return needs_prepare_tiles_; }

  enum Action {
    ACTION_NONE,
    ACTION_SEND_BEGIN_MAIN_FRAME,
    ACTION_COMMIT,
    ACTION_ACTIVATE_SYNC_TREE,
    ACTION_PERFORM_IMPL_SIDE_INVALIDATION,
    ACTION_DRAW_IF_POSSIBLE,
    ACTION_DRAW_FORCED,
    ACTION_DRAW_ABORT,
    ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION,
    ACTION_PREPARE_TILES,
    ACTION_INVALIDATE_COMPOSITOR_FRAME_SINK,
    ACTION_NOTIFY_BEGIN_MAIN_FRAME_NOT_SENT,
  };
  static const char* ActionToString(Action action);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
  void AsValueInto(base::trace_event::TracedValue* dict) const;

  Action NextAction() const;
  void WillSendBeginMainFrame();
  void WillNotifyBeginMainFrameNotSent();
  void WillCommit(bool commit_had_no_updates);
  void WillActivate();
  void WillDraw();
  void WillBeginCompositorFrameSinkCreation();
  void WillPrepareTiles();
  void WillInvalidateCompositorFrameSink();
  void WillPerformImplSideInvalidation();

  void DidDraw(DrawResult draw_result);

  void AbortDraw();

  // Indicates whether the impl thread needs a BeginImplFrame callback in order
  // to make progress.
  bool BeginFrameNeeded() const;

  // Indicates that the Scheduler has received a BeginFrame that did not require
  // a BeginImplFrame, because the Scheduler stopped observing BeginFrames.
  // Updates the sequence and freshness numbers for the dropped BeginFrame.
  void OnBeginFrameDroppedNotObserving(uint32_t source_id,
                                       uint64_t sequence_number);

  // Indicates that the system has entered and left a BeginImplFrame callback.
  // The scheduler will not draw more than once in a given BeginImplFrame
  // callback nor send more than one BeginMainFrame message.
  void OnBeginImplFrame(uint32_t source_id, uint64_t sequence_number);
  // Indicates that the scheduler has entered the draw phase. The scheduler
  // will not draw more than once in a single draw phase.
  // TODO(sunnyps): Rename OnBeginImplFrameDeadline to OnDraw or similar.
  void OnBeginImplFrameDeadline();
  void OnBeginImplFrameIdle();

  int current_frame_number() const { return current_frame_number_; }

  BeginImplFrameState begin_impl_frame_state() const {
    return begin_impl_frame_state_;
  }
  BeginImplFrameDeadlineMode CurrentBeginImplFrameDeadlineMode() const;

  // If the main thread didn't manage to produce a new frame in time for the
  // impl thread to draw, it is in a high latency mode.
  bool main_thread_missed_last_deadline() const {
    return main_thread_missed_last_deadline_;
  }

  bool IsDrawThrottled() const;

  // Indicates whether the LayerTreeHostImpl is visible.
  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  void SetBeginFrameSourcePaused(bool paused);
  bool begin_frame_source_paused() const { return begin_frame_source_paused_; }

  // Indicates that a redraw is required, either due to the impl tree changing
  // or the screen being damaged and simply needing redisplay.
  void SetNeedsRedraw();
  bool needs_redraw() const { return needs_redraw_; }

  bool OnlyImplSideUpdatesExpected() const;

  // Indicates that prepare-tiles is required. This guarantees another
  // PrepareTiles will occur shortly (even if no redraw is required).
  void SetNeedsPrepareTiles();

  // If the scheduler attempted to draw, this provides feedback regarding
  // whether or not a CompositorFrame was actually submitted. We might skip the
  // submitting anything when there is not damage, for example.
  void DidSubmitCompositorFrame();

  // Notification from the CompositorFrameSink that a submitted frame has been
  // consumed and it is ready for the next one.
  void DidReceiveCompositorFrameAck();

  int pending_submit_frames() const { return pending_submit_frames_; }

  // Indicates whether to prioritize impl thread latency (i.e., animation
  // smoothness) over new content activation.
  void SetTreePrioritiesAndScrollState(TreePriority tree_priority,
                                       ScrollHandlerState scroll_handler_state);

  // Indicates if the main thread will likely respond within 1 vsync.
  void SetCriticalBeginMainFrameToActivateIsFast(bool is_fast);

  // A function of SetTreePrioritiesAndScrollState and
  // SetCriticalBeginMainFrameToActivateIsFast.
  bool ImplLatencyTakesPriority() const;

  // Indicates that a new begin main frame flow needs to be performed, either
  // to pull updates from the main thread to the impl, or to push deltas from
  // the impl thread to main.
  void SetNeedsBeginMainFrame();
  bool needs_begin_main_frame() const { return needs_begin_main_frame_; }

  void SetMainThreadWantsBeginMainFrameNotExpectedMessages(bool new_state);
  bool wants_begin_main_frame_not_expected_messages() const {
    return wants_begin_main_frame_not_expected_;
  }

  // Requests a single impl frame (after the current frame if there is one
  // active).
  void SetNeedsOneBeginImplFrame();

  // Call this only in response to receiving an ACTION_SEND_BEGIN_MAIN_FRAME
  // from NextAction.
  // Indicates that all painting is complete.
  void NotifyReadyToCommit();

  // Call this only in response to receiving an ACTION_SEND_BEGIN_MAIN_FRAME
  // from NextAction if the client rejects the BeginMainFrame message.
  void BeginMainFrameAborted(CommitEarlyOutReason reason);

  // Indicates production should be skipped to recover latency.
  void SetSkipNextBeginMainFrameToReduceLatency();

  // Resourceless software draws are allowed even when invisible.
  void SetResourcelessSoftwareDraw(bool resourceless_draw);

  // Indicates whether drawing would, at this time, make sense.
  // CanDraw can be used to suppress flashes or checkerboarding
  // when such behavior would be undesirable.
  void SetCanDraw(bool can);

  // Indicates that scheduled BeginMainFrame is started.
  void NotifyBeginMainFrameStarted();

  // Indicates that the pending tree is ready for activation.
  void NotifyReadyToActivate();

  // Indicates the active tree's visible tiles are ready to be drawn.
  void NotifyReadyToDraw();

  void SetNeedsImplSideInvalidation();

  bool has_pending_tree() const { return has_pending_tree_; }
  bool active_tree_needs_first_draw() const {
    return active_tree_needs_first_draw_;
  }

  void DidPrepareTiles();
  void DidLoseCompositorFrameSink();
  void DidCreateAndInitializeCompositorFrameSink();
  bool HasInitializedCompositorFrameSink() const;

  // True if we need to abort draws to make forward progress.
  bool PendingDrawsShouldBeAborted() const;

  bool CouldSendBeginMainFrame() const;

  void SetDeferCommits(bool defer_commits);

  void SetVideoNeedsBeginFrames(bool video_needs_begin_frames);
  bool video_needs_begin_frames() const { return video_needs_begin_frames_; }

  bool did_submit_in_last_frame() const { return did_submit_in_last_frame_; }

  bool needs_impl_side_invalidation() const {
    return needs_impl_side_invalidation_;
  }
  bool previous_pending_tree_was_impl_side() const {
    return previous_pending_tree_was_impl_side_;
  }

  uint32_t begin_frame_source_id() const { return begin_frame_source_id_; }
  uint64_t last_begin_frame_sequence_number_active_tree_was_fresh() const {
    return last_begin_frame_sequence_number_active_tree_was_fresh_;
  }
  uint64_t last_begin_frame_sequence_number_compositor_frame_was_fresh() const {
    return last_begin_frame_sequence_number_compositor_frame_was_fresh_;
  }

 protected:
  bool BeginFrameRequiredForAction() const;
  bool BeginFrameNeededForVideo() const;
  bool ProactiveBeginFrameWanted() const;

  bool ShouldPerformImplSideInvalidation() const;
  bool CouldCreatePendingTree() const;

  bool ShouldTriggerBeginImplFrameDeadlineImmediately() const;

  // True if we need to force activations to make forward progress.
  // TODO(sunnyps): Rename this to ShouldAbortCurrentFrame or similar.
  bool PendingActivationsShouldBeForced() const;

  bool ShouldBeginCompositorFrameSinkCreation() const;
  bool ShouldDraw() const;
  bool ShouldActivatePendingTree() const;
  bool ShouldSendBeginMainFrame() const;
  bool ShouldCommit() const;
  bool ShouldPrepareTiles() const;
  bool ShouldInvalidateCompositorFrameSink() const;
  bool ShouldNotifyBeginMainFrameNotSent() const;

  void WillDrawInternal();
  void WillPerformImplSideInvalidationInternal();
  void DidDrawInternal(DrawResult draw_result);

  void UpdateBeginFrameSequenceNumbersForBeginFrame(uint32_t source_id,
                                                    uint64_t sequence_number);
  void UpdateBeginFrameSequenceNumbersForBeginFrameDeadline();

  const SchedulerSettings settings_;

  CompositorFrameSinkState compositor_frame_sink_state_ =
      COMPOSITOR_FRAME_SINK_NONE;
  BeginImplFrameState begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_IDLE;
  BeginMainFrameState begin_main_frame_state_ = BEGIN_MAIN_FRAME_STATE_IDLE;
  ForcedRedrawOnTimeoutState forced_redraw_state_ = FORCED_REDRAW_STATE_IDLE;

  // These fields are used to track the freshness of pending updates in the
  // commit/activate/draw pipeline. The Scheduler uses the CompositorFrame's
  // freshness to fill the |latest_confirmed_sequence_number| field in
  // BeginFrameAcks.
  uint32_t begin_frame_source_id_ = 0;
  uint64_t begin_frame_sequence_number_ = BeginFrameArgs::kInvalidFrameNumber;
  uint64_t last_begin_frame_sequence_number_begin_main_frame_sent_ =
      BeginFrameArgs::kInvalidFrameNumber;
  uint64_t last_begin_frame_sequence_number_pending_tree_was_fresh_ =
      BeginFrameArgs::kInvalidFrameNumber;
  uint64_t last_begin_frame_sequence_number_active_tree_was_fresh_ =
      BeginFrameArgs::kInvalidFrameNumber;
  uint64_t last_begin_frame_sequence_number_compositor_frame_was_fresh_ =
      BeginFrameArgs::kInvalidFrameNumber;

  // These are used for tracing only.
  int commit_count_ = 0;
  int current_frame_number_ = 0;
  int last_frame_number_submit_performed_ = -1;
  int last_frame_number_draw_performed_ = -1;
  int last_frame_number_begin_main_frame_sent_ = -1;
  int last_frame_number_invalidate_compositor_frame_sink_performed_ = -1;

  // These are used to ensure that an action only happens once per frame,
  // deadline, etc.
  bool did_draw_ = false;
  bool did_send_begin_main_frame_for_current_frame_ = true;
  // Initialized to true to prevent begin main frame before begin frames have
  // started. Reset to true when we stop asking for begin frames.
  bool did_notify_begin_main_frame_not_sent_ = true;
  bool did_commit_during_frame_ = false;
  bool did_invalidate_compositor_frame_sink_ = false;
  bool did_perform_impl_side_invalidation_ = false;
  bool did_prepare_tiles_ = false;

  int consecutive_checkerboard_animations_ = 0;
  int pending_submit_frames_ = 0;
  int submit_frames_with_current_compositor_frame_sink_ = 0;
  bool needs_redraw_ = false;
  bool needs_prepare_tiles_ = false;
  bool needs_begin_main_frame_ = false;
  bool needs_one_begin_impl_frame_ = false;
  bool visible_ = false;
  bool begin_frame_source_paused_ = false;
  bool resourceless_draw_ = false;
  bool can_draw_ = false;
  bool has_pending_tree_ = false;
  bool pending_tree_is_ready_for_activation_ = false;
  bool active_tree_needs_first_draw_ = false;
  bool did_create_and_initialize_first_compositor_frame_sink_ = false;
  TreePriority tree_priority_ = NEW_CONTENT_TAKES_PRIORITY;
  ScrollHandlerState scroll_handler_state_ =
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER;
  bool critical_begin_main_frame_to_activate_is_fast_ = true;
  bool main_thread_missed_last_deadline_ = false;
  bool skip_next_begin_main_frame_to_reduce_latency_ = false;
  bool defer_commits_ = false;
  bool video_needs_begin_frames_ = false;
  bool last_commit_had_no_updates_ = false;
  bool wait_for_ready_to_draw_ = false;
  bool did_draw_in_last_frame_ = false;
  bool did_submit_in_last_frame_ = false;
  bool needs_impl_side_invalidation_ = false;

  bool previous_pending_tree_was_impl_side_ = false;
  bool current_pending_tree_is_impl_side_ = false;

  bool wants_begin_main_frame_not_expected_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerStateMachine);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
