// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display_scheduler.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/output_surface.h"

namespace cc {

DisplayScheduler::DisplayScheduler(base::SingleThreadTaskRunner* task_runner,
                                   int max_pending_swaps)
    : client_(nullptr),
      begin_frame_source_(nullptr),
      task_runner_(task_runner),
      inside_surface_damaged_(false),
      visible_(false),
      output_surface_lost_(false),
      root_surface_resources_locked_(true),
      inside_begin_frame_deadline_interval_(false),
      needs_draw_(false),
      expecting_root_surface_damage_because_of_resize_(false),
      all_active_child_surfaces_ready_to_draw_(false),
      next_swap_id_(1),
      pending_swaps_(0),
      max_pending_swaps_(max_pending_swaps),
      observing_begin_frame_source_(false),
      root_surface_damaged_(false),
      expect_damage_from_root_surface_(false),
      weak_ptr_factory_(this) {
  begin_frame_deadline_closure_ = base::Bind(
      &DisplayScheduler::OnBeginFrameDeadline, weak_ptr_factory_.GetWeakPtr());
}

DisplayScheduler::~DisplayScheduler() {
  StopObservingBeginFrames();
}

void DisplayScheduler::SetClient(DisplaySchedulerClient* client) {
  client_ = client;
}

void DisplayScheduler::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  begin_frame_source_ = begin_frame_source;
}

void DisplayScheduler::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  // If going invisible, we'll stop observing begin frames once we try
  // to draw and fail.
  StartObservingBeginFrames();
  ScheduleBeginFrameDeadline();
}

// If we try to draw when the root surface resources are locked, the
// draw will fail.
void DisplayScheduler::SetRootSurfaceResourcesLocked(bool locked) {
  TRACE_EVENT1("cc", "DisplayScheduler::SetRootSurfaceResourcesLocked",
               "locked", locked);
  root_surface_resources_locked_ = locked;
  ScheduleBeginFrameDeadline();
}

// This is used to force an immediate swap before a resize.
void DisplayScheduler::ForceImmediateSwapIfPossible() {
  TRACE_EVENT0("cc", "DisplayScheduler::ForceImmediateSwapIfPossible");
  bool in_begin = inside_begin_frame_deadline_interval_;
  bool did_draw = AttemptDrawAndSwap();
  if (in_begin)
    DidFinishFrame(did_draw);
}

void DisplayScheduler::DisplayResized() {
  expecting_root_surface_damage_because_of_resize_ = true;
  expect_damage_from_root_surface_ = true;
  needs_draw_ = true;
  ScheduleBeginFrameDeadline();
}

// Notification that there was a resize or the root surface changed and
// that we should just draw immediately.
void DisplayScheduler::SetNewRootSurface(const SurfaceId& root_surface_id) {
  TRACE_EVENT0("cc", "DisplayScheduler::SetNewRootSurface");
  root_surface_id_ = root_surface_id;
  SurfaceDamaged(root_surface_id);
}

