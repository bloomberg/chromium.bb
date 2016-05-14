// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/display_compositor.h"

#include "cc/output/copy_output_request.h"
#include "cc/output/output_surface.h"
#include "cc/output/renderer_settings.h"
#include "cc/surfaces/display.h"
#include "components/mus/surfaces/direct_output_surface.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "components/mus/surfaces/top_level_display_client.h"

#if defined(USE_OZONE)
#include "components/mus/surfaces/direct_output_surface_ozone.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#endif

namespace mus {

DisplayCompositor::DisplayCompositor(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gfx::AcceleratedWidget widget,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : task_runner_(task_runner),
      surfaces_state_(surfaces_state),
      factory_(surfaces_state->manager(), this),
      allocator_(surfaces_state->next_id_namespace()) {
  allocator_.RegisterSurfaceIdNamespace(surfaces_state_->manager());
  surfaces_state_->manager()->RegisterSurfaceFactoryClient(
      allocator_.id_namespace(), this);

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

  display_client_.reset(new TopLevelDisplayClient(
      std::move(output_surface), surfaces_state_->manager(),
      nullptr /* bitmap_manager */, nullptr /* gpu_memory_buffer_manager */,
      cc::RendererSettings(), task_runner_, allocator_.id_namespace()));

  display_client_->Initialize();
}

DisplayCompositor::~DisplayCompositor() {
  surfaces_state_->manager()->UnregisterSurfaceFactoryClient(
      allocator_.id_namespace());
}

void DisplayCompositor::SubmitCompositorFrame(
    std::unique_ptr<cc::CompositorFrame> frame,
    const base::Callback<void(cc::SurfaceDrawStatus)>& callback) {
  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size.IsEmpty() || frame_size != display_size_) {
    if (!surface_id_.is_null())
      factory_.Destroy(surface_id_);
    surface_id_ = allocator_.GenerateId();
    factory_.Create(surface_id_);
    display_size_ = frame_size;
    display_client_->display()->Resize(display_size_);
  }
  display_client_->display()->SetSurfaceId(surface_id_,
                                           frame->metadata.device_scale_factor);
  factory_.SubmitCompositorFrame(surface_id_, std::move(frame), callback);
}

void DisplayCompositor::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> output_request) {
  factory_.RequestCopyOfSurface(surface_id_, std::move(output_request));
}

void DisplayCompositor::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  // TODO(fsamuel): Implement this.
}

void DisplayCompositor::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(fsamuel): Implement this.
}

}  // namespace mus
