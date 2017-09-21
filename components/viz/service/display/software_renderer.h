// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_RENDERER_H_

#include "base/macros.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/latency/latency_info.h"

namespace cc {
class OutputSurface;
class DisplayResourceProvider;
}  // namespace cc

namespace viz {
class DebugBorderDrawQuad;
class PictureDrawQuad;
class RenderPassDrawQuad;
class SoftwareOutputDevice;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;

class VIZ_SERVICE_EXPORT SoftwareRenderer : public DirectRenderer {
 public:
  SoftwareRenderer(const RendererSettings* settings,
                   OutputSurface* output_surface,
                   cc::DisplayResourceProvider* resource_provider);

  ~SoftwareRenderer() override;

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
  void GenerateMipmap() override;

 private:
  void ClearCanvas(SkColor color);
  void ClearFramebuffer();
  void SetClipRect(const gfx::Rect& rect);
  bool IsSoftwareResource(ResourceId resource_id) const;

  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad);
  void DrawPictureQuad(const PictureDrawQuad* quad);
  void DrawRenderPassQuad(const RenderPassDrawQuad* quad);
  void DrawSolidColorQuad(const SolidColorDrawQuad* quad);
  void DrawTextureQuad(const TextureDrawQuad* quad);
  void DrawTileQuad(const TileDrawQuad* quad);
  void DrawUnsupportedQuad(const DrawQuad* quad);
  bool ShouldApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const cc::FilterOperations* background_filters) const;
  sk_sp<SkImage> ApplyImageFilter(SkImageFilter* filter,
                                  const RenderPassDrawQuad* quad,
                                  const SkBitmap& to_filter,
                                  SkIRect* auto_bounds) const;
  gfx::Rect GetBackdropBoundingBoxForRenderPassQuad(
      const RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      const cc::FilterOperations* background_filters,
      gfx::Rect* unclipped_rect) const;
  SkBitmap GetBackdropBitmap(const gfx::Rect& bounding_rect) const;
  sk_sp<SkShader> GetBackgroundFilterShader(
      const RenderPassDrawQuad* quad,
      SkShader::TileMode content_tile_mode) const;

  bool disable_picture_quad_image_filtering_ = false;

  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  SoftwareOutputDevice* output_device_;
  SkCanvas* root_canvas_ = nullptr;
  SkCanvas* current_canvas_ = nullptr;
  SkPaint current_paint_;
  std::unique_ptr<cc::ResourceProvider::ScopedWriteLockSoftware>
      current_framebuffer_lock_;
  std::unique_ptr<SkCanvas> current_framebuffer_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareRenderer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_RENDERER_H_
