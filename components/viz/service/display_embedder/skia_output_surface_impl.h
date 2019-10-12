// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/util/type_safety/pass_key.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"

namespace base {
class WaitableEvent;
}

namespace viz {

class ImageContextImpl;
class SkiaOutputSurfaceDependency;
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
class VIZ_SERVICE_EXPORT SkiaOutputSurfaceImpl : public SkiaOutputSurface {
 public:
  static std::unique_ptr<SkiaOutputSurface> Create(
      std::unique_ptr<SkiaOutputSurfaceDependency> deps,
      const RendererSettings& renderer_settings);

  SkiaOutputSurfaceImpl(util::PassKey<SkiaOutputSurfaceImpl> pass_key,
                        std::unique_ptr<SkiaOutputSurfaceDependency> deps,
                        const RendererSettings& renderer_settings);
  ~SkiaOutputSurfaceImpl() override;

  // OutputSurface implementation:
  gpu::SurfaceHandle GetSurfaceHandle() const override;
  void BindToClient(OutputSurfaceClient* client) override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetGpuVSyncCallback(GpuVSyncCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  unsigned UpdateGpuFence() override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
  base::ScopedClosureRunner GetCacheBackBufferCb() override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      const std::vector<ImageContext*>& contexts,
      SkYUVColorSpace yuv_color_space,
      sk_sp<SkColorSpace> dst_color_space,
      bool has_alpha) override;
  gpu::SyncToken SkiaSwapBuffers(OutputSurfaceFrame frame,
                                 bool wants_sync_token) override;
  void ScheduleOutputSurfaceAsOverlay(
      OverlayProcessor::OutputSurfaceOverlayPlane output_surface_plane)
      override;

  SkCanvas* BeginPaintRenderPass(const RenderPassId& id,
                                 const gfx::Size& surface_size,
                                 ResourceFormat format,
                                 bool mipmap,
                                 sk_sp<SkColorSpace> color_space) override;
  gpu::SyncToken SubmitPaint(base::OnceClosure on_finished) override;
  void MakePromiseSkImage(ImageContext* image_context) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const RenderPassId& id,
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) override;

  void RemoveRenderPassResource(std::vector<RenderPassId> ids) override;
  void SetEnableDCLayers(bool enable) override;
  void ScheduleDCLayers(std::vector<DCLayerOverlay> overlays) override;
  void CopyOutput(RenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request) override;
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;

  // ExternalUseClient implementation:
  void ReleaseImageContexts(
      std::vector<std::unique_ptr<ImageContext>> image_contexts) override;
  std::unique_ptr<ExternalUseClient::ImageContext> CreateImageContext(
      const gpu::MailboxHolder& holder,
      const gfx::Size& size,
      ResourceFormat format,
      const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
      sk_sp<SkColorSpace> color_space) override;

  // Set the fields of |capabilities_| and propagates to |impl_on_gpu_|. Should
  // be called after BindToClient().
  void SetCapabilitiesForTesting(bool flipped_output_surface);

  // Used in unit tests.
  void ScheduleGpuTaskForTesting(
      base::OnceClosure callback,
      std::vector<gpu::SyncToken> sync_tokens) override;

  // Wait on the resource sync tokens, and send the promotion hints to
  // the |SharedImage| instances based on the |Mailbox| instances. This should
  // exclude the actual overlay candidate.
  void SendOverlayPromotionNotification(
      std::vector<gpu::SyncToken> sync_tokens,
      base::flat_set<gpu::Mailbox> promotion_denied,
      base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions) override;

  // Notify the overlay candidate to render.
  void RenderToOverlay(gpu::SyncToken sync_token,
                       gpu::Mailbox overlay_candidate_mailbox,
                       const gfx::Rect& bounds) override;

