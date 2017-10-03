// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/base/render_surface_filters.h"
#include "cc/resources/scoped_resource.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkLayerRasterizer.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace viz {
namespace {

static inline bool IsScalarNearlyInteger(SkScalar scalar) {
  return SkScalarNearlyZero(scalar - SkScalarRoundToScalar(scalar));
}

bool IsScaleAndIntegerTranslate(const SkMatrix& matrix) {
  return IsScalarNearlyInteger(matrix[SkMatrix::kMTransX]) &&
         IsScalarNearlyInteger(matrix[SkMatrix::kMTransY]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMSkewX]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMSkewY]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp0]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp1]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp2] - 1.0f);
}

}  // anonymous namespace

SkiaRenderer::SkiaRenderer(const RendererSettings* settings,
                           OutputSurface* output_surface,
                           cc::DisplayResourceProvider* resource_provider)
    : DirectRenderer(settings, output_surface, resource_provider) {
  const auto& context_caps =
      output_surface_->context_provider()->ContextCapabilities();
  use_swap_with_bounds_ = context_caps.swap_buffers_with_bounds;
}

SkiaRenderer::~SkiaRenderer() {}

bool SkiaRenderer::CanPartialSwap() {
  if (use_swap_with_bounds_)
    return false;
  auto* context_provider = output_surface_->context_provider();
  return context_provider->ContextCapabilities().post_sub_buffer;
}

ResourceFormat SkiaRenderer::BackbufferFormat() const {
  // From GL Renderer.
  if (current_frame()->current_render_pass->color_space.IsHDR() &&
      resource_provider_->IsRenderBufferFormatSupported(RGBA_F16)) {
    return RGBA_F16;
  }
  return resource_provider_->best_texture_format();
}

void SkiaRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("cc", "SkiaRenderer::BeginDrawingFrame");
  // Copied from GLRenderer.
  bool use_sync_query_ = false;
  scoped_refptr<cc::ResourceProvider::Fence> read_lock_fence;
  // TODO(weiliangc): Implement use_sync_query_. (crbug.com/644851)
  if (use_sync_query_) {
    NOTIMPLEMENTED();
  } else {
    read_lock_fence =
        base::MakeRefCounted<cc::ResourceProvider::SynchronousFence>(
            output_surface_->context_provider()->ContextGL());
  }
  resource_provider_->SetReadLockFence(read_lock_fence.get());

  // Insert WaitSyncTokenCHROMIUM on quad resources prior to drawing the frame,
  // so that drawing can proceed without GL context switching interruptions.
  cc::ResourceProvider* resource_provider = resource_provider_;
  for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider->WaitSyncToken(resource_id);
    }
  }
}

void SkiaRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("cc", "SkiaRenderer::FinishDrawingFrame");
  if (settings_->show_overdraw_feedback) {
    sk_sp<SkImage> image = overdraw_surface_->makeImageSnapshot();
    SkPaint paint;
    static const SkPMColor colors[SkOverdrawColorFilter::kNumColors] = {
        0x00000000, 0x00000000, 0x2f0000ff, 0x2f00ff00, 0x3fff0000, 0x7fff0000,
    };
    sk_sp<SkColorFilter> color_filter = SkOverdrawColorFilter::Make(colors);
    paint.setColorFilter(color_filter);
    root_surface_->getCanvas()->drawImage(image.get(), 0, 0, &paint);
    root_surface_->getCanvas()->flush();
  }
  current_framebuffer_surface_lock_ = nullptr;
  current_framebuffer_lock_ = nullptr;
  current_canvas_ = nullptr;

  swap_buffer_rect_ = current_frame()->root_damage_rect;

  if (use_swap_with_bounds_)
    swap_content_bounds_ = current_frame()->root_content_bounds;
}

void SkiaRenderer::SwapBuffers(std::vector<ui::LatencyInfo> latency_info) {
  DCHECK(visible_);
  TRACE_EVENT0("cc,benchmark", "SkiaRenderer::SwapBuffers");
  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(latency_info);
  output_frame.size = surface_size_for_swap_buffers();
  if (use_swap_with_bounds_) {
    output_frame.content_bounds = std::move(swap_content_bounds_);
  } else if (use_partial_swap_) {
    swap_buffer_rect_.Intersect(gfx::Rect(surface_size_for_swap_buffers()));
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  } else if (swap_buffer_rect_.IsEmpty() && allow_empty_swap_) {
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  }
  output_surface_->SwapBuffers(std::move(output_frame));

  swap_buffer_rect_ = gfx::Rect();
}

