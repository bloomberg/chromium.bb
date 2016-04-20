// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/top_level_display_client.h"

#include "base/memory/ptr_util.h"
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

#if defined(USE_OZONE)
#include "components/mus/surfaces/direct_output_surface_ozone.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#endif

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
  surfaces_state_->manager()->RegisterSurfaceIdNamespace(cc_id_.id_namespace());

  display_.reset(new cc::Display(this, surfaces_state_->manager(), nullptr,
                                 nullptr, cc::RendererSettings(),
                                 cc_id_.id_namespace()));

  scoped_refptr<SurfacesContextProvider> surfaces_context_provider(
      new SurfacesContextProvider(widget, gpu_state));
  // TODO(rjkroege): If there is something better to do than CHECK, add it.
  CHECK(surfaces_context_provider->BindToCurrentThread());

  std::unique_ptr<cc::OutputSurface> output_surface;
  if (surfaces_context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
    output_surface = base::WrapUnique(new DirectOutputSurfaceOzone(
        surfaces_context_provider, widget, task_runner_.get(), GL_TEXTURE_2D,
        GL_RGB));
#else
    NOTREACHED();
#endif
  } else {
    output_surface = base::WrapUnique(
        new DirectOutputSurface(surfaces_context_provider, task_runner_.get()));
  }

  int max_frames_pending = output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  if (gpu_state->HardwareRenderingAvailable()) {
    display_->Initialize(std::move(output_surface), task_runner_.get());
  } else {
    // TODO(rjkroege): Implement software compositing.
  }
  display_->Resize(last_submitted_frame_size_);

  // TODO(fsamuel): Plumb the proper device scale factor.
  display_->SetSurfaceId(cc_id_, 1.f /* device_scale_factor */);
}

TopLevelDisplayClient::~TopLevelDisplayClient() {
  surfaces_state_->manager()->InvalidateSurfaceIdNamespace(
      cc_id_.id_namespace());
  factory_.Destroy(cc_id_);
}

void TopLevelDisplayClient::SubmitCompositorFrame(
    std::unique_ptr<cc::CompositorFrame> frame,
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
    std::unique_ptr<cc::CopyOutputRequest> output_request) {
  factory_.RequestCopyOfSurface(cc_id_, std::move(output_request));
}

void TopLevelDisplayClient::OutputSurfaceLost() {
  if (!display_)  // Shutdown case
    return;
  display_.reset();
}

void TopLevelDisplayClient::SetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {}

void TopLevelDisplayClient::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  // TODO(fsamuel): Implement this.
}

void TopLevelDisplayClient::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Implement this.
}

}  // namespace mus
