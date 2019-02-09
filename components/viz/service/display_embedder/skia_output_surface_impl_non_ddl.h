// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_NON_DDL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_NON_DDL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"

namespace gl {
class GLSurface;
}

namespace gpu {
class MailboxManager;
class SyncPointClientState;
class SyncPointManager;
class SyncPointOrderData;
}  // namespace gpu

namespace viz {

// A SkiaOutputSurface implementation for running SkiaRenderer on GpuThread.
// Comparing to SkiaOutputSurfaceImpl, it will issue skia draw operations
// against OS graphics API (GL, Vulkan, etc) instead of recording deferred
// display list first.
class VIZ_SERVICE_EXPORT SkiaOutputSurfaceImplNonDDL
    : public SkiaOutputSurface {
 public:
  SkiaOutputSurfaceImplNonDDL(
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::MailboxManager* mailbox_manager,
      gpu::SyncPointManager* sync_point_manager);
  ~SkiaOutputSurfaceImplNonDDL() override;

  // OutputSurface implementation:
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
#if BUILDFLAG(ENABLE_VULKAN)
  gpu::VulkanSurface* GetVulkanSurface() override;
#endif
  unsigned UpdateGpuFence() override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      std::vector<ResourceMetadata> metadatas,
      SkYUVColorSpace yuv_color_space,
      bool has_alpha) override;
  void SkiaSwapBuffers(OutputSurfaceFrame frame) override;
  SkCanvas* BeginPaintRenderPass(const RenderPassId& id,
                                 const gfx::Size& surface_size,
                                 ResourceFormat format,
                                 bool mipmap,
                                 sk_sp<SkColorSpace> color_space) override;
  gpu::SyncToken SubmitPaint() override;
  sk_sp<SkImage> MakePromiseSkImage(ResourceMetadata metadata) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const RenderPassId& id,
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) override;
  gpu::SyncToken ReleasePromiseSkImages(
      std::vector<sk_sp<SkImage>> images) override;

  void RemoveRenderPassResource(std::vector<RenderPassId> ids) override;
  void CopyOutput(RenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request) override;
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;

 private:
  GrContext* gr_context() { return shared_context_state_->gr_context(); }

  bool GetGrBackendTexture(const ResourceMetadata& metadata,
                           GrBackendTexture* backend_texture);

  void ContextLost();

  uint64_t sync_fence_release_ = 0;

  // Stuffs for running with |task_executor_| instead of |gpu_service_|.
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  gpu::MailboxManager* mailbox_manager_;
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  uint32_t order_num_ = 0u;

  OutputSurfaceClient* client_ = nullptr;

  unsigned int backing_framebuffer_object_ = 0;
  gfx::Size reshape_surface_size_;
  float reshape_device_scale_factor_ = 0.f;
  gfx::ColorSpace reshape_color_space_;
  bool reshape_has_alpha_ = false;
  bool reshape_use_stencil_ = false;

  // The current render pass id set by BeginPaintRenderPass.
  RenderPassId current_render_pass_id_ = 0;

  // Observers for context lost.
  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  // The SkSurface for the framebuffer.
  sk_sp<SkSurface> sk_surface_;

  // Offscreen SkSurfaces for render passes.
  base::flat_map<RenderPassId, sk_sp<SkSurface>> offscreen_sk_surfaces_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImplNonDDL);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_NON_DDL_H_
