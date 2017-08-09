// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class BeginFrameSource;
class SurfaceInfo;

class VIZ_SERVICE_EXPORT DisplaySchedulerClient {
 public:
  virtual ~DisplaySchedulerClient() {}

  virtual bool DrawAndSwap() = 0;
  virtual bool SurfaceHasUndrawnFrame(const SurfaceId& surface_id) const = 0;
  virtual bool SurfaceDamaged(const SurfaceId& surface_id,
                              const BeginFrameAck& ack) = 0;
  virtual void SurfaceDiscarded(const SurfaceId& surface_id) = 0;
  virtual void DidFinishFrame(const BeginFrameAck& ack) = 0;
};

class VIZ_SERVICE_EXPORT DisplayScheduler : public BeginFrameObserverBase,
                                            public SurfaceObserver {
 public:
  DisplayScheduler(BeginFrameSource* begin_frame_source,
                   base::SingleThreadTaskRunner* task_runner,
                   int max_pending_swaps,
                   bool wait_for_all_surfaces_before_draw = false);
  ~DisplayScheduler() override;

  void SetClient(DisplaySchedulerClient* client);

  void SetVisible(bool visible);
  void SetRootSurfaceResourcesLocked(bool locked);
  void ForceImmediateSwapIfPossible();
  virtual void DisplayResized();
  virtual void SetNewRootSurface(const SurfaceId& root_surface_id);
  virtual void ProcessSurfaceDamage(const SurfaceId& surface_id,
                                    const BeginFrameAck& ack,
                                    bool display_damaged);

  virtual void DidSwapBuffers();
  void DidReceiveSwapBuffersAck();

  void OutputSurfaceLost();

  // BeginFrameObserverBase implementation.
  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  // SurfaceObserver implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;
  void OnSurfaceActivated(const SurfaceId& surface_id) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override;
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack) override;
  void OnSurfaceDiscarded(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override;
  void OnSurfaceWillDraw(const SurfaceId& surface_id) override;

 protected:
  enum class BeginFrameDeadlineMode { kImmediate, kRegular, kLate, kNone };
  base::TimeTicks DesiredBeginFrameDeadlineTime() const;
  BeginFrameDeadlineMode AdjustedBeginFrameDeadlineMode() const;
  BeginFrameDeadlineMode DesiredBeginFrameDeadlineMode() const;
  virtual void ScheduleBeginFrameDeadline();
  bool AttemptDrawAndSwap();
  void OnBeginFrameDeadline();
  bool DrawAndSwap();
  void StartObservingBeginFrames();
  void StopObservingBeginFrames();
  bool ShouldDraw();
  void DidFinishFrame(bool did_draw);
  // Updates |has_pending_surfaces_| and returns whether its value changed.
  bool UpdateHasPendingSurfaces();

  DisplaySchedulerClient* client_;
  BeginFrameSource* begin_frame_source_;
  base::SingleThreadTaskRunner* task_runner_;

  BeginFrameArgs current_begin_frame_args_;
  base::Closure begin_frame_deadline_closure_;
  base::CancelableClosure begin_frame_deadline_task_;
  base::TimeTicks begin_frame_deadline_task_time_;

  base::CancelableClosure missed_begin_frame_task_;
  bool inside_surface_damaged_;

  bool visible_;
  bool output_surface_lost_;
  bool root_surface_resources_locked_;

  bool inside_begin_frame_deadline_interval_;
  bool needs_draw_;
  bool expecting_root_surface_damage_because_of_resize_;
  bool has_pending_surfaces_;

  struct SurfaceBeginFrameState {
    BeginFrameArgs last_args;
    BeginFrameAck last_ack;
  };
  base::flat_map<SurfaceId, SurfaceBeginFrameState> surface_states_;

  int next_swap_id_;
  int pending_swaps_;
  int max_pending_swaps_;
  bool wait_for_all_surfaces_before_draw_;

  bool observing_begin_frame_source_;

  SurfaceId root_surface_id_;

  base::WeakPtrFactory<DisplayScheduler> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayScheduler);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_
