// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "cc/cc_export.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/sync_query_collection.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/latency/latency_info.h"

class SkColorFilter;
class SkNWayCanvas;
class SkPictureRecorder;

namespace gpu {
struct Capabilities;
}

namespace viz {
class DebugBorderDrawQuad;
class PictureDrawQuad;
class SkiaOutputSurface;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;
class YUVVideoDrawQuad;

// TODO(795132): SkColorSpace is only a subset comparing to gfx::ColorSpace.
// Need to figure out support for color space that is not covered by
// SkColorSpace.
class VIZ_SERVICE_EXPORT SkiaRenderer : public DirectRenderer {
 public:
  // Different draw modes that are supported by SkiaRenderer right now.
  enum DrawMode { DDL, SKPRECORD };

  // TODO(penghuang): Remove skia_output_surface when DDL is used everywhere.
  SkiaRenderer(const RendererSettings* settings,
               OutputSurface* output_surface,
               DisplayResourceProvider* resource_provider,
               SkiaOutputSurface* skia_output_surface,
               DrawMode mode);
  ~SkiaRenderer() override;

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info) override;

  void SetDisablePictureQuadImageFiltering(bool disable) {
    disable_picture_quad_image_filtering_ = disable;
  }

 protected:
  bool CanPartialSwap() override;
  void UpdateRenderPassTextures(
      const RenderPassList& render_passes_in_draw_order,
      const base::flat_map<RenderPassId, RenderPassRequirements>&
          render_passes_in_frame) override;
  void AllocateRenderPassResourceIfNeeded(
      const RenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) override;
  bool IsRenderPassResourceAllocated(
      const RenderPassId& render_pass_id) const override;
  gfx::Size GetRenderPassBackingPixelSize(
      const RenderPassId& render_pass_id) override;
  void BindFramebufferToOutputSurface() override;
  void BindFramebufferToTexture(const RenderPassId render_pass_id) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(const DrawQuad* quad, const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  bool FlippedFramebuffer() const override;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyDrawnRenderPass(const copy_output::RenderPassGeometry& geometry,
                           std::unique_ptr<CopyOutputRequest> request) override;
  void SetEnableDCLayers(bool enable) override;
  void DidChangeVisibility() override;
  void FinishDrawingQuadList() override;
  void GenerateMipmap() override;

 private:
  struct DrawQuadParams;
  struct DrawRenderPassDrawQuadParams;
  class ScopedSkImageBuilder;
  class ScopedYUVSkImageBuilder;

  void ClearCanvas(SkColor color);
  void ClearFramebuffer();

  // TODO(michaelludwig):
  // The majority of quad types need to be updated to call the new experimental
  // SkCanvas APIs, which changes what is needed for canvas prep. This is the
  // old implementation of DoDrawQuad and types will be migrated individually
  // to the new system and handled directly in the new DoDrawQuad definition,
  // after which this function can be removed.
  void DoSingleDrawQuad(const DrawQuad* quad, const gfx::QuadF* draw_region);

  void PrepareCanvasForDrawQuads(
      gfx::Transform contents_device_transform,
      const gfx::QuadF* draw_region,
      const gfx::Rect* scissor_rect,
      base::Optional<SkAutoCanvasRestore>* auto_canvas_restore);
  // Callers should init an SkAutoCanvasRestore before calling this function.
  void PrepareCanvas(const gfx::Rect* scissor_rect, const gfx::Transform* cdt);

  DrawQuadParams CalculateDrawQuadParams(const DrawQuad* quad,
                                         const gfx::QuadF* draw_region);
  SkCanvas::ImageSetEntry MakeEntry(const DrawQuadParams& params,
                                    const SkImage* image,
                                    const gfx::RectF& src,
                                    int matrix_index,
                                    bool use_opacity = true);

  bool MustFlushBatchedQuads(const DrawQuad* new_quad,
                             const DrawQuadParams& params);
  void AddQuadToBatch(const DrawQuadParams& params,
                      const SkImage* image,
                      const gfx::RectF& tex_coords);
  void FlushBatchedQuads();

  // Utility to draw a single quad as a filled color
  void DrawColoredQuad(const DrawQuadParams& params, SkColor color);
  // Utility to make a single ImageSetEntry and draw it with the complex paint.
  // Assumes the paint applies the param's opacity.
  void DrawSingleImage(const DrawQuadParams& params,
                       const SkImage* image,
                       const gfx::RectF& src,
                       const SkPaint* paint);

  // DebugBorder, Picture, RPDQ, and SolidColor quads cannot be batched. They
  // either are not textures (debug, picture, solid color), or it's very likely
  // the texture will have advanced paint effects (rpdq)
  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                           const DrawQuadParams& params);
  void DrawPictureQuad(const PictureDrawQuad* quad, SkPaint* paint);
  void DrawRenderPassQuad(const RenderPassDrawQuad* quad, SkPaint* paint);
  void DrawRenderPassQuadInternal(const RenderPassDrawQuad* quad,
                                  sk_sp<SkImage> content_image,
                                  SkPaint* paint);

