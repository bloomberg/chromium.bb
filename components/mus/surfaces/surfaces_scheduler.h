// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_SURFACES_SCHEDULER_H_
#define COMPONENTS_MUS_SURFACES_SURFACES_SCHEDULER_H_

#include <set>

#include "cc/scheduler/scheduler.h"

namespace cc {
class Display;
class RenderingStatsInstrumentation;
}

namespace mus {

// TODO(brianderson): Reconcile with DisplayScheduler crbug.com/476676
class SurfacesScheduler : public cc::SchedulerClient {
 public:
  SurfacesScheduler();
  ~SurfacesScheduler() override;

  void SetNeedsDraw();

  void OnVSyncParametersUpdated(base::TimeTicks timebase,
                                base::TimeDelta interval);

  void AddDisplay(cc::Display* display);
  void RemoveDisplay(cc::Display* display);

 private:
  void WillBeginImplFrame(const cc::BeginFrameArgs& args) override;
  void DidFinishImplFrame() override;
  void ScheduledActionSendBeginMainFrame(
      const cc::BeginFrameArgs& args) override;
  cc::DrawResult ScheduledActionDrawAndSwapIfPossible() override;
  cc::DrawResult ScheduledActionDrawAndSwapForced() override;
  void ScheduledActionAnimate() override;
  void ScheduledActionCommit() override;
  void ScheduledActionActivateSyncTree() override;
  void ScheduledActionBeginOutputSurfaceCreation() override;
  void ScheduledActionPrepareTiles() override;
  void ScheduledActionInvalidateOutputSurface() override;
  void SendBeginFramesToChildren(const cc::BeginFrameArgs& args) override;
  void SendBeginMainFrameNotExpectedSoon() override;

  std::set<cc::Display*> displays_;
  scoped_ptr<cc::Scheduler> scheduler_;
  scoped_ptr<cc::RenderingStatsInstrumentation>
      rendering_stats_instrumentation_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesScheduler);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_SURFACES_SCHEDULER_H_
