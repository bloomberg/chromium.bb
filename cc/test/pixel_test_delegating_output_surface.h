// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_DELEGATING_OUTPUT_SURFACE_H_
#define CC_TEST_PIXEL_TEST_DELEGATING_OUTPUT_SURFACE_H_

#include "base/memory/weak_ptr.h"
#include "cc/output/output_surface.h"
#include "cc/output/renderer_settings.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

class PixelTestDelegatingOutputSurface : public OutputSurface,
                                         public SurfaceFactoryClient {
 public:
  PixelTestDelegatingOutputSurface(
      scoped_refptr<ContextProvider> compositor_context_provider,
      scoped_refptr<ContextProvider> worker_context_provider,
      scoped_refptr<ContextProvider> display_context_provider,
      const RendererSettings& renderer_settings,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const gfx::Size& surface_expansion_size,
      bool allow_force_reclaim_resources,
      bool synchronous_composite);
  ~PixelTestDelegatingOutputSurface() override;

  // OutputSurface implementation.
  bool BindToClient(OutputSurfaceClient* client) override;
  void DetachFromClient() override;
  void SwapBuffers(CompositorFrame frame) override;
  void ForceReclaimResources() override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override;

  // Allow tests to enlarge the backing texture for a non-root render pass, to
  // simulate reusing a larger texture from a previous frame for a new
  // render pass. This should be called before the output surface is bound.
  void SetEnlargePassTextureAmount(const gfx::Size& amount);

 private:
  void DrawCallback(SurfaceDrawStatus);

  class PixelTestDisplayClient : public DisplayClient {
    void DisplayOutputSurfaceLost() override {}
    void DisplaySetMemoryPolicy(const ManagedMemoryPolicy& policy) override {}
  };

  SharedBitmapManager* const shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* const gpu_memory_buffer_manager_;
  const gfx::Size surface_expansion_size_;
  const bool allow_force_reclaim_resources_;
  const bool synchronous_composite_;
  const RendererSettings renderer_settings_;

  // Passed to the Display.
  scoped_refptr<ContextProvider> display_context_provider_;

  gfx::Size enlarge_pass_texture_amount_;

  // TODO(danakj): These don't to be stored in unique_ptrs when OutputSurface
  // is owned/destroyed on the compositor thread.
  std::unique_ptr<SurfaceManager> surface_manager_;
  std::unique_ptr<SurfaceIdAllocator> surface_id_allocator_;
  SurfaceId delegated_surface_id_;

  // Uses surface_manager_.
  std::unique_ptr<SurfaceFactory> surface_factory_;

  PixelTestDisplayClient display_client_;
  // Uses surface_manager_.
  std::unique_ptr<Display> display_;

  base::WeakPtrFactory<PixelTestDelegatingOutputSurface> weak_ptrs_;
};

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_DELEGATING_OUTPUT_SURFACE_H_