  void DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                          const DrawQuadParams& state);

  void DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                           const DrawQuadParams& params);
  void DrawTextureQuad(const TextureDrawQuad* quad,
                       const DrawQuadParams& params);
  void DrawTileDrawQuad(const TileDrawQuad* quad, const DrawQuadParams& params);
  void DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                        const DrawQuadParams& params);
  void DrawUnsupportedQuad(const DrawQuad* quad, SkPaint* paint);
  bool CalculateRPDQParams(sk_sp<SkImage> src_image,
                           const RenderPassDrawQuad* quad,
                           DrawRenderPassDrawQuadParams* params);
  bool ShouldApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const cc::FilterOperations* backdrop_filters) const;
  const TileDrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass) override;

  // Get corresponding GrContext. Returns nullptr when there is no GrContext.
  // TODO(weiliangc): This currently only returns nullptr. If SKPRecord isn't
  // going to use this later, it should be removed.
  GrContext* GetGrContext();
  bool is_using_ddl() const { return draw_mode_ == DrawMode::DDL; }

  sk_sp<SkColorFilter> GetColorFilter(const gfx::ColorSpace& src,
                                      const gfx::ColorSpace& dst);
  // A map from RenderPass id to the texture used to draw the RenderPass from.
  struct RenderPassBacking {
    sk_sp<SkSurface> render_pass_surface;
    gfx::Size size;
    bool generate_mipmap;
    gfx::ColorSpace color_space;
    ResourceFormat format;

    // Specific for SkPictureRecorder.
    std::unique_ptr<SkPictureRecorder> recorder;
    sk_sp<SkPicture> picture;

    RenderPassBacking(GrContext* gr_context,
                      const gpu::Capabilities& caps,
                      const gfx::Size& size,
                      bool generate_mipmap,
                      const gfx::ColorSpace& color_space);
    RenderPassBacking(const gfx::Size& size,
                      bool generate_mipmap,
                      const gfx::ColorSpace& color_space);
    ~RenderPassBacking();
    RenderPassBacking(RenderPassBacking&&);
    RenderPassBacking& operator=(RenderPassBacking&&);
  };
  base::flat_map<RenderPassId, RenderPassBacking> render_pass_backings_;

  const DrawMode draw_mode_;

  // Interface used for drawing. Common among different draw modes.
  sk_sp<SkSurface> root_surface_;
  sk_sp<SkSurface> non_root_surface_;
  SkCanvas* root_canvas_ = nullptr;
  SkCanvas* current_canvas_ = nullptr;
  SkSurface* current_surface_ = nullptr;

  bool disable_picture_quad_image_filtering_ = false;
  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  // Specific for overdraw.
  sk_sp<SkSurface> overdraw_surface_;
  std::unique_ptr<SkCanvas> overdraw_canvas_;
  std::unique_ptr<SkNWayCanvas> nway_canvas_;

  // TODO(crbug.com/920344): Use partial swap for SkDDL.
  bool use_swap_with_bounds_ = false;
  gfx::Rect swap_buffer_rect_;
  std::vector<gfx::Rect> swap_content_bounds_;

  // State common to all quads in a batch. Draws that require an SkPaint not
  // captured by this state cannot be batched.
  struct BatchedQuadState {
    gfx::Rect scissor_rect;
    SkBlendMode blend_mode;
    SkFilterQuality filter_quality;
    bool has_scissor_rect;
  };
  BatchedQuadState batched_quad_state_;
  std::vector<SkCanvas::ImageSetEntry> batched_quads_;
  // Same order as batched_quads_, but only includes draw regions for the
  // entries that have fHasClip == true. Each draw region is 4 consecutive pts
  std::vector<SkPoint> batched_draw_regions_;
  // Each entry of batched_quads_ will have an index into this vector; multiple
  // entries may point to the same matrix.
  std::vector<SkMatrix> batched_cdt_matrices_;

  // Specific for SkDDL.
  SkiaOutputSurface* const skia_output_surface_ = nullptr;

  // Lock set for resources that are used for the current frame. All resources
  // in this set will be unlocked with a sync token when the frame is done in
  // the compositor thread. And the sync token will be released when the DDL
  // for the current frame is replayed on the GPU thread.
  // It is only used with DDL.
  base::Optional<DisplayResourceProvider::LockSetForExternalUse>
      lock_set_for_external_use_;

  // Promise images created from resources used in the current frame. This map
  // will be cleared when the frame is done and before all resources in
  // |lock_set_for_external_use_| are unlocked on the compositor thread.
  // It is only used with DDL.
  base::flat_map<ResourceId, sk_sp<SkImage>> promise_images_;
  using YUVIds = std::tuple<ResourceId, ResourceId, ResourceId, ResourceId>;
  base::flat_map<YUVIds, sk_sp<SkImage>> yuv_promise_images_;

  // Specific for SkPRecord.
  std::unique_ptr<SkPictureRecorder> root_recorder_;
  sk_sp<SkPicture> root_picture_;
  sk_sp<SkPicture>* current_picture_;
  SkPictureRecorder* current_recorder_;
  ContextProvider* context_provider_ = nullptr;
  base::Optional<SyncQueryCollection> sync_queries_;

  std::map<gfx::ColorSpace, std::map<gfx::ColorSpace, sk_sp<SkColorFilter>>>
      color_filter_cache_;

  DISALLOW_COPY_AND_ASSIGN(SkiaRenderer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