bool SkiaRenderer::FlippedFramebuffer() const {
  // TODO(weiliangc): Make sure flipped correctly for Windows.
  // (crbug.com/644851)
  return false;
}

void SkiaRenderer::EnsureScissorTestEnabled() {
  is_scissor_enabled_ = true;
}

void SkiaRenderer::EnsureScissorTestDisabled() {
  is_scissor_enabled_ = false;
}

void SkiaRenderer::BindFramebufferToOutputSurface() {
  DCHECK(!output_surface_->HasExternalStencilTest());
  current_framebuffer_lock_ = nullptr;

  // TODO(weiliangc): Set up correct can_use_lcd_text and
  // use_distance_field_text for SkSurfaceProps flags. How to setup is in
  // ResourceProvider. (crbug.com/644851)

  GrContext* gr_context = output_surface_->context_provider()->GrContext();
  if (!root_canvas_ || root_canvas_->getGrContext() != gr_context ||
      gfx::SkISizeToSize(root_canvas_->getBaseLayerSize()) !=
          current_frame()->device_viewport_size) {
    // Either no SkSurface setup yet, or new GrContext, need to create new
    // surface.
    GrGLFramebufferInfo framebuffer_info;
    framebuffer_info.fFBOID = 0;
    GrBackendRenderTarget render_target(
        current_frame()->device_viewport_size.width(),
        current_frame()->device_viewport_size.height(), 0, 8,
        kRGBA_8888_GrPixelConfig, framebuffer_info);

    // This is for use_distance_field_text false, and can_use_lcd_text true.
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    SkSurfaceProps surface_props =
        SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

    root_surface_ = SkSurface::MakeFromBackendRenderTarget(
        gr_context, render_target, kBottomLeft_GrSurfaceOrigin, nullptr,
        &surface_props);
  }

  root_canvas_ = root_surface_->getCanvas();
  if (settings_->show_overdraw_feedback) {
    const gfx::Size size(root_surface_->width(), root_surface_->height());
    overdraw_surface_ = root_surface_->makeSurface(
        SkImageInfo::MakeA8(size.width(), size.height()));
    nway_canvas_ = std::make_unique<SkNWayCanvas>(size.width(), size.height());
    overdraw_canvas_ =
        std::make_unique<SkOverdrawCanvas>(overdraw_surface_->getCanvas());
    nway_canvas_->addCanvas(overdraw_canvas_.get());
    nway_canvas_->addCanvas(root_canvas_);
    current_canvas_ = nway_canvas_.get();
  } else {
    current_canvas_ = root_canvas_;
  }
}

bool SkiaRenderer::BindFramebufferToTexture(const cc::ScopedResource* texture) {
  DCHECK(texture->id());

  // Explicitly release lock, otherwise we can crash when try to lock
  // same texture again.
  current_framebuffer_surface_lock_ = nullptr;
  current_framebuffer_lock_ = nullptr;
  current_framebuffer_lock_ =
      base::WrapUnique(new cc::ResourceProvider::ScopedWriteLockGL(
          resource_provider_, texture->id()));

  current_framebuffer_surface_lock_ =
      base::WrapUnique(new cc::ResourceProvider::ScopedSkSurface(
          output_surface_->context_provider()->GrContext(),
          current_framebuffer_lock_->GetTexture(),
          current_framebuffer_lock_->target(),
          current_framebuffer_lock_->size(),
          current_framebuffer_lock_->format(), false, true, 0));

  current_canvas_ = current_framebuffer_surface_lock_->surface()->getCanvas();
  return true;
}

void SkiaRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
}

