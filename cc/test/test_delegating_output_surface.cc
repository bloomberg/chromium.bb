// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_delegating_output_surface.h"

#include <stdint.h>
#include <utility>

#include "cc/output/begin_frame_args.h"
#include "cc/test/begin_frame_args_test.h"

static constexpr uint32_t kCompositorClientId = 1;

namespace cc {

TestDelegatingOutputSurface::TestDelegatingOutputSurface(
    scoped_refptr<ContextProvider> compositor_context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    std::unique_ptr<Display> display,
    bool context_shared_with_compositor,
    bool allow_force_reclaim_resources)
    : OutputSurface(std::move(compositor_context_provider),
                    std::move(worker_context_provider),
                    nullptr),
      surface_manager_(new SurfaceManager),
      surface_id_allocator_(new SurfaceIdAllocator(kCompositorClientId)),
      surface_factory_(new SurfaceFactory(surface_manager_.get(), this)),
      display_(std::move(display)),
      weak_ptrs_(this) {
  CHECK(display_);
  capabilities_.delegated_rendering = true;
  capabilities_.can_force_reclaim_resources = allow_force_reclaim_resources;
  capabilities_.delegated_sync_points_required =
      !context_shared_with_compositor;

  surface_id_allocator_->RegisterSurfaceClientId(surface_manager_.get());
}

TestDelegatingOutputSurface::~TestDelegatingOutputSurface() {}

bool TestDelegatingOutputSurface::BindToClient(OutputSurfaceClient* client) {
  if (!OutputSurface::BindToClient(client))
    return false;

  // We want the Display's output surface to hear about lost context, and since
  // this shares a context with it (when delegated_sync_points_required is
  // false), we should not be listening for lost context callbacks on the
  // context here.
  if (!capabilities_.delegated_sync_points_required && context_provider())
    context_provider()->SetLostContextCallback(base::Closure());

  surface_manager_->RegisterSurfaceFactoryClient(
      surface_id_allocator_->client_id(), this);
  display_->Initialize(&display_client_, surface_manager_.get(),
                       surface_id_allocator_->client_id());
  return true;
}

void TestDelegatingOutputSurface::DetachFromClient() {
  if (!delegated_surface_id_.is_null())
    surface_factory_->Destroy(delegated_surface_id_);
  surface_manager_->UnregisterSurfaceFactoryClient(
      surface_id_allocator_->client_id());

  display_ = nullptr;
  surface_factory_ = nullptr;
  surface_id_allocator_ = nullptr;
  surface_manager_ = nullptr;
  weak_ptrs_.InvalidateWeakPtrs();
  OutputSurface::DetachFromClient();
}

void TestDelegatingOutputSurface::SwapBuffers(CompositorFrame frame) {
  if (delegated_surface_id_.is_null()) {
    delegated_surface_id_ = surface_id_allocator_->GenerateId();
    surface_factory_->Create(delegated_surface_id_);
  }
  display_->SetSurfaceId(delegated_surface_id_,
                         frame.metadata.device_scale_factor);

  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list.back()->output_rect.size();
  display_->Resize(frame_size);

  surface_factory_->SubmitCompositorFrame(
      delegated_surface_id_, std::move(frame),
      base::Bind(&TestDelegatingOutputSurface::DrawCallback,
                 weak_ptrs_.GetWeakPtr()));

  if (!display_->has_scheduler())
    display_->DrawAndSwap();
}

void TestDelegatingOutputSurface::DrawCallback(SurfaceDrawStatus) {
  client_->DidSwapBuffersComplete();
}

void TestDelegatingOutputSurface::ForceReclaimResources() {
  if (capabilities_.can_force_reclaim_resources &&
      !delegated_surface_id_.is_null()) {
    surface_factory_->SubmitCompositorFrame(delegated_surface_id_,
                                            CompositorFrame(),
                                            SurfaceFactory::DrawCallback());
  }
}

void TestDelegatingOutputSurface::BindFramebuffer() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
}

uint32_t TestDelegatingOutputSurface::GetFramebufferCopyTextureFormat() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
  return 0;
}

void TestDelegatingOutputSurface::ReturnResources(
    const ReturnedResourceArray& resources) {
  client_->ReclaimResources(resources);
}

void TestDelegatingOutputSurface::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  client_->SetBeginFrameSource(begin_frame_source);
}

}  // namespace cc
