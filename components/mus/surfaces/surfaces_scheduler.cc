// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/surfaces_scheduler.h"

#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/scheduler/compositor_timing_history.h"
#include "cc/surfaces/display.h"

namespace mus {

SurfacesScheduler::SurfacesScheduler()
    : rendering_stats_instrumentation_(
          cc::RenderingStatsInstrumentation::Create()) {
  cc::SchedulerSettings settings;
  scoped_ptr<cc::CompositorTimingHistory> compositor_timing_history(
      new cc::CompositorTimingHistory(cc::CompositorTimingHistory::NULL_UMA,
                                      rendering_stats_instrumentation_.get()));
  scheduler_ = cc::Scheduler::Create(
      this, settings, 0, base::MessageLoop::current()->task_runner().get(),
      nullptr, compositor_timing_history.Pass());
  scheduler_->SetVisible(true);
  scheduler_->SetCanDraw(true);
  scheduler_->SetNeedsBeginMainFrame();
}

SurfacesScheduler::~SurfacesScheduler() {}

void SurfacesScheduler::SetNeedsDraw() {
  // Don't tell the scheduler we need to draw if we have no active displays
  // which can happen if we haven't initialized displays yet or if all active
  // displays have lost their context.
  if (!displays_.empty())
    scheduler_->SetNeedsRedraw();
}

void SurfacesScheduler::OnVSyncParametersUpdated(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  scheduler_->CommitVSyncParameters(timebase, interval);
}

void SurfacesScheduler::AddDisplay(cc::Display* display) {
  DCHECK(displays_.find(display) == displays_.end());
  displays_.insert(display);

  // A draw might be necessary (e.g., this display might be getting added on
  // resumption from the app being in the background as happens on android).
  SetNeedsDraw();
}

void SurfacesScheduler::RemoveDisplay(cc::Display* display) {
  auto it = displays_.find(display);
  DCHECK(it != displays_.end());
  displays_.erase(it);
}

void SurfacesScheduler::WillBeginImplFrame(const cc::BeginFrameArgs& args) {}

void SurfacesScheduler::DidFinishImplFrame() {}

void SurfacesScheduler::ScheduledActionSendBeginMainFrame(
    const cc::BeginFrameArgs& args) {
  scheduler_->NotifyBeginMainFrameStarted(base::TimeTicks());
  scheduler_->NotifyReadyToCommit();
}

cc::DrawResult SurfacesScheduler::ScheduledActionDrawAndSwapIfPossible() {
  for (const auto& it : displays_) {
    it->DrawAndSwap();
  }
  return cc::DRAW_SUCCESS;
}

cc::DrawResult SurfacesScheduler::ScheduledActionDrawAndSwapForced() {
  NOTREACHED() << "ScheduledActionDrawAndSwapIfPossible always succeeds.";
  return cc::DRAW_SUCCESS;
}

void SurfacesScheduler::ScheduledActionAnimate() {}

void SurfacesScheduler::ScheduledActionCommit() {
  scheduler_->NotifyReadyToActivate();
}

void SurfacesScheduler::ScheduledActionActivateSyncTree() {}

void SurfacesScheduler::ScheduledActionBeginOutputSurfaceCreation() {
  scheduler_->DidCreateAndInitializeOutputSurface();
}

void SurfacesScheduler::ScheduledActionPrepareTiles() {}

void SurfacesScheduler::ScheduledActionInvalidateOutputSurface() {}

void SurfacesScheduler::SendBeginFramesToChildren(
    const cc::BeginFrameArgs& args) {}

void SurfacesScheduler::SendBeginMainFrameNotExpectedSoon() {}

}  // namespace mus