void SkiaRenderer::SetClipRect(const gfx::Rect& rect) {
  if (!current_canvas_)
    return;
  // Skia applies the current matrix to clip rects so we reset it temporary.
  SkMatrix current_matrix = current_canvas_->getTotalMatrix();
  current_canvas_->resetMatrix();
  // SetClipRect is assumed to be applied temporarily, on an
  // otherwise-unclipped canvas.
  DCHECK_EQ(current_canvas_->getDeviceClipBounds().width(),
            current_canvas_->imageInfo().width());
  DCHECK_EQ(current_canvas_->getDeviceClipBounds().height(),
            current_canvas_->imageInfo().height());
  current_canvas_->clipRect(gfx::RectToSkRect(rect));
  current_canvas_->setMatrix(current_matrix);
}

void SkiaRenderer::ClearCanvas(SkColor color) {
  if (!current_canvas_)
    return;

  if (is_scissor_enabled_) {
    // The same paint used by SkCanvas::clear, but applied to the scissor rect.
    SkPaint clear_paint;
    clear_paint.setColor(color);
    clear_paint.setBlendMode(SkBlendMode::kSrc);
    current_canvas_->drawRect(gfx::RectToSkRect(scissor_rect_), clear_paint);
  } else {
    current_canvas_->clear(color);
  }
}

void SkiaRenderer::ClearFramebuffer() {
  if (current_frame()->current_render_pass->has_transparent_background) {
    ClearCanvas(SkColorSetARGB(0, 0, 0, 0));
  } else {
#ifndef NDEBUG
    // On DEBUG builds, opaque render passes are cleared to blue
    // to easily see regions that were not drawn on the screen.
    ClearCanvas(SkColorSetARGB(255, 0, 0, 255));
#endif
  }
}

void SkiaRenderer::PrepareSurfaceForPass(
    SurfaceInitializationMode initialization_mode,
    const gfx::Rect& render_pass_scissor) {
  switch (initialization_mode) {
    case SURFACE_INITIALIZATION_MODE_PRESERVE:
      EnsureScissorTestDisabled();
      return;
    case SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR:
      EnsureScissorTestDisabled();
      ClearFramebuffer();
      break;
    case SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR:
      SetScissorTestRect(render_pass_scissor);
      ClearFramebuffer();
      break;
  }
}

bool SkiaRenderer::IsSoftwareResource(ResourceId resource_id) const {
  switch (resource_provider_->GetResourceType(resource_id)) {
    case cc::ResourceProvider::RESOURCE_TYPE_GPU_MEMORY_BUFFER:
    case cc::ResourceProvider::RESOURCE_TYPE_GL_TEXTURE:
      return true;
    case cc::ResourceProvider::RESOURCE_TYPE_BITMAP:
      return false;
  }

  LOG(FATAL) << "Invalid resource type.";
  return false;
}