// Indicates that there was damage to one of the surfaces.
// Has some logic to wait for multiple active surfaces before
// triggering the deadline.
void DisplayScheduler::SurfaceDamaged(const SurfaceId& surface_id) {
  TRACE_EVENT1("cc", "DisplayScheduler::SurfaceDamaged", "surface_id",
               surface_id.ToString());

  // We may cause a new BeginFrame to be run inside this method, but to help
  // avoid being reentrant to the caller of SurfaceDamaged, track when this is
  // happening with |inside_surface_damaged_|.
  base::AutoReset<bool> auto_reset(&inside_surface_damaged_, true);

  needs_draw_ = true;

  if (surface_id == root_surface_id_) {
    root_surface_damaged_ = true;
    expecting_root_surface_damage_because_of_resize_ = false;
  } else {
    child_surface_ids_damaged_.insert(surface_id);

    // TODO(mithro): Use hints from SetNeedsBeginFrames and SwapAborts.
    all_active_child_surfaces_ready_to_draw_ = base::STLIncludes(
        child_surface_ids_damaged_, child_surface_ids_to_expect_damage_from_);
  }

  StartObservingBeginFrames();
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::OutputSurfaceLost() {
  TRACE_EVENT0("cc", "DisplayScheduler::OutputSurfaceLost");
  output_surface_lost_ = true;
  ScheduleBeginFrameDeadline();
}

bool DisplayScheduler::DrawAndSwap() {
  TRACE_EVENT0("cc", "DisplayScheduler::DrawAndSwap");
  DCHECK_LT(pending_swaps_, max_pending_swaps_);
  DCHECK(!output_surface_lost_);

  bool success = client_->DrawAndSwap();
  if (!success)
    return false;

  child_surface_ids_to_expect_damage_from_ =
      base::STLSetIntersection<std::vector<SurfaceId>>(
          child_surface_ids_damaged_, child_surface_ids_damaged_prev_);

  child_surface_ids_damaged_prev_.swap(child_surface_ids_damaged_);
  child_surface_ids_damaged_.clear();

  needs_draw_ = false;
  all_active_child_surfaces_ready_to_draw_ =
      child_surface_ids_to_expect_damage_from_.empty();

  expect_damage_from_root_surface_ = root_surface_damaged_;
  root_surface_damaged_ = false;
  return true;
}

bool DisplayScheduler::OnBeginFrameDerivedImpl(const BeginFrameArgs& args) {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT2("cc", "DisplayScheduler::BeginFrame", "args", args.AsValue(),
               "now", now);

  if (inside_surface_damaged_) {
    // Repost this so that we don't run a missed BeginFrame on the same
    // callstack. Otherwise we end up running unexpected scheduler actions
    // immediately while inside some other action (such as submitting a
    // CompositorFrame for a SurfaceFactory).
    DCHECK_EQ(args.type, BeginFrameArgs::MISSED);
    DCHECK(missed_begin_frame_task_.IsCancelled());
    missed_begin_frame_task_.Reset(base::Bind(
        base::IgnoreResult(&DisplayScheduler::OnBeginFrameDerivedImpl),
        // The CancelableCallback will not run after it is destroyed, which
        // happens when |this| is destroyed.
        base::Unretained(this), args));
    task_runner_->PostTask(FROM_HERE, missed_begin_frame_task_.callback());
    return true;
  }

  // Save the |BeginFrameArgs| as the callback (missed_begin_frame_task_) can be
  // destroyed if we StopObservingBeginFrames(), and it would take the |args|
  // with it. Instead save the args and cancel the |missed_begin_frame_task_|.
  BeginFrameArgs save_args = args;
  // If we get another BeginFrame before a posted missed frame, just drop the
  // missed frame. Also if this was the missed frame, drop the Callback inside
  // it.
  missed_begin_frame_task_.Cancel();

  // If we get another BeginFrame before the previous deadline,
  // synchronously trigger the previous deadline before progressing.
  if (inside_begin_frame_deadline_interval_) {
    OnBeginFrameDeadline();
  }

  // Schedule the deadline.
  current_begin_frame_args_ = save_args;
  current_begin_frame_args_.deadline -=
      BeginFrameArgs::DefaultEstimatedParentDrawTime();
  inside_begin_frame_deadline_interval_ = true;
  ScheduleBeginFrameDeadline();

  return true;
}

void DisplayScheduler::StartObservingBeginFrames() {
  if (!observing_begin_frame_source_ && ShouldDraw()) {
    begin_frame_source_->AddObserver(this);
    observing_begin_frame_source_ = true;
  }
}

void DisplayScheduler::StopObservingBeginFrames() {
  if (observing_begin_frame_source_) {
    begin_frame_source_->RemoveObserver(this);
    observing_begin_frame_source_ = false;

    // A missed BeginFrame may be queued, so drop that too if we're going to
    // stop listening.
    missed_begin_frame_task_.Cancel();
  }
}

bool DisplayScheduler::ShouldDraw() {
  // Note: When any of these cases becomes true, StartObservingBeginFrames must
  // be called to ensure the draw will happen.
  return needs_draw_ && !output_surface_lost_ && visible_;
}

void DisplayScheduler::OnBeginFrameSourcePausedChanged(bool paused) {
  // BeginFrameSources used with DisplayScheduler do not make use of this
  // feature.
  if (paused)
    NOTIMPLEMENTED();
}

base::TimeTicks DisplayScheduler::DesiredBeginFrameDeadlineTime() {
  if (output_surface_lost_) {
    TRACE_EVENT_INSTANT0("cc", "Lost output surface", TRACE_EVENT_SCOPE_THREAD);
    return base::TimeTicks();
  }

  if (pending_swaps_ >= max_pending_swaps_) {
    TRACE_EVENT_INSTANT0("cc", "Swap throttled", TRACE_EVENT_SCOPE_THREAD);
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  if (!needs_draw_) {
    TRACE_EVENT_INSTANT0("cc", "No damage yet", TRACE_EVENT_SCOPE_THREAD);
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  if (root_surface_resources_locked_) {
    TRACE_EVENT_INSTANT0("cc", "Root surface resources locked",
                         TRACE_EVENT_SCOPE_THREAD);
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  bool root_ready_to_draw =
      !expect_damage_from_root_surface_ || root_surface_damaged_;

  if (all_active_child_surfaces_ready_to_draw_ && root_ready_to_draw) {
    TRACE_EVENT_INSTANT0("cc", "All active surfaces ready",
                         TRACE_EVENT_SCOPE_THREAD);
    return base::TimeTicks();
  }

  // TODO(mithro): Be smarter about resize deadlines.
  if (expecting_root_surface_damage_because_of_resize_) {
    TRACE_EVENT_INSTANT0("cc", "Entire display damaged",
                         TRACE_EVENT_SCOPE_THREAD);
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  // Use an earlier deadline if we are only waiting for the root surface
  // in case our expect_damage_from_root_surface heuristic is incorrect.
  // TODO(mithro): Replace this with SetNeedsBeginFrame and SwapAbort
  // logic.
  if (all_active_child_surfaces_ready_to_draw_ &&
      expect_damage_from_root_surface_) {
    TRACE_EVENT_INSTANT0("cc", "Waiting for damage from root surface",
                         TRACE_EVENT_SCOPE_THREAD);
    // This adjusts the deadline by DefaultEstimatedParentDrawTime for
    // a second time. The first one represented the Surfaces draw to display
    // latency. This one represents root surface commit+raster+draw latency.
    // We treat the root surface differently since it lives on the same thread
    // as Surfaces and waiting for it too long may push out the Surfaces draw.
    // If we also assume the root surface is fast to start a commit after the
    // beginning of a frame, it'll have a chance to lock its resources, which
    // will cause us to wait for it to unlock its resources above.
    // TODO(mithro): Replace hard coded estimates.
    return current_begin_frame_args_.deadline -
           BeginFrameArgs::DefaultEstimatedParentDrawTime();
  }

  TRACE_EVENT_INSTANT0("cc", "More damage expected soon",
                       TRACE_EVENT_SCOPE_THREAD);
  return current_begin_frame_args_.deadline;
}

void DisplayScheduler::ScheduleBeginFrameDeadline() {
  TRACE_EVENT0("cc", "DisplayScheduler::ScheduleBeginFrameDeadline");

  // We need to wait for the next BeginFrame before scheduling a deadline.
  if (!inside_begin_frame_deadline_interval_) {
    TRACE_EVENT_INSTANT0("cc", "Waiting for next BeginFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    DCHECK(begin_frame_deadline_task_.IsCancelled());
    return;
  }

  // Determine the deadline we want to use.
  base::TimeTicks desired_deadline = DesiredBeginFrameDeadlineTime();

  // Avoid re-scheduling the deadline if it's already correctly scheduled.
  if (!begin_frame_deadline_task_.IsCancelled() &&
      desired_deadline == begin_frame_deadline_task_time_) {
    TRACE_EVENT_INSTANT0("cc", "Using existing deadline",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // Schedule the deadline.
  begin_frame_deadline_task_time_ = desired_deadline;
  begin_frame_deadline_task_.Cancel();
  begin_frame_deadline_task_.Reset(begin_frame_deadline_closure_);

  base::TimeDelta delta =
      std::max(base::TimeDelta(), desired_deadline - base::TimeTicks::Now());
  task_runner_->PostDelayedTask(FROM_HERE,
                                begin_frame_deadline_task_.callback(), delta);
  TRACE_EVENT2("cc", "Using new deadline", "delta", delta.ToInternalValue(),
               "desired_deadline", desired_deadline);
}

bool DisplayScheduler::AttemptDrawAndSwap() {
  inside_begin_frame_deadline_interval_ = false;
  begin_frame_deadline_task_.Cancel();
  begin_frame_deadline_task_time_ = base::TimeTicks();

  if (ShouldDraw()) {
    if (pending_swaps_ < max_pending_swaps_ && !root_surface_resources_locked_)
      return DrawAndSwap();
  } else {
    // We are going idle, so reset expectations.
    child_surface_ids_to_expect_damage_from_.clear();
    child_surface_ids_damaged_prev_.clear();
    child_surface_ids_damaged_.clear();
    all_active_child_surfaces_ready_to_draw_ = true;
    expect_damage_from_root_surface_ = false;

    StopObservingBeginFrames();
  }
  return false;
}

void DisplayScheduler::OnBeginFrameDeadline() {
  TRACE_EVENT0("cc", "DisplayScheduler::OnBeginFrameDeadline");
  DCHECK(inside_begin_frame_deadline_interval_);

  bool did_draw = AttemptDrawAndSwap();
  DidFinishFrame(did_draw);
}

void DisplayScheduler::DidFinishFrame(bool did_draw) {
  DCHECK(begin_frame_source_);
  // TODO(eseckler): Let client know that frame was completed.
  begin_frame_source_->DidFinishFrame(this);
}

void DisplayScheduler::DidSwapBuffers() {
  pending_swaps_++;
  uint32_t swap_id = next_swap_id_++;
  TRACE_EVENT_ASYNC_BEGIN0("cc", "DisplayScheduler:pending_swaps", swap_id);
}

void DisplayScheduler::DidReceiveSwapBuffersAck() {
  uint32_t swap_id = next_swap_id_ - pending_swaps_;
  pending_swaps_--;
  TRACE_EVENT_ASYNC_END0("cc", "DisplayScheduler:pending_swaps", swap_id);
  ScheduleBeginFrameDeadline();
}

}  // namespace cc
