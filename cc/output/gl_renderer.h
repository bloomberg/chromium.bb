// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_GL_RENDERER_H_
#define CC_OUTPUT_GL_RENDERER_H_

#include <deque>
#include <unordered_map>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/output/color_lut_cache.h"
#include "cc/output/context_cache_controller.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer_draw_cache.h"
#include "cc/output/program_binding.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/latency/latency_info.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {
class GLRendererShaderTest;
class OutputSurface;
class Resource;
class ResourcePool;
class ScopedResource;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class TextureMailboxDeleter;
class StaticGeometryBinding;
class DynamicGeometryBinding;
struct DrawRenderPassDrawQuadParams;

// Class that handles drawing of composited render layers using GL.
class CC_EXPORT GLRenderer : public DirectRenderer {
 public:
  class ScopedUseGrContext;

  GLRenderer(const RendererSettings* settings,
             OutputSurface* output_surface,
             ResourceProvider* resource_provider,
             TextureMailboxDeleter* texture_mailbox_deleter);
  ~GLRenderer() override;

  bool use_swap_with_bounds() const { return use_swap_with_bounds_; }

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info) override;
  void SwapBuffersComplete() override;

  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;

  virtual bool IsContextLost();

 protected:
  void DidChangeVisibility() override;

  const gfx::QuadF& SharedGeometryQuad() const { return shared_geometry_quad_; }
  const StaticGeometryBinding* SharedGeometry() const {
    return shared_geometry_.get();
  }

  void GetFramebufferPixelsAsync(const gfx::Rect& rect,
                                 std::unique_ptr<CopyOutputRequest> request);
  void GetFramebufferTexture(unsigned texture_id,
                             const gfx::Rect& device_rect);
  void ReleaseRenderPassTextures();
  enum BoundGeometry { NO_BINDING, SHARED_BINDING, CLIPPED_BINDING };
  void PrepareGeometry(BoundGeometry geometry_to_bind);
  void SetStencilEnabled(bool enabled);
  bool stencil_enabled() const { return stencil_shadow_; }
  void SetBlendEnabled(bool enabled);
  bool blend_enabled() const { return blend_shadow_; }

  bool CanPartialSwap() override;
  ResourceFormat BackbufferFormat() const override;
  void BindFramebufferToOutputSurface() override;
  bool BindFramebufferToTexture(const ScopedResource* resource) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(const class DrawQuad*,
                  const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  bool FlippedFramebuffer() const override;
  bool FlippedRootFramebuffer() const;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyCurrentRenderPassToBitmap(
      std::unique_ptr<CopyOutputRequest> request) override;
  void SetEnableDCLayers(bool enable) override;
  void FinishDrawingQuadList() override;

  // Returns true if quad requires antialiasing and false otherwise.
  static bool ShouldAntialiasQuad(const gfx::QuadF& device_layer_quad,
                                  bool clipped,
                                  bool force_aa);

  // Inflate the quad and fill edge array for fragment shader.
  // |local_quad| is set to inflated quad. |edge| array is filled with
  // inflated quad's edge data.
  static void SetupQuadForClippingAndAntialiasing(
      const gfx::Transform& device_transform,
      const DrawQuad* quad,
      const gfx::QuadF* device_layer_quad,
      const gfx::QuadF* clip_region,
      gfx::QuadF* local_quad,
      float edge[24]);
  static void SetupRenderPassQuadForClippingAndAntialiasing(
      const gfx::Transform& device_transform,
      const RenderPassDrawQuad* quad,
      const gfx::QuadF* device_layer_quad,
      const gfx::QuadF* clip_region,
      gfx::QuadF* local_quad,
      float edge[24]);

 private:
  friend class GLRendererShaderPixelTest;
  friend class GLRendererShaderTest;

  // If any of the following functions returns false, then it means that drawing
  // is not possible.
  bool InitializeRPDQParameters(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQShadersForBlending(DrawRenderPassDrawQuadParams* params);
  bool UpdateRPDQWithSkiaFilters(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQTexturesForSampling(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQBlendMode(DrawRenderPassDrawQuadParams* params);
  void ChooseRPDQProgram(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQUniforms(DrawRenderPassDrawQuadParams* params);
  void DrawRPDQ(const DrawRenderPassDrawQuadParams& params);

  static void ToGLMatrix(float* gl_matrix, const gfx::Transform& transform);

  void DiscardPixels();
  void ClearFramebuffer();
  void SetViewport();

  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad);
  static bool IsDefaultBlendMode(SkBlendMode blend_mode) {
    return blend_mode == SkBlendMode::kSrcOver;
  }
  bool CanApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode);
  void ApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode);
  void RestoreBlendFuncToDefault(SkBlendMode blend_mode);

  gfx::Rect GetBackdropBoundingBoxForRenderPassQuad(
      const RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      const FilterOperations* filters,
      const FilterOperations* background_filters,
      const gfx::QuadF* clip_region,
      bool use_aa,
      gfx::Rect* unclipped_rect);
  std::unique_ptr<ScopedResource> GetBackdropTexture(
      const gfx::Rect& bounding_rect);

  static bool ShouldApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const FilterOperations* background_filters);
  sk_sp<SkImage> ApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const FilterOperations& background_filters,
      ScopedResource* background_texture,
      const gfx::RectF& rect,
      const gfx::RectF& unclipped_rect);

  const TileDrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass) override;

  void DrawRenderPassQuad(const RenderPassDrawQuad* quadi,
                          const gfx::QuadF* clip_region);
  void DrawRenderPassQuadInternal(DrawRenderPassDrawQuadParams* params);
  void DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                          const gfx::QuadF* clip_region);
  void DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                           const gfx::QuadF* clip_region);
  void DrawTextureQuad(const TextureDrawQuad* quad,
                       const gfx::QuadF* clip_region);
  void EnqueueTextureQuad(const TextureDrawQuad* quad,
                          const gfx::QuadF* clip_region);
  void FlushTextureQuadCache(BoundGeometry flush_binding);
  void DrawTileQuad(const TileDrawQuad* quad, const gfx::QuadF* clip_region);
  void DrawContentQuad(const ContentDrawQuadBase* quad,
                       ResourceId resource_id,
                       const gfx::QuadF* clip_region);
  void DrawContentQuadAA(const ContentDrawQuadBase* quad,
                         ResourceId resource_id,
                         const gfx::Transform& device_transform,
                         const gfx::QuadF& aa_quad,
                         const gfx::QuadF* clip_region);
  void DrawContentQuadNoAA(const ContentDrawQuadBase* quad,
                           ResourceId resource_id,
                           const gfx::QuadF* clip_region);
  void DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                        const gfx::QuadF* clip_region);

  void SetShaderOpacity(const DrawQuad* quad);
  void SetShaderQuadF(const gfx::QuadF& quad);
  void SetShaderMatrix(const gfx::Transform& transform);
  void SetShaderColor(SkColor color, float opacity);
  void DrawQuadGeometryClippedByQuadF(const gfx::Transform& draw_transform,
                                      const gfx::RectF& quad_rect,
                                      const gfx::QuadF& clipping_region_quad,
                                      const float uv[8]);
  void DrawQuadGeometry(const gfx::Transform& projection_matrix,
                        const gfx::Transform& draw_transform,
                        const gfx::RectF& quad_rect);

  // If |dst_color_space| is invalid, then no color conversion (apart from
  // YUV to RGB conversion) is performed. This explicit argument is available
  // so that video color conversion can be enabled separately from general color
  // conversion.
  // TODO(ccameron): Remove the version with an explicit |dst_color_space|,
  // since that will always be the device color space.
  void SetUseProgram(const ProgramKey& program_key,
                     const gfx::ColorSpace& src_color_space,
                     const gfx::ColorSpace& dst_color_space);
  void SetUseProgram(const ProgramKey& program_key,
                     const gfx::ColorSpace& src_color_space);

  bool MakeContextCurrent();

  void InitializeSharedObjects();
  void CleanupSharedObjects();

  typedef base::Callback<void(std::unique_ptr<CopyOutputRequest> copy_request,
                              bool success)>
      AsyncGetFramebufferPixelsCleanupCallback;
  void FinishedReadback(unsigned source_buffer,
                        unsigned query,
                        const gfx::Size& size);

  void ReinitializeGLState();
  void RestoreGLState();

  void ScheduleCALayers();
  void ScheduleDCLayers();
  void ScheduleOverlays();

  // Copies the contents of the render pass draw quad, including filter effects,
  // to an overlay resource, returned in |resource|. The resource is allocated
  // from |overlay_resource_pool_|.
  // The resulting Resource may be larger than the original quad. The new size
  // and position is placed in |new_bounds|.
  void CopyRenderPassDrawQuadToOverlayResource(
      const CALayerOverlay* ca_layer_overlay,
      Resource** resource,
      gfx::RectF* new_bounds);

  // Schedules the |ca_layer_overlay|, which is guaranteed to have a non-null
  // |rpdq| parameter.
  void ScheduleRenderPassDrawQuad(const CALayerOverlay* ca_layer_overlay);

  // Setup/flush all pending overdraw feedback to framebuffer.
  void SetupOverdrawFeedback();
  void FlushOverdrawFeedback(const gfx::Rect& output_rect);
  // Process overdraw feedback from query.
  using OverdrawFeedbackCallback = base::Callback<void(unsigned, int)>;
  void ProcessOverdrawFeedback(std::vector<int>* overdraw,
                               size_t num_expected_results,
                               int max_result,
                               unsigned query,
                               int multiplier);

  using OverlayResourceLock =
      std::unique_ptr<ResourceProvider::ScopedReadLockGL>;
  using OverlayResourceLockList = std::vector<OverlayResourceLock>;

  // Resources that have been sent to the GPU process, but not yet swapped.
  OverlayResourceLockList pending_overlay_resources_;

  // Resources that should be shortly swapped by the GPU process.
  std::deque<OverlayResourceLockList> swapping_overlay_resources_;

  // Resources that the GPU process has finished swapping. The key is the
  // texture id of the resource.
  std::map<unsigned, OverlayResourceLock> swapped_and_acked_overlay_resources_;

  unsigned offscreen_framebuffer_id_ = 0u;

  std::unique_ptr<StaticGeometryBinding> shared_geometry_;
  std::unique_ptr<DynamicGeometryBinding> clipped_geometry_;
  gfx::QuadF shared_geometry_quad_;

  // This will return nullptr if the requested program has not yet been
  // initialized.
  const Program* GetProgramIfInitialized(const ProgramKey& key) const;

  std::unordered_map<ProgramKey, std::unique_ptr<Program>, ProgramKeyHash>
      program_cache_;

  const gfx::ColorTransform* GetColorTransform(const gfx::ColorSpace& src,
                                               const gfx::ColorSpace& dst);
  std::map<gfx::ColorSpace,
           std::map<gfx::ColorSpace, std::unique_ptr<gfx::ColorTransform>>>
      color_transform_cache_;

  gpu::gles2::GLES2Interface* gl_;
  gpu::ContextSupport* context_support_;
  std::unique_ptr<ContextCacheController::ScopedVisibility> context_visibility_;

  TextureMailboxDeleter* texture_mailbox_deleter_;

  gfx::Rect swap_buffer_rect_;
  std::vector<gfx::Rect> swap_content_bounds_;
  gfx::Rect scissor_rect_;
  bool is_scissor_enabled_ = false;
  bool stencil_shadow_ = false;
  bool blend_shadow_ = false;
  const Program* current_program_ = nullptr;
  TexturedQuadDrawCache draw_cache_;
  int highp_threshold_cache_ = 0;

  struct PendingAsyncReadPixels;
  std::vector<std::unique_ptr<PendingAsyncReadPixels>>
      pending_async_read_pixels_;

  std::unique_ptr<ResourceProvider::ScopedWriteLockGL>
      current_framebuffer_lock_;
  // This is valid when current_framebuffer_lock_ is not null.
  ResourceFormat current_framebuffer_format_;

  class SyncQuery;
  std::deque<std::unique_ptr<SyncQuery>> pending_sync_queries_;
  std::deque<std::unique_ptr<SyncQuery>> available_sync_queries_;
  std::unique_ptr<SyncQuery> current_sync_query_;
  bool use_discard_framebuffer_ = false;
  bool use_sync_query_ = false;
  bool use_blend_equation_advanced_ = false;
  bool use_blend_equation_advanced_coherent_ = false;
  bool use_occlusion_query_ = false;
  bool use_swap_with_bounds_ = false;

  // Some overlays require that content is copied from a render pass into an
  // overlay resource. This means the GLRenderer needs its own ResourcePool.
  std::unique_ptr<ResourcePool> overlay_resource_pool_;

  // If true, draw a green border after compositing a texture quad using GL.
  bool gl_composited_texture_quad_border_;

  // The method FlippedFramebuffer determines whether the framebuffer associated
  // with a DrawingFrame is flipped. It makes the assumption that the
  // DrawingFrame is being used as part of a render pass. If a DrawingFrame is
  // not being used as part of a render pass, setting it here forces
  // FlippedFramebuffer to return |true|.
  bool force_drawing_frame_framebuffer_unflipped_ = false;

  BoundGeometry bound_geometry_;
  ColorLUTCache color_lut_cache_;

  unsigned offscreen_stencil_renderbuffer_id_ = 0;
  gfx::Size offscreen_stencil_renderbuffer_size_;

  unsigned num_triangles_drawn_ = 0;

  base::WeakPtrFactory<GLRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_GL_RENDERER_H_