void SkiaRenderer::DoDrawQuad(const DrawQuad* quad,
                              const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;
  if (draw_region) {
    current_canvas_->save();
  }

  TRACE_EVENT0("cc", "SkiaRenderer::DoDrawQuad");
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(quad->rect));
  gfx::Transform contents_device_transform =
      current_frame()->window_matrix * current_frame()->projection_matrix *
      quad_rect_matrix;
  contents_device_transform.FlattenTo2d();
  SkMatrix sk_device_matrix;
  gfx::TransformToFlattenedSkMatrix(contents_device_transform,
                                    &sk_device_matrix);
  current_canvas_->setMatrix(sk_device_matrix);

  current_paint_.reset();
  if (settings_->force_antialiasing ||
      !IsScaleAndIntegerTranslate(sk_device_matrix)) {
    // TODO(danakj): Until we can enable AA only on exterior edges of the
    // layer, disable AA if any interior edges are present. crbug.com/248175
    bool all_four_edges_are_exterior =
        quad->IsTopEdge() && quad->IsLeftEdge() && quad->IsBottomEdge() &&
        quad->IsRightEdge();
    if (settings_->allow_antialiasing &&
        (settings_->force_antialiasing || all_four_edges_are_exterior))
      current_paint_.setAntiAlias(true);
    current_paint_.setFilterQuality(kLow_SkFilterQuality);
  }

  if (quad->ShouldDrawWithBlending() ||
      quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver) {
    current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
    current_paint_.setBlendMode(
        static_cast<SkBlendMode>(quad->shared_quad_state->blend_mode));
  } else {
    current_paint_.setBlendMode(SkBlendMode::kSrc);
  }

  if (draw_region) {
    gfx::QuadF local_draw_region(*draw_region);
    SkPath draw_region_clip_path;
    local_draw_region -=
        gfx::Vector2dF(quad->visible_rect.x(), quad->visible_rect.y());
    local_draw_region.Scale(1.0f / quad->visible_rect.width(),
                            1.0f / quad->visible_rect.height());
    local_draw_region -= gfx::Vector2dF(0.5f, 0.5f);

    SkPoint clip_points[4];
    QuadFToSkPoints(local_draw_region, clip_points);
    draw_region_clip_path.addPoly(clip_points, 4, true);

    current_canvas_->clipPath(draw_region_clip_path);
  }

  switch (quad->material) {
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::PICTURE_CONTENT:
      DrawPictureQuad(PictureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(RenderPassDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TEXTURE_CONTENT:
      DrawTextureQuad(TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TILED_CONTENT:
      DrawTileQuad(TileDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SURFACE_CONTENT:
      // Surface content should be fully resolved to other quad types before
      // reaching a direct renderer.
      NOTREACHED();
      break;
    case DrawQuad::INVALID:
    case DrawQuad::YUV_VIDEO_CONTENT:
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawUnsupportedQuad(quad);
      NOTREACHED();
      break;
  }

  current_canvas_->resetMatrix();
  if (draw_region) {
    current_canvas_->restore();
  }
}

void SkiaRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad) {
  // We need to apply the matrix manually to have pixel-sized stroke width.
  SkPoint vertices[4];
  gfx::RectFToSkRect(QuadVertexRect()).toQuad(vertices);
  SkPoint transformed_vertices[4];
  current_canvas_->getTotalMatrix().mapPoints(transformed_vertices, vertices,
                                              4);
  current_canvas_->resetMatrix();

  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->shared_quad_state->opacity *
                          SkColorGetA(quad->color));
  current_paint_.setStyle(SkPaint::kStroke_Style);
  current_paint_.setStrokeWidth(quad->width);
  current_canvas_->drawPoints(SkCanvas::kPolygon_PointMode, 4,
                              transformed_vertices, current_paint_);
}

void SkiaRenderer::DrawPictureQuad(const PictureDrawQuad* quad) {
  SkMatrix content_matrix;
  content_matrix.setRectToRect(gfx::RectFToSkRect(quad->tex_coord_rect),
                               gfx::RectFToSkRect(QuadVertexRect()),
                               SkMatrix::kFill_ScaleToFit);
  current_canvas_->concat(content_matrix);

  const bool needs_transparency =
      SkScalarRoundToInt(quad->shared_quad_state->opacity * 255) < 255;
  const bool disable_image_filtering =
      disable_picture_quad_image_filtering_ || quad->nearest_neighbor;

  TRACE_EVENT0("cc", "SkiaRenderer::DrawPictureQuad");

  SkCanvas* raster_canvas = current_canvas_;

  std::unique_ptr<SkCanvas> color_transform_canvas;
  if (settings_->enable_color_correct_rendering) {
    // TODO(enne): color transform needs to be replicated in gles2_cmd_decoder
    color_transform_canvas = SkCreateColorSpaceXformCanvas(
        current_canvas_, gfx::ColorSpace::CreateSRGB().ToSkColorSpace());
    raster_canvas = color_transform_canvas.get();
  }

  base::Optional<skia::OpacityFilterCanvas> opacity_canvas;
  if (needs_transparency || disable_image_filtering) {
    // TODO(aelias): This isn't correct in all cases. We should detect these
    // cases and fall back to a persistent bitmap backing
    // (http://crbug.com/280374).
    // TODO(vmpstr): Fold this canvas into playback and have raster source
    // accept a set of settings on playback that will determine which canvas to
    // apply. (http://crbug.com/594679)
    opacity_canvas.emplace(raster_canvas, quad->shared_quad_state->opacity,
                           disable_image_filtering);
    raster_canvas = &*opacity_canvas;
  }

  // Treat all subnormal values as zero for performance.
  cc::ScopedSubnormalFloatDisabler disabler;

  raster_canvas->save();
  raster_canvas->translate(-quad->content_rect.x(), -quad->content_rect.y());
  raster_canvas->clipRect(gfx::RectToSkRect(quad->content_rect));
  raster_canvas->scale(quad->contents_scale, quad->contents_scale);
  quad->display_item_list->Raster(raster_canvas);
  raster_canvas->restore();
}

void SkiaRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad) {
  gfx::RectF visible_quad_vertex_rect = cc::MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->shared_quad_state->opacity *
                          SkColorGetA(quad->color));
  current_canvas_->drawRect(gfx::RectFToSkRect(visible_quad_vertex_rect),
                            current_paint_);
}

