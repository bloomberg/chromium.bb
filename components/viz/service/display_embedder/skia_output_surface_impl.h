// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"

namespace base {
class WaitableEvent;
}

namespace viz {

class GpuServiceImpl;
class VizProcessContextProvider;
class SkiaOutputSurfaceImplOnGpu;
class SyntheticBeginFrameSource;

class YUVResourceMetadata;

// The SkiaOutputSurface implementation. It is the output surface for
// SkiaRenderer. It lives on the compositor thread, but it will post tasks
// to the GPU thread for initializing. Currently, SkiaOutputSurfaceImpl
// create a SkiaOutputSurfaceImplOnGpu on the GPU thread. It will be used
// for creating a SkSurface from the default framebuffer and providing the
// SkSurfaceCharacterization for the SkSurface. And then SkiaOutputSurfaceImpl
// will create SkDeferredDisplayListRecorder and SkCanvas for SkiaRenderer to
// render into. In SwapBuffers, it detaches a SkDeferredDisplayList from the
// recorder and plays it back on the framebuffer SkSurface on the GPU thread
// through SkiaOutputSurfaceImpleOnGpu.
class SkiaOutputSurfaceImpl : public SkiaOutputSurface {
 public:
  SkiaOutputSurfaceImpl(
      GpuServiceImpl* gpu_service,
      gpu::SurfaceHandle surface_handle,
      scoped_refptr<VizProcessContextProvider> context_provider,
      SyntheticBeginFrameSource* synthetic_begin_frame_source);
  ~SkiaOutputSurfaceImpl() override;

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

  // SkiaOutputSurface implementation:
  SkCanvas* GetSkCanvasForCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImage(ResourceMetadata metadata) override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      std::vector<ResourceMetadata> metadatas,
      SkYUVColorSpace yuv_color_space) override;
  gpu::SyncToken SkiaSwapBuffers(OutputSurfaceFrame frame) override;
  SkCanvas* BeginPaintRenderPass(const RenderPassId& id,
                                 const gfx::Size& surface_size,
                                 ResourceFormat format,
                                 bool mipmap) override;
  gpu::SyncToken FinishPaintRenderPass() override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(const RenderPassId& id,
                                                  const gfx::Size& size,
                                                  ResourceFormat format,
                                                  bool mipmap) override;
  void RemoveRenderPassResource(std::vector<RenderPassId> ids) override;

 private:
  template <class T>
  class PromiseTextureHelper;
  void InitializeOnGpuThread(base::WaitableEvent* event);
  void RecreateRecorder();
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params);
  void BufferPresented(const gfx::PresentationFeedback& feedback);

  uint64_t sync_fence_release_ = 0;
  GpuServiceImpl* const gpu_service_;
  const gpu::SurfaceHandle surface_handle_;
  SyntheticBeginFrameSource* const synthetic_begin_frame_source_;
  OutputSurfaceClient* client_ = nullptr;

  SkSurfaceCharacterization characterization_;
  std::unique_ptr<SkDeferredDisplayListRecorder> recorder_;

  // The current render pass id set by BeginPaintRenderPass.
  RenderPassId current_render_pass_id_ = 0;

  // The SkDDL recorder created by BeginPaintRenderPass, and
  // FinishPaintRenderPass will turn it into a SkDDL and play the SkDDL back on
  // the GPU thread.
  std::unique_ptr<SkDeferredDisplayListRecorder> offscreen_surface_recorder_;

  // Sync tokens for resources which are used for the current frame.
  std::vector<gpu::SyncToken> resource_sync_tokens_;

  // YUV resource metadatas for the current frame or the current render pass.
  // They should be preprocessed for playing recorded frame into a surface.
  // TODO(penghuang): Remove it when Skia supports drawing YUV textures
  // directly.
  std::vector<YUVResourceMetadata*> yuv_resource_metadatas_;

  // The task runner for running task on the client (compositor) thread.
  scoped_refptr<base::SingleThreadTaskRunner> client_thread_task_runner_;

  // |impl_on_gpu| is created and destroyed on the GPU thread.
  std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
