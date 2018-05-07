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
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace base {
class WaitableEvent;
}

namespace gl {
class GLSurface;
}

namespace gpu {
class SyncPointClientState;
class TextureBase;
}

namespace viz {

class GpuServiceImpl;
class VizProcessContextProvider;
class SyntheticBeginFrameSource;

// The SkiaOutputSurface implementation. It is the output surface for
// SkiaRenderer. It lives on the compositor thread, but it will post tasks
// to the GPU thread for initializing, reshaping and swapping buffers, etc.
// Currently, SkiaOutputSurfaceImpl sets up SkSurface from the default GL
// framebuffer, creates SkDeferredDisplayListRecorder and SkCanvas for
// SkiaRenderer to render into. In SwapBuffers, it detaches a
// SkDeferredDisplayList from the recorder and plays it back on the framebuffer
// SkSurface on the GPU thread.
class SkiaOutputSurfaceImpl : public SkiaOutputSurface,
                              public gpu::ImageTransportSurfaceDelegate {
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

  // gpu::ImageTransportSurfaceDelegate implementation:
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params) override;
  const gpu::gles2::FeatureInfo* GetFeatureInfo() const override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;

  void SetSnapshotRequestedCallback(const base::Closure& callback) override;
  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void BufferPresented(const gfx::PresentationFeedback& feedback) override;

  void AddFilter(IPC::MessageFilter* message_filter) override;
  int32_t GetRouteID() const override;

 private:
  class YUVResourceMetadata;
  void InitializeOnGpuThread(base::WaitableEvent* event);
  void DestroyOnGpuThread(base::WaitableEvent* event);
  void ReshapeOnGpuThread(const gfx::Size& size,
                          float device_scale_factor,
                          const gfx::ColorSpace& color_space,
                          bool has_alpha,
                          bool use_stencil,
                          SkSurfaceCharacterization* characterization,
                          base::WaitableEvent* event);
  void SwapBuffersOnGpuThread(
      OutputSurfaceFrame frame,
      std::unique_ptr<SkDeferredDisplayList> ddl,
      std::vector<YUVResourceMetadata*> yuv_resource_metadatas,
      uint64_t sync_fence_release);
  void FinishPaintRenderPassOnGpuThread(
      RenderPassId id,
      std::unique_ptr<SkDeferredDisplayList> ddl,
      std::vector<YUVResourceMetadata*> yuv_resource_metadatas,
      uint64_t sync_fence_release);
  void RemoveRenderPassResourceOnGpuThread(std::vector<RenderPassId> ids);
  void RecreateRecorder();
  void PreprocessYUVResources(
      std::vector<YUVResourceMetadata*> yuv_resource_metadatas);
  void BindOrCopyTextureIfNecessary(gpu::TextureBase* texture_base);
  void DidSwapBuffersCompleteOnClientThread(
      gpu::SwapBuffersCompleteParams params);
  void UpdateVSyncParametersOnClientThread(base::TimeTicks timebase,
                                           base::TimeDelta interval);
  void BufferPresentedOnClientThread(uint64_t swap_id,
                                     const gfx::PresentationFeedback& feedback);

  template <class T>
  class PromiseTextureHelper;
  // Fullfill callback for promise SkImage created from a resource.
  void OnPromiseTextureFullfill(const ResourceMetadata& metadata,
                                GrBackendTexture* backend_texture);
  // Fullfill callback for promise SkImage created from YUV resources.
  void OnPromiseTextureFullfill(const YUVResourceMetadata& metadata,
                                GrBackendTexture* backend_texture);
  // Fullfill callback for promise SkImage created from a render pass.
  void OnPromiseTextureFullfill(const RenderPassId id,
                                GrBackendTexture* backend_texture);

  // Generage the next swap ID and push it to our pending swap ID queues.
  void OnSwapBuffers();

  const gpu::CommandBufferId command_buffer_id_;
  uint64_t sync_fence_release_ = 0;
  GpuServiceImpl* const gpu_service_;
  const gpu::SurfaceHandle surface_handle_;
  SyntheticBeginFrameSource* const synthetic_begin_frame_source_;
  OutputSurfaceClient* client_ = nullptr;

  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;

  gpu::GpuPreferences gpu_preferences_;
  scoped_refptr<gl::GLSurface> surface_;

  sk_sp<SkSurface> sk_surface_;
  SkSurfaceCharacterization characterization_;
  std::unique_ptr<SkDeferredDisplayListRecorder> recorder_;

  // The current render pass id set by BeginPaintRenderPass.
  RenderPassId current_render_pass_id_ = 0;

  // The SkDDL recorder created by BeginPaintRenderPass, and
  // FinishPaintRenderPass will turn it into a SkDDL and play the SkDDL back on
  // the GPU thread.
  std::unique_ptr<SkDeferredDisplayListRecorder> offscreen_surface_recorder_;

  // Offscreen surfaces for render passes. It must be accessed on the GPU
  // thread.
  base::flat_map<RenderPassId, sk_sp<SkSurface>> offscreen_surfaces_;

  // Sync tokens for resources which are used for the current frame.
  std::vector<gpu::SyncToken> resource_sync_tokens_;

  // YUV resource metadatas for the current frame or the current render pass.
  // They should be preprocessed for playing recorded frame into a surface.
  // TODO(penghuang): Remove it when Skia supports drawing YUV textures
  // directly.
  std::vector<YUVResourceMetadata*> yuv_resource_metadatas_;

  // ID is pushed each time we begin a swap, and popped each time we present or
  // complete a swap.
  base::circular_deque<uint64_t> pending_presented_ids_;
  base::circular_deque<uint64_t> pending_swap_completed_ids_;
  uint64_t swap_id_ = 0;

  // The task runner for running task on the client (compositor) thread.
  scoped_refptr<base::SingleThreadTaskRunner> client_thread_task_runner_;

  THREAD_CHECKER(client_thread_checker_);
  THREAD_CHECKER(gpu_thread_checker_);

  base::WeakPtr<SkiaOutputSurfaceImpl> client_thread_weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImpl> gpu_thread_weak_ptr_factory_;
  base::WeakPtrFactory<SkiaOutputSurfaceImpl> client_thread_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