void SkiaRenderer::DrawTextureQuad(const TextureDrawQuad* quad) {
  if (!IsSoftwareResource(quad->resource_id())) {
    DrawUnsupportedQuad(quad);
    return;
  }

  // TODO(skaslev): Add support for non-premultiplied alpha.
  cc::DisplayResourceProvider::ScopedReadLockSkImage lock(resource_provider_,
                                                          quad->resource_id());
  const SkImage* image = lock.sk_image();
  if (!image)
    return;
  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  gfx::RectF visible_uv_rect = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect sk_uv_rect = gfx::RectFToSkRect(visible_uv_rect);
  gfx::RectF visible_quad_vertex_rect = cc::MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect quad_rect = gfx::RectFToSkRect(visible_quad_vertex_rect);

  if (quad->y_flipped)
    current_canvas_->scale(1, -1);

  bool blend_background =
      quad->background_color != SK_ColorTRANSPARENT && !image->isOpaque();
  bool needs_layer = blend_background && (current_paint_.getAlpha() != 0xFF);
  if (needs_layer) {
    current_canvas_->saveLayerAlpha(&quad_rect, current_paint_.getAlpha());
    current_paint_.setAlpha(0xFF);
  }
  if (blend_background) {
    SkPaint background_paint;
    background_paint.setColor(quad->background_color);
    current_canvas_->drawRect(quad_rect, background_paint);
  }
  current_paint_.setFilterQuality(
      quad->nearest_neighbor ? kNone_SkFilterQuality : kLow_SkFilterQuality);
  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, &current_paint_);
  if (needs_layer)
    current_canvas_->restore();
}

void SkiaRenderer::DrawTileQuad(const TileDrawQuad* quad) {
  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  DCHECK(IsSoftwareResource(quad->resource_id()));
  cc::DisplayResourceProvider::ScopedReadLockSkImage lock(resource_provider_,
                                                          quad->resource_id());
  if (!lock.sk_image())
    return;
  gfx::RectF visible_tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));
  gfx::RectF visible_quad_vertex_rect = cc::MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));

  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  current_paint_.setFilterQuality(
      quad->nearest_neighbor ? kNone_SkFilterQuality : kLow_SkFilterQuality);
  current_canvas_->drawImageRect(lock.sk_image(), uv_rect,
                                 gfx::RectFToSkRect(visible_quad_vertex_rect),
                                 &current_paint_);
}

void SkiaRenderer::DrawRenderPassQuad(const RenderPassDrawQuad* quad) {
  cc::ScopedResource* content_texture =
      render_pass_textures_[quad->render_pass_id].get();
  DCHECK(content_texture);
  DCHECK(content_texture->id());
  DCHECK(IsSoftwareResource(content_texture->id()));
  cc::DisplayResourceProvider::ScopedReadLockSkImage lock(
      resource_provider_, content_texture->id());
  if (!lock.sk_image())
    return;

  SkRect dest_rect = gfx::RectFToSkRect(QuadVertexRect());
  SkRect dest_visible_rect =
      gfx::RectFToSkRect(cc::MathUtil::ScaleRectProportional(
          QuadVertexRect(), gfx::RectF(quad->rect),
          gfx::RectF(quad->visible_rect)));
  SkRect content_rect = RectFToSkRect(quad->tex_coord_rect);

  const SkImage* content = lock.sk_image();

  current_canvas_->drawImageRect(lock.sk_image(), content_rect,
                                 dest_visible_rect, &current_paint_);

  const cc::FilterOperations* filters = FiltersForPass(quad->render_pass_id);

  // TODO(weiliangc): Implement filters. (crbug.com/644851)
  if (filters) {
    NOTIMPLEMENTED();
  }

  SkMatrix content_mat;
  content_mat.setRectToRect(content_rect, dest_rect,
                            SkMatrix::kFill_ScaleToFit);

  sk_sp<SkShader> shader;
  shader = content->makeShader(SkShader::kClamp_TileMode,
                               SkShader::kClamp_TileMode, &content_mat);

  // TODO(weiliangc): Implement mask. (crbug.com/644851)
  if (quad->mask_resource_id()) {
    NOTIMPLEMENTED();
  }

  // TODO(weiliangc): If we have a background filter shader, render its results
  // first. (crbug.com/644851)

  current_paint_.setShader(std::move(shader));
  current_canvas_->drawRect(dest_visible_rect, current_paint_);
}

