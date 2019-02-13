// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_metadata.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/latency/latency_tracker.h"

class SkDeferredDisplayList;

namespace base {
class WaitableEvent;
}

namespace gfx {
class ColorSpace;
}

namespace gl {
class GLApi;
class GLSurface;
}

namespace gpu {
class CommandBufferTaskExecutor;
class SyncPointClientState;
class SharedImageRepresentationSkia;
}

namespace ui {
#if defined(USE_OZONE)
class PlatformWindowSurface;
#endif
}  // namespace ui

namespace viz {

class DirectContextProvider;
class GLRendererCopier;
class GpuServiceImpl;
class TextureDeleter;
class VulkanContextProvider;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// The SkiaOutputSurface implementation running on the GPU thread. This class
// should be created, used and destroyed on the GPU thread.
class SkiaOutputSurfaceImplOnGpu : public gpu::ImageTransportSurfaceDelegate {
 public:
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size)>;
  using BufferPresentedCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback& feedback)>;
  using ContextLostCallback = base::RepeatingCallback<void()>;

  SkiaOutputSurfaceImplOnGpu(
      gpu::SurfaceHandle surface_handle,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MailboxManager* mailbox_manager,
      scoped_refptr<gpu::SyncPointClientState> sync_point_client_data,
      std::unique_ptr<gpu::SharedImageRepresentationFactory> sir_factory,
      gpu::raster::GrShaderCache* gr_shader_cache,
      VulkanContextProvider* vulkan_context_provider,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      const BufferPresentedCallback& buffer_presented_callback,
      const ContextLostCallback& context_lost_callback);
  SkiaOutputSurfaceImplOnGpu(
      GpuServiceImpl* gpu_service,
      gpu::SurfaceHandle surface_handle,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      const BufferPresentedCallback& buffer_presented_callback,
      const ContextLostCallback& context_lost_callback);
  SkiaOutputSurfaceImplOnGpu(
      gpu::CommandBufferTaskExecutor* task_executor,
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::SequenceId sequence_id,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      const BufferPresentedCallback& buffer_presented_callback,
      const ContextLostCallback& context_lost_callback);

  ~SkiaOutputSurfaceImplOnGpu() override;

  gpu::CommandBufferId command_buffer_id() const {
    return sync_point_client_state_->command_buffer_id();
  }
  const OutputSurface::Capabilities capabilities() const {
    return capabilities_;
  }
  const base::WeakPtr<SkiaOutputSurfaceImplOnGpu>& weak_ptr() const {
    return weak_ptr_;
  }

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil,
               SkSurfaceCharacterization* characterization,
               base::WaitableEvent* event);
  void FinishPaintCurrentFrame(
      std::unique_ptr<SkDeferredDisplayList> ddl,
      std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
      std::vector<gpu::SyncToken> sync_tokens,
      uint64_t sync_fence_release);
  void SwapBuffers(OutputSurfaceFrame frame);
  void FinishPaintRenderPass(RenderPassId id,
                             std::unique_ptr<SkDeferredDisplayList> ddl,
                             std::vector<gpu::SyncToken> sync_tokens,
                             uint64_t sync_fence_release);
  void RemoveRenderPassResource(std::vector<RenderPassId> ids);
  void CopyOutput(RenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request);

  // Fulfill callback for promise SkImage created from a resource.
  sk_sp<SkPromiseImageTexture> FulfillPromiseTexture(
      const gpu::MailboxHolder& mailbox_holder,
      const gfx::Size& size,
      const ResourceFormat resource_format,
      std::unique_ptr<gpu::SharedImageRepresentationSkia>* shared_image_out);
  // Fulfill callback for promise SkImage created from a render pass.
  // |shared_image_out| is ignored for render passes, as these aren't based on
  // SharedImage.
  sk_sp<SkPromiseImageTexture> FulfillPromiseTexture(
      const RenderPassId id,
      std::unique_ptr<gpu::SharedImageRepresentationSkia>* shared_image_out);

  sk_sp<GrContextThreadSafeProxy> GetGrContextThreadSafeProxy();
  const gl::GLVersionInfo* gl_version_info() const { return gl_version_info_; }

  void DestroySkImages(std::vector<sk_sp<SkImage>>&& images,
                       uint64_t sync_fence_release);

  bool was_context_lost() { return context_state_->context_lost(); }

  class ScopedUseContextProvider;
  class SurfaceWrapper;

 private:
// gpu::ImageTransportSurfaceDelegate implementation:
#if defined(OS_WIN)
  void DidCreateAcceleratedSurfaceChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params) override;
  const gpu::gles2::FeatureInfo* GetFeatureInfo() const override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  void BufferPresented(const gfx::PresentationFeedback& feedback) override;
  void AddFilter(IPC::MessageFilter* message_filter) override;
  int32_t GetRouteID() const override;

  void InitializeForGL();
  void InitializeForGLWithGpuService(GpuServiceImpl* gpu_service);
  void InitializeForGLWithTaskExecutor(
      gpu::CommandBufferTaskExecutor* task_executor,
      scoped_refptr<gl::GLSurface> gl_surface);
  void InitializeForVulkan(GpuServiceImpl* gpu_service);

  void BindOrCopyTextureIfNecessary(gpu::TextureBase* texture_base);

  // Generage the next swap ID and push it to our pending swap ID queues.
  void OnSwapBuffers();

  void CreateSkSurfaceForGL();

  // Make context current for GL, and return false if the context is lost.
  // It will do nothing when Vulkan is used.
  bool MakeCurrent();

  void PullTextureUpdates(std::vector<gpu::SyncToken> sync_token);

  void ReleaseFenceSyncAndPushTextureUpdates(uint64_t sync_fence_release);

  GrContext* gr_context() { return context_state_->gr_context(); }
  gpu::DecoderContext* decoder();

  void ScheduleDelayedWork();
  void PerformDelayedWork();

  bool is_using_vulkan() const { return !!vulkan_context_provider_; }

  const gpu::SurfaceHandle surface_handle_;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  gpu::MailboxManager* const mailbox_manager_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  gpu::raster::GrShaderCache* const gr_shader_cache_;
  VulkanContextProvider* const vulkan_context_provider_;
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;
  BufferPresentedCallback buffer_presented_callback_;
  ContextLostCallback context_lost_callback_;

#if defined(USE_OZONE)
  // This should outlive gl_surface_ and vulkan_surface_.
  std::unique_ptr<ui::PlatformWindowSurface> window_surface_;
#endif

  gpu::GpuPreferences gpu_preferences_;
  gfx::Size size_;
  gfx::ColorSpace color_space_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  sk_sp<SkSurface> sk_surface_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  const gl::GLVersionInfo* gl_version_info_ = nullptr;
  OutputSurface::Capabilities capabilities_;

  std::unique_ptr<SurfaceWrapper> vulkan_surface_;

  // Offscreen surfaces for render passes. It can only be accessed on GPU
  // thread.
  struct OffscreenSurface {
    OffscreenSurface();
    OffscreenSurface(const OffscreenSurface& offscreen_surface);
    OffscreenSurface(OffscreenSurface&& offscreen_surface);
    OffscreenSurface& operator=(const OffscreenSurface& offscreen_surface);
    OffscreenSurface& operator=(OffscreenSurface&& offscreen_surface);
    ~OffscreenSurface();
    sk_sp<SkSurface> surface;
    sk_sp<SkPromiseImageTexture> promise_texture;
  };
  base::flat_map<RenderPassId, OffscreenSurface> offscreen_surfaces_;

  // Params are pushed each time we begin a swap, and popped each time we
  // present or complete a swap.
  base::circular_deque<std::pair<uint64_t, gfx::Size>>
      pending_swap_completed_params_;
  uint64_t swap_id_ = 0;

  ui::LatencyTracker latency_tracker_;

  scoped_refptr<base::SingleThreadTaskRunner> context_current_task_runner_;
  scoped_refptr<DirectContextProvider> context_provider_;
  std::unique_ptr<TextureDeleter> texture_deleter_;
  std::unique_ptr<GLRendererCopier> copier_;

  bool delayed_work_pending_ = false;

  gl::GLApi* api_ = nullptr;
  bool supports_alpha_ = false;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImplOnGpu> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImplOnGpu);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
