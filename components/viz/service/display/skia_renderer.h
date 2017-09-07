// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_

#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/output/direct_renderer.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/latency/latency_info.h"

class SkNWayCanvas;

namespace cc {
class DebugBorderDrawQuad;
class OutputSurface;
class PictureDrawQuad;
class RenderPassDrawQuad;
class ResourceProvider;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;
}  // namespace cc

namespace viz {
class VIZ_SERVICE_EXPORT SkiaRenderer : public cc::DirectRenderer {
 public:
  SkiaRenderer(const RendererSettings* settings,
               cc::OutputSurface* output_surface,
               cc::DisplayResourceProvider* resource_provider);

  ~SkiaRenderer() override;

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info) override;

  void SetDisablePictureQuadImageFiltering(bool disable) {
    disable_picture_quad_image_filtering_ = disable;
  }

 protected:
  bool CanPartialSwap() override;
  ResourceFormat BackbufferFormat() const override;
  void BindFramebufferToOutputSurface() override;
  bool BindFramebufferToTexture(const cc::ScopedResource* texture) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(const DrawQuad* quad, const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  bool FlippedFramebuffer() const override;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyDrawnRenderPass(std::unique_ptr<CopyOutputRequest> request) override;
  void SetEnableDCLayers(bool enable) override;
  void DidChangeVisibility() override;
  void FinishDrawingQuadList() override;

 private:
  void ClearCanvas(SkColor color);
  void ClearFramebuffer();
  void SetClipRect(const gfx::Rect& rect);
  bool IsSoftwareResource(ResourceId resource_id) const;

  void DrawDebugBorderQuad(const cc::DebugBorderDrawQuad* quad);
  void DrawPictureQuad(const cc::PictureDrawQuad* quad);
  void DrawRenderPassQuad(const cc::RenderPassDrawQuad* quad);
  void DrawSolidColorQuad(const cc::SolidColorDrawQuad* quad);
  void DrawTextureQuad(const cc::TextureDrawQuad* quad);
  void DrawTileQuad(const cc::TileDrawQuad* quad);
  void DrawUnsupportedQuad(const DrawQuad* quad);
  bool ShouldApplyBackgroundFilters(
      const cc::RenderPassDrawQuad* quad,
      const cc::FilterOperations* background_filters) const;
  sk_sp<SkImage> ApplyImageFilter(SkImageFilter* filter,
                                  const cc::RenderPassDrawQuad* quad,
                                  const SkBitmap& to_filter,
                                  SkIRect* auto_bounds) const;
  gfx::Rect GetBackdropBoundingBoxForRenderPassQuad(
      const cc::RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      const cc::FilterOperations* background_filters) const;
  SkBitmap GetBackdropBitmap(const gfx::Rect& bounding_rect) const;
  sk_sp<SkShader> GetBackgroundFilterShader(
      const cc::RenderPassDrawQuad* quad,
      SkShader::TileMode content_tile_mode) const;

  bool disable_picture_quad_image_filtering_ = false;

  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  sk_sp<SkSurface> root_surface_;
  sk_sp<SkSurface> overdraw_surface_;
  std::unique_ptr<SkCanvas> overdraw_canvas_;
  std::unique_ptr<SkNWayCanvas> nway_canvas_;
  SkCanvas* root_canvas_ = nullptr;
  SkCanvas* current_canvas_ = nullptr;
  SkPaint current_paint_;
  std::unique_ptr<cc::ResourceProvider::ScopedWriteLockGL>
      current_framebuffer_lock_;
  std::unique_ptr<cc::ResourceProvider::ScopedSkSurface>
      current_framebuffer_surface_lock_;

  bool use_swap_with_bounds_ = false;

  gfx::Rect swap_buffer_rect_;
  std::vector<gfx::Rect> swap_content_bounds_;

  DISALLOW_COPY_AND_ASSIGN(SkiaRenderer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