void SkiaRenderer::DrawUnsupportedQuad(const DrawQuad* quad) {
  // TODO(weiliangc): Make sure unsupported quads work. (crbug.com/644851)
  NOTIMPLEMENTED();
#ifdef NDEBUG
  current_paint_.setColor(SK_ColorWHITE);
#else
  current_paint_.setColor(SK_ColorMAGENTA);
#endif
  current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
  current_canvas_->drawRect(gfx::RectFToSkRect(QuadVertexRect()),
                            current_paint_);
}

void SkiaRenderer::CopyDrawnRenderPass(
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(weiliangc): Make copy request work. (crbug.com/644851)
  NOTIMPLEMENTED();
}

void SkiaRenderer::SetEnableDCLayers(bool enable) {
  // TODO(crbug.com/678800): Part of surport overlay on Windows.
  NOTIMPLEMENTED();
}

void SkiaRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

void SkiaRenderer::FinishDrawingQuadList() {
  current_canvas_->flush();
}

void SkiaRenderer::GenerateMipmap() {
  // TODO(reveman): Generates mipmaps for current canvas. (crbug.com/763664)
  NOTIMPLEMENTED();
}

bool SkiaRenderer::ShouldApplyBackgroundFilters(
    const RenderPassDrawQuad* quad,
    const cc::FilterOperations* background_filters) const {
  if (!background_filters)
    return false;
  DCHECK(!background_filters->IsEmpty());

  // TODO(hendrikw): Look into allowing background filters to see pixels from
  // other render targets.  See crbug.com/314867.

  return true;
}

// If non-null, auto_bounds will be filled with the automatically-computed
// destination bounds. If null, the output will be the same size as the
// input bitmap.
sk_sp<SkImage> SkiaRenderer::ApplyImageFilter(SkImageFilter* filter,
                                              const RenderPassDrawQuad* quad,
                                              const SkBitmap& to_filter,
                                              SkIRect* auto_bounds) const {
  // TODO(weiliangc): Implement image filter. (crbug.com/644851)
  NOTIMPLEMENTED();
  return nullptr;
}

SkBitmap SkiaRenderer::GetBackdropBitmap(const gfx::Rect& bounding_rect) const {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(bounding_rect.width(),
                                                bounding_rect.height()));
  if (!current_canvas_->readPixels(bitmap, bounding_rect.x(),
                                   bounding_rect.y()))
    bitmap.reset();
  return bitmap;
}

gfx::Rect SkiaRenderer::GetBackdropBoundingBoxForRenderPassQuad(
    const RenderPassDrawQuad* quad,
    const gfx::Transform& contents_device_transform,
    const cc::FilterOperations* background_filters) const {
  DCHECK(ShouldApplyBackgroundFilters(quad, background_filters));
  gfx::Rect backdrop_rect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
      contents_device_transform, QuadVertexRect()));

  SkMatrix matrix;
  matrix.setScale(quad->filters_scale.x(), quad->filters_scale.y());
  backdrop_rect = background_filters->MapRectReverse(backdrop_rect, matrix);

  backdrop_rect.Intersect(MoveFromDrawToWindowSpace(
      current_frame()->current_render_pass->output_rect));

  return backdrop_rect;
}

sk_sp<SkShader> SkiaRenderer::GetBackgroundFilterShader(
    const RenderPassDrawQuad* quad,
    SkShader::TileMode content_tile_mode) const {
  // TODO(weiliangc): properly implement background filters. (crbug.com/644851)
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace viz
