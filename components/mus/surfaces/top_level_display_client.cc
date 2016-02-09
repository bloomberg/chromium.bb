// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/top_level_display_client.h"

#include "base/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/surfaces/direct_output_surface.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "components/mus/surfaces/surfaces_state.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mus {
namespace {
void CallCallback(const base::Closure& callback, cc::SurfaceDrawStatus status) {
  callback.Run();
}
}

TopLevelDisplayClient::TopLevelDisplayClient(
    gfx::AcceleratedWidget widget,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      surfaces_state_(surfaces_state),
      factory_(surfaces_state->manager(), this),
      cc_id_(static_cast<uint64_t>(surfaces_state->next_id_namespace()) << 32) {
  factory_.Create(cc_id_);

  display_.reset(new cc::Display(this, surfaces_state_->manager(), nullptr,
                                 nullptr, cc::RendererSettings()));

  scoped_ptr<cc::OutputSurface> output_surface =
      make_scoped_ptr(new DirectOutputSurface(
          new SurfacesContextProvider(this, widget, gpu_state)));

  int max_frames_pending = output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  synthetic_frame_source_ = cc::SyntheticBeginFrameSource::Create(
      task_runner_.get(), cc::BeginFrameArgs::DefaultInterval());

  scheduler_.reset(
      new cc::DisplayScheduler(display_.get(), synthetic_frame_source_.get(),
                               task_runner_.get(), max_frames_pending));

  if (gpu_state->HardwareRenderingAvailable()) {
    display_->Initialize(std::move(output_surface), scheduler_.get());
  } else {
    // TODO(rjkroege): Implement software compositing.
  }
  display_->Resize(last_submitted_frame_size_);

  // TODO(fsamuel): Plumb the proper device scale factor.
  display_->SetSurfaceId(cc_id_, 1.f /* device_scale_factor */);
}

TopLevelDisplayClient::~TopLevelDisplayClient() {
  factory_.Destroy(cc_id_);
}

void TopLevelDisplayClient::SubmitCompositorFrame(
    scoped_ptr<cc::CompositorFrame> frame,
    const base::Closure& callback) {
  pending_frame_ = std::move(frame);

  last_submitted_frame_size_ =
      pending_frame_->delegated_frame_data->render_pass_list.back()
          ->output_rect.size();
  display_->Resize(last_submitted_frame_size_);
  factory_.SubmitCompositorFrame(cc_id_, std::move(pending_frame_),
                                 base::Bind(&CallCallback, callback));
}

void TopLevelDisplayClient::RequestCopyOfOutput(
    scoped_ptr<cc::CopyOutputRequest> output_request) {
  factory_.RequestCopyOfSurface(cc_id_, std::move(output_request));
}

void TopLevelDisplayClient::CommitVSyncParameters(base::TimeTicks timebase,
                                                  base::TimeDelta interval) {}

void TopLevelDisplayClient::OutputSurfaceLost() {
  if (!display_)  // Shutdown case
    return;
  display_.reset();
}

void TopLevelDisplayClient::SetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {}

void TopLevelDisplayClient::OnVSyncParametersUpdated(int64_t timebase,
                                                     int64_t interval) {
  auto timebase_time_ticks = base::TimeTicks::FromInternalValue(timebase);
  auto interval_time_delta = base::TimeDelta::FromInternalValue(interval);

  if (interval_time_delta.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval_time_delta = cc::BeginFrameArgs::DefaultInterval();
  }

  synthetic_frame_source_->OnUpdateVSyncParameters(timebase_time_ticks,
                                                   interval_time_delta);
}

void TopLevelDisplayClient::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  // TODO(fsamuel): Implement this.
}

void TopLevelDisplayClient::SetBeginFrameSource(
    cc::SurfaceId surface_id,
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Implement this.
}

}  // namespace mus
