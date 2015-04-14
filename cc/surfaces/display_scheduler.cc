// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display_scheduler.h"

#include <vector>

#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/output_surface.h"
#include "ui/gfx/frame_time.h"

namespace cc {

DisplayScheduler::DisplayScheduler(
    DisplaySchedulerClient* client,
    BeginFrameSource* begin_frame_source,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int max_pending_swaps)
    : client_(client),
      begin_frame_source_(begin_frame_source),
      task_runner_(task_runner),
      output_surface_lost_(false),
      resources_locked_by_browser_(true),
      inside_begin_frame_deadline_interval_(false),
      needs_draw_(false),
      entire_display_damaged_(false),
      all_active_surfaces_ready_to_draw_(false),
      pending_swaps_(0),
      max_pending_swaps_(max_pending_swaps),
      weak_ptr_factory_(this) {
  begin_frame_source_->AddObserver(this);
  begin_frame_deadline_closure_ = base::Bind(
      &DisplayScheduler::OnBeginFrameDeadline, weak_ptr_factory_.GetWeakPtr());
}

DisplayScheduler::~DisplayScheduler() {
  begin_frame_source_->RemoveObserver(this);
}

// If we try to draw when the Browser has locked it's resources, the
// draw will fail.
void DisplayScheduler::SetResourcesLockedByBrowser(bool locked) {
  resources_locked_by_browser_ = locked;
  ScheduleBeginFrameDeadline();
}

// Notification that there was a resize or the root surface changed and
// that we should just draw immediately.
void DisplayScheduler::EntireDisplayDamaged() {
  TRACE_EVENT0("cc", "DisplayScheduler::EntireDisplayDamaged");
  needs_draw_ = true;
  entire_display_damaged_ = true;

  begin_frame_source_->SetNeedsBeginFrames(!output_surface_lost_);
  ScheduleBeginFrameDeadline();
}

// Indicates that there was damage to one of the surfaces.
// Has some logic to wait for multiple active surfaces before
// triggering the deadline.
void DisplayScheduler::SurfaceDamaged(SurfaceId surface_id) {
  TRACE_EVENT1("cc", "DisplayScheduler::SurfaceDamaged", "surface_id",
               surface_id.id);

  needs_draw_ = true;

  surface_ids_damaged_.insert(surface_id);

  // TODO(mithro): Use hints from SetNeedsBeginFrames.
  all_active_surfaces_ready_to_draw_ = base::STLIncludes(
      surface_ids_damaged_, surface_ids_to_expect_damage_from_);

  begin_frame_source_->SetNeedsBeginFrames(!output_surface_lost_);
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::OutputSurfaceLost() {
  TRACE_EVENT0("cc", "DisplayScheduler::OutputSurfaceLost");
  output_surface_lost_ = true;
  begin_frame_source_->SetNeedsBeginFrames(false);
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::DrawAndSwap() {
  TRACE_EVENT0("cc", "DisplayScheduler::DrawAndSwap");
  DCHECK_LT(pending_swaps_, max_pending_swaps_);
  DCHECK(!output_surface_lost_);

  bool success = client_->DrawAndSwap();
  if (!success)
    return;

  needs_draw_ = false;
  entire_display_damaged_ = false;
  all_active_surfaces_ready_to_draw_ = false;

  surface_ids_to_expect_damage_from_ =
      base::STLSetIntersection<std::vector<SurfaceId>>(
          surface_ids_damaged_, surface_ids_damaged_prev_);

  surface_ids_damaged_prev_.swap(surface_ids_damaged_);
  surface_ids_damaged_.clear();
}

bool DisplayScheduler::OnBeginFrameMixInDelegate(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc", "DisplayScheduler::BeginFrame", "args", args.AsValue());

  // If we get another BeginFrame before the previous deadline,
  // synchronously trigger the previous deadline before progressing.
  if (inside_begin_frame_deadline_interval_) {
    OnBeginFrameDeadline();
  }

  // Schedule the deadline.
  current_begin_frame_args_ = args;
  current_begin_frame_args_.deadline -=
      BeginFrameArgs::DefaultEstimatedParentDrawTime();
  inside_begin_frame_deadline_interval_ = true;
  ScheduleBeginFrameDeadline();

  return true;
}

base::TimeTicks DisplayScheduler::DesiredBeginFrameDeadlineTime() {
  if (output_surface_lost_) {
    TRACE_EVENT_INSTANT0("cc", "Lost output surface", TRACE_EVENT_SCOPE_THREAD);
    return base::TimeTicks();
  }

  if (!needs_draw_) {
    TRACE_EVENT_INSTANT0("cc", "No damage yet", TRACE_EVENT_SCOPE_THREAD);
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  if (pending_swaps_ >= max_pending_swaps_) {
    TRACE_EVENT_INSTANT0("cc", "Swap throttled", TRACE_EVENT_SCOPE_THREAD);
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  if (resources_locked_by_browser_) {
    TRACE_EVENT_INSTANT0("cc", "Resources locked by Browser",
                         TRACE_EVENT_SCOPE_THREAD);
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  // TODO(mithro): Be smarter about resize deadlines.
  if (entire_display_damaged_) {
    TRACE_EVENT_INSTANT0("cc", "Entire display damaged",
                         TRACE_EVENT_SCOPE_THREAD);
    return base::TimeTicks();
  }

  if (all_active_surfaces_ready_to_draw_) {
    TRACE_EVENT_INSTANT0("cc", "Active surfaces ready",
                         TRACE_EVENT_SCOPE_THREAD);
    return base::TimeTicks();
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
      desired_deadline == begin_frame_deadline_task_time_)
    return;

  // Schedule the deadline.
  begin_frame_deadline_task_time_ = desired_deadline;
  begin_frame_deadline_task_.Cancel();
  begin_frame_deadline_task_.Reset(begin_frame_deadline_closure_);

  base::TimeDelta delta =
      std::max(base::TimeDelta(), desired_deadline - base::TimeTicks::Now());
  task_runner_->PostDelayedTask(FROM_HERE,
                                begin_frame_deadline_task_.callback(), delta);
}

void DisplayScheduler::OnBeginFrameDeadline() {
  TRACE_EVENT0("cc", "DisplayScheduler::OnBeginFrameDeadline");
  inside_begin_frame_deadline_interval_ = false;
  begin_frame_deadline_task_.Cancel();
  begin_frame_deadline_task_time_ = base::TimeTicks();

  if (needs_draw_ && !output_surface_lost_) {
    if (pending_swaps_ < max_pending_swaps_ && !resources_locked_by_browser_)
      DrawAndSwap();
  } else {
    begin_frame_source_->SetNeedsBeginFrames(false);
  }

  begin_frame_source_->DidFinishFrame(0);
}

void DisplayScheduler::DidSwapBuffers() {
  pending_swaps_++;
  TRACE_EVENT1("cc", "DisplayScheduler::DidSwapBuffers", "pending_frames",
               pending_swaps_);
}

void DisplayScheduler::DidSwapBuffersComplete() {
  pending_swaps_--;
  TRACE_EVENT1("cc", "DisplayScheduler::DidSwapBuffersComplete",
               "pending_frames", pending_swaps_);
  ScheduleBeginFrameDeadline();
}

}  // namespace cc
