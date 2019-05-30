// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display_embedder/skia_output_surface_base.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"

namespace base {
class WaitableEvent;
}

namespace viz {

class GpuServiceImpl;
class SkiaOutputSurfaceImplOnGpu;

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
class VIZ_SERVICE_EXPORT SkiaOutputSurfaceImpl : public SkiaOutputSurfaceBase {
 public:
  SkiaOutputSurfaceImpl(GpuServiceImpl* gpu_service,
                        gpu::SurfaceHandle surface_handle,
                        const RendererSettings& renderer_settings);
  ~SkiaOutputSurfaceImpl() override;

  // OutputSurface implementation:
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      const std::vector<ResourceMetadata>& metadatas,
      SkYUVColorSpace yuv_color_space,
      sk_sp<SkColorSpace> dst_color_space,
      bool has_alpha) override;
  void SkiaSwapBuffers(OutputSurfaceFrame frame) override;
  SkCanvas* BeginPaintRenderPass(const RenderPassId& id,
                                 const gfx::Size& surface_size,
                                 ResourceFormat format,
                                 bool mipmap,
                                 sk_sp<SkColorSpace> color_space) override;
  gpu::SyncToken SubmitPaint(base::OnceClosure on_finished) override;
  sk_sp<SkImage> MakePromiseSkImage(const ResourceMetadata& metadata) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const RenderPassId& id,
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) override;

  void RemoveRenderPassResource(std::vector<RenderPassId> ids) override;
  void CopyOutput(RenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request) override;

  // ExternalUseClient implementation:
  void ReleaseCachedResources(const std::vector<ResourceId>& ids) override;

 protected:
  // Set the fields of |capabilities_| and propagates to |impl_on_gpu_|. Should
  // be called after BindToClient().
  void SetCapabilitiesForTesting(bool flipped_output_surface);

 private:
  void InitializeOnGpuThread(base::WaitableEvent* event);
  SkSurfaceCharacterization CreateSkSurfaceCharacterization(
      const gfx::Size& surface_size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space);
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params,
                              const gfx::Size& pixel_size);
  void BufferPresented(const gfx::PresentationFeedback& feedback);
  void ScheduleGpuTask(base::OnceClosure callback,
                       std::vector<gpu::SyncToken> sync_tokens);
  GrBackendFormat GetGrBackendFormatForTexture(
      ResourceFormat resource_format,
      uint32_t gl_texture_target,
      base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info = base::nullopt);

  uint64_t sync_fence_release_ = 0;
  GpuServiceImpl* const gpu_service_;
  const bool is_using_vulkan_;
  const gpu::SurfaceHandle surface_handle_;
  UpdateVSyncParametersCallback update_vsync_parameters_callback_;

  std::unique_ptr<base::WaitableEvent> initialize_waitable_event_;
  SkSurfaceCharacterization characterization_;
  base::Optional<SkDeferredDisplayListRecorder> recorder_;

  // The current render pass id set by BeginPaintRenderPass.
  RenderPassId current_render_pass_id_ = 0;

  // The SkDDL recorder is used for overdraw feedback. It is created by
  // BeginPaintOverdraw, and FinishPaintCurrentFrame will turn it into a SkDDL
  // and play the SkDDL back on the GPU thread.
  base::Optional<SkDeferredDisplayListRecorder> overdraw_surface_recorder_;

  // |overdraw_canvas_| is used to record draw counts.
  base::Optional<SkOverdrawCanvas> overdraw_canvas_;

  // |nway_canvas_| contains |overdraw_canvas_| and root canvas.
  base::Optional<SkNWayCanvas> nway_canvas_;

  // The cache for promise image created from render passes.
  base::flat_map<RenderPassId, std::unique_ptr<ImageContext>>
      render_pass_image_cache_;

  // Sync tokens for resources which are used for the current frame or render
  // pass.
  std::vector<gpu::SyncToken> resource_sync_tokens_;

  // The task runner for running task on the client (compositor) thread.
  scoped_refptr<base::SingleThreadTaskRunner> client_thread_task_runner_;

  const RendererSettings renderer_settings_;

  // |impl_on_gpu| is created and destroyed on the GPU thread.
  std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;

  base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