 private:
  bool Initialize();
  void InitializeOnGpuThread(base::WaitableEvent* event, bool* result);
  SkSurfaceCharacterization CreateSkSurfaceCharacterization(
      const gfx::Size& surface_size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space);
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params,
                              const gfx::Size& pixel_size);
  void BufferPresented(const gfx::PresentationFeedback& feedback);

  // Provided as a callback for the GPU thread.
  void OnGpuVSync(base::TimeTicks timebase, base::TimeDelta interval);

  void ScheduleGpuTask(base::OnceClosure callback,
                       std::vector<gpu::SyncToken> sync_tokens);
  GrBackendFormat GetGrBackendFormatForTexture(
      ResourceFormat resource_format,
      uint32_t gl_texture_target,
      const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info);
  void PrepareYUVATextureIndices(const std::vector<ImageContext*>& contexts,
                                 bool has_alpha,
                                 SkYUVAIndex indices[4]);
  void ContextLost();

  void RecreateRootRecorder();

  OutputSurfaceClient* client_ = nullptr;
  bool needs_swap_size_notifications_ = false;

  // Images for current frame or render pass.
  std::vector<ImageContextImpl*> images_in_current_paint_;

  THREAD_CHECKER(thread_checker_);

  // Observers for context lost.
  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  uint64_t sync_fence_release_ = 0;
  std::unique_ptr<SkiaOutputSurfaceDependency> dependency_;
  const bool is_using_vulkan_;
  UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  GpuVSyncCallback gpu_vsync_callback_;
  bool is_displayed_as_overlay_ = false;

  std::unique_ptr<base::WaitableEvent> initialize_waitable_event_;
  SkSurfaceCharacterization characterization_;
  base::Optional<SkDeferredDisplayListRecorder> root_recorder_;

  class ScopedPaint {
   public:
    explicit ScopedPaint(SkDeferredDisplayListRecorder* root_recorder);
    ScopedPaint(SkSurfaceCharacterization characterization,
                RenderPassId render_pass_id);
    ~ScopedPaint();

    SkDeferredDisplayListRecorder* recorder() { return recorder_; }
    RenderPassId render_pass_id() { return render_pass_id_; }

   private:
    // This is recorder being used for current paint
    SkDeferredDisplayListRecorder* recorder_;
    // If we need new recorder for this Paint (i.e it's not root render pass),
    // it's stored here
    base::Optional<SkDeferredDisplayListRecorder> recorder_storage_;
    const RenderPassId render_pass_id_;
  };

  // This holds current paint info
  base::Optional<ScopedPaint> current_paint_;

  // The SkDDL recorder is used for overdraw feedback. It is created by
  // BeginPaintOverdraw, and FinishPaintCurrentFrame will turn it into a SkDDL
  // and play the SkDDL back on the GPU thread.
  base::Optional<SkDeferredDisplayListRecorder> overdraw_surface_recorder_;

  // |overdraw_canvas_| is used to record draw counts.
  base::Optional<SkOverdrawCanvas> overdraw_canvas_;

  // |nway_canvas_| contains |overdraw_canvas_| and root canvas.
  base::Optional<SkNWayCanvas> nway_canvas_;

  // The cache for promise image created from render passes.
  base::flat_map<RenderPassId, std::unique_ptr<ImageContextImpl>>
      render_pass_image_cache_;

  // Sync tokens for resources which are used for the current frame or render
  // pass.
  std::vector<gpu::SyncToken> resource_sync_tokens_;

  const RendererSettings renderer_settings_;

  // The display transform relative to the hardware natural orientation,
  // applied to the frame content. The transform can be rotations in 90 degree
  // increments or flips.
  gfx::OverlayTransform pre_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // |task_sequence| is used to schedule task on GPU as a single
  // sequence. In regular Viz it is implemented by SchedulerSequence. For
  // Android WebView it is implemented on top of WebView's task queue.
  std::unique_ptr<gpu::SingleTaskSequence> task_sequence_;

  // |impl_on_gpu| is created and destroyed on the GPU thread.
  std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;

  base::Optional<gfx::Rect> draw_rectangle_;

  // We defer the draw to the framebuffer until SwapBuffers or CopyOutput
  // to avoid the expense of posting a task and calling MakeCurrent.
  base::OnceCallback<bool()> deferred_framebuffer_draw_closure_;

  base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
