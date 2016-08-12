// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_DELEGATING_OUTPUT_SURFACE_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_DELEGATING_OUTPUT_SURFACE_H_

#include "cc/output/output_surface.h"
#include "cc/output/renderer_settings.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"

namespace blimp {
namespace client {

class BlimpDelegatingOutputSurface : public cc::OutputSurface,
                                     public cc::SurfaceFactoryClient,
                                     public cc::DisplayClient {
 public:
  BlimpDelegatingOutputSurface(
      scoped_refptr<cc::ContextProvider> compositor_context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      std::unique_ptr<cc::OutputSurface> display_output_surface,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const cc::RendererSettings& renderer_settings,
      base::SingleThreadTaskRunner* task_runner);
  ~BlimpDelegatingOutputSurface() override;

  // OutputSurface implementation.
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void DetachFromClient() override;
  void SwapBuffers(cc::CompositorFrame frame) override;
  void ForceReclaimResources() override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplaySetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const cc::RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;

 private:
  // TODO(danakj): These don't to be stored in unique_ptrs when OutputSurface
  // is owned/destroyed on the compositor thread.
  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  cc::SurfaceId delegated_surface_id_;

  // Uses surface_manager_.
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;

  // Uses surface_manager_.
  std::unique_ptr<cc::Display> display_;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_DELEGATING_OUTPUT_SURFACE_H_
