// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display_scheduler.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/surface_info.h"

namespace cc {

DisplayScheduler::DisplayScheduler(BeginFrameSource* begin_frame_source,
                                   base::SingleThreadTaskRunner* task_runner,
                                   int max_pending_swaps)
    : client_(nullptr),
      begin_frame_source_(begin_frame_source),
      task_runner_(task_runner),
      inside_surface_damaged_(false),
      visible_(false),
      output_surface_lost_(false),
      root_surface_resources_locked_(true),
      inside_begin_frame_deadline_interval_(false),
      needs_draw_(false),
      expecting_root_surface_damage_because_of_resize_(false),
      has_pending_surfaces_(false),
      next_swap_id_(1),
      pending_swaps_(0),
      max_pending_swaps_(max_pending_swaps),
      observing_begin_frame_source_(false),
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
  needs_draw_ = true;
  ScheduleBeginFrameDeadline();
}

// Notification that there was a resize or the root surface changed and
// that we should just draw immediately.
void DisplayScheduler::SetNewRootSurface(const SurfaceId& root_surface_id) {
  TRACE_EVENT0("cc", "DisplayScheduler::SetNewRootSurface");
  root_surface_id_ = root_surface_id;
  BeginFrameAck ack;
  ack.has_damage = true;
  ProcessSurfaceDamage(root_surface_id, ack, true);
}

// Indicates that there was damage to one of the surfaces.
// Has some logic to wait for multiple active surfaces before
// triggering the deadline.
void DisplayScheduler::ProcessSurfaceDamage(const SurfaceId& surface_id,
                                            const BeginFrameAck& ack,
                                            bool display_damaged) {
  TRACE_EVENT1("cc", "DisplayScheduler::SurfaceDamaged", "surface_id",
               surface_id.ToString());

  // We may cause a new BeginFrame to be run inside this method, but to help
  // avoid being reentrant to the caller of SurfaceDamaged, track when this is
  // happening with |inside_surface_damaged_|.
  base::AutoReset<bool> auto_reset(&inside_surface_damaged_, true);

  if (display_damaged) {
    needs_draw_ = true;

    if (surface_id == root_surface_id_)
      expecting_root_surface_damage_because_of_resize_ = false;

    StartObservingBeginFrames();
  }

  // Update surface state.
  bool valid_ack = ack.sequence_number != BeginFrameArgs::kInvalidFrameNumber;
  if (valid_ack) {
    auto it = surface_states_.find(surface_id);
    if (it != surface_states_.end())
      it->second.last_ack = ack;
    else
      valid_ack = false;
  }

  bool pending_surfaces_changed = false;
  if (display_damaged || valid_ack)
    pending_surfaces_changed = UpdateHasPendingSurfaces();

  if (display_damaged || pending_surfaces_changed)
    ScheduleBeginFrameDeadline();
}

bool DisplayScheduler::UpdateHasPendingSurfaces() {
  // If we're not currently inside a deadline interval, we will call
  // UpdateHasPendingSurfaces() again during OnBeginFrameImpl().
  if (!inside_begin_frame_deadline_interval_ || !client_)
    return false;

  bool old_value = has_pending_surfaces_;

  for (const std::pair<SurfaceId, SurfaceBeginFrameState>& entry :
       surface_states_) {
    const SurfaceId& surface_id = entry.first;
    const SurfaceBeginFrameState& state = entry.second;

    // Surface is ready if it hasn't received the current BeginFrame or receives
    // BeginFrames from a different source and thus likely belongs to a
    // different surface hierarchy.
    uint32_t source_id = current_begin_frame_args_.source_id;
    uint64_t sequence_number = current_begin_frame_args_.sequence_number;
    if (!state.last_args.IsValid() || state.last_args.source_id != source_id ||
        state.last_args.sequence_number != sequence_number) {
      continue;
    }

    // Surface is ready if it has acknowledged the current BeginFrame.
    if (state.last_ack.source_id == source_id &&
        state.last_ack.sequence_number == sequence_number) {
      continue;
    }

    // Surface is ready if there is an undrawn active CompositorFrame, because
    // its producer is CompositorFrameAck throttled.
    if (client_->SurfaceHasUndrawnFrame(entry.first))
      continue;

    has_pending_surfaces_ = true;
    TRACE_EVENT_INSTANT2("cc", "DisplayScheduler::UpdateHasPendingSurfaces",
                         TRACE_EVENT_SCOPE_THREAD, "has_pending_surfaces",
                         has_pending_surfaces_, "pending_surface_id",
                         surface_id.ToString());
    return has_pending_surfaces_ != old_value;
  }
  has_pending_surfaces_ = false;
  TRACE_EVENT_INSTANT1("cc", "DisplayScheduler::UpdateHasPendingSurfaces",
                       TRACE_EVENT_SCOPE_THREAD, "has_pending_surfaces",
                       has_pending_surfaces_);
  return has_pending_surfaces_ != old_value;
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

  needs_draw_ = false;
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
  if (inside_begin_frame_deadline_interval_)
    OnBeginFrameDeadline();

  // Schedule the deadline.
  current_begin_frame_args_ = save_args;
  current_begin_frame_args_.deadline -=
      BeginFrameArgs::DefaultEstimatedParentDrawTime();
  inside_begin_frame_deadline_interval_ = true;
  UpdateHasPendingSurfaces();
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

void DisplayScheduler::OnSurfaceCreated(const SurfaceInfo& surface_info) {
  SurfaceId surface_id = surface_info.id();
  DCHECK(!base::ContainsKey(surface_states_, surface_id));
  surface_states_[surface_id] = SurfaceBeginFrameState();
}

void DisplayScheduler::OnSurfaceDestroyed(const SurfaceId& surface_id) {
  auto it = surface_states_.find(surface_id);
  if (it == surface_states_.end())
    return;
  surface_states_.erase(it);
  if (UpdateHasPendingSurfaces())
    ScheduleBeginFrameDeadline();
}

bool DisplayScheduler::OnSurfaceDamaged(const SurfaceId& surface_id,
                                        const BeginFrameAck& ack) {
  bool damaged = client_->SurfaceDamaged(surface_id, ack);
  ProcessSurfaceDamage(surface_id, ack, damaged);

  return damaged;
}

void DisplayScheduler::OnSurfaceDiscarded(const SurfaceId& surface_id) {
  client_->SurfaceDiscarded(surface_id);
}

void DisplayScheduler::OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                               const BeginFrameArgs& args) {
  TRACE_EVENT1("cc", "DisplayScheduler::SurfaceDamageExpected", "surface_id",
               surface_id.ToString());
  auto it = surface_states_.find(surface_id);
  if (it == surface_states_.end())
    return;
  it->second.last_args = args;
  if (UpdateHasPendingSurfaces())
    ScheduleBeginFrameDeadline();
}

void DisplayScheduler::OnSurfaceWillDraw(const SurfaceId& surface_id) {}

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

  bool all_surfaces_ready =
      !has_pending_surfaces_ && root_surface_id_.is_valid();
  if (!needs_draw_ && !all_surfaces_ready) {
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

  if (all_surfaces_ready && !expecting_root_surface_damage_because_of_resize_) {
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
    // TODO(eseckler): Should we avoid going idle if
    // |expecting_root_surface_damage_because_of_resize_| is true?
    expecting_root_surface_damage_because_of_resize_ = false;

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
