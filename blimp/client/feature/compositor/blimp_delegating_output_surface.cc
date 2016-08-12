// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_delegating_output_surface.h"

#include <stdint.h>
#include <utility>

#include "cc/output/texture_mailbox_deleter.h"

static constexpr uint32_t kCompositorClientId = 1;

namespace blimp {
namespace client {

BlimpDelegatingOutputSurface::BlimpDelegatingOutputSurface(
    scoped_refptr<cc::ContextProvider> compositor_context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    std::unique_ptr<cc::OutputSurface> display_output_surface,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const cc::RendererSettings& renderer_settings,
    base::SingleThreadTaskRunner* task_runner)
    : cc::OutputSurface(std::move(compositor_context_provider),
                        std::move(worker_context_provider),
                        nullptr),
      surface_manager_(new cc::SurfaceManager),
      surface_id_allocator_(new cc::SurfaceIdAllocator(kCompositorClientId)),
      surface_factory_(new cc::SurfaceFactory(surface_manager_.get(), this)) {
  std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner)));
  std::unique_ptr<cc::DisplayScheduler> scheduler(new cc::DisplayScheduler(
      begin_frame_source.get(), task_runner,
      display_output_surface->capabilities().max_frames_pending));
  display_.reset(new cc::Display(
      nullptr /*shared_bitmap_manager*/, gpu_memory_buffer_manager,
      renderer_settings, std::move(begin_frame_source),
      std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner)));

  capabilities_.delegated_rendering = true;
}

BlimpDelegatingOutputSurface::~BlimpDelegatingOutputSurface() = default;

bool BlimpDelegatingOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  bool bound = cc::OutputSurface::BindToClient(client);
  DCHECK(bound);  // No support for failing to set up the context.

  surface_manager_->RegisterSurfaceClientId(surface_id_allocator_->client_id());
  surface_manager_->RegisterSurfaceFactoryClient(
      surface_id_allocator_->client_id(), this);
  display_->Initialize(this, surface_manager_.get(),
                       surface_id_allocator_->client_id());
  // TODO(danakj): Do this when https://codereview.chromium.org/2238693002/ .
  // display_->SetVisible(true);
  return true;
}

void BlimpDelegatingOutputSurface::DetachFromClient() {
  if (!delegated_surface_id_.is_null())
    surface_factory_->Destroy(delegated_surface_id_);
  surface_manager_->UnregisterSurfaceFactoryClient(
      surface_id_allocator_->client_id());
  surface_manager_->InvalidateSurfaceClientId(
      surface_id_allocator_->client_id());
  display_ = nullptr;
  surface_factory_ = nullptr;
  surface_id_allocator_ = nullptr;
  surface_manager_ = nullptr;
  cc::OutputSurface::DetachFromClient();
}

void BlimpDelegatingOutputSurface::SwapBuffers(cc::CompositorFrame frame) {
  if (delegated_surface_id_.is_null()) {
    delegated_surface_id_ = surface_id_allocator_->GenerateId();
    surface_factory_->Create(delegated_surface_id_);
  }
  display_->SetSurfaceId(delegated_surface_id_,
                         frame.metadata.device_scale_factor);

  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list.back()->output_rect.size();
  display_->Resize(frame_size);

  surface_factory_->SubmitCompositorFrame(delegated_surface_id_,
                                          std::move(frame), base::Closure());
  cc::OutputSurface::PostSwapBuffersComplete();
}

void BlimpDelegatingOutputSurface::ForceReclaimResources() {}

void BlimpDelegatingOutputSurface::BindFramebuffer() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
}

uint32_t BlimpDelegatingOutputSurface::GetFramebufferCopyTextureFormat() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
  return 0;
}

void BlimpDelegatingOutputSurface::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  client_->ReclaimResources(resources);
}

void BlimpDelegatingOutputSurface::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  client_->SetBeginFrameSource(begin_frame_source);
}

void BlimpDelegatingOutputSurface::DisplayOutputSurfaceLost() {
  DidLoseOutputSurface();
}

void BlimpDelegatingOutputSurface::DisplaySetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {
  SetMemoryPolicy(policy);
}

void BlimpDelegatingOutputSurface::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const cc::RenderPassList& render_passes) {}

void BlimpDelegatingOutputSurface::DisplayDidDrawAndSwap() {}

}  // namespace client
}  // namespace blimp
