// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/software_renderer.h"

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
#include "components/viz/service/display/software_output_device.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkLayerRasterizer.h"
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

SoftwareRenderer::SoftwareRenderer(
    const RendererSettings* settings,
    OutputSurface* output_surface,
    cc::DisplayResourceProvider* resource_provider)
    : DirectRenderer(settings, output_surface, resource_provider),
      output_device_(output_surface->software_device()) {}

SoftwareRenderer::~SoftwareRenderer() {}

bool SoftwareRenderer::CanPartialSwap() {
  return true;
}

ResourceFormat SoftwareRenderer::BackbufferFormat() const {
  return resource_provider_->best_texture_format();
}

void SoftwareRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("cc", "SoftwareRenderer::BeginDrawingFrame");
  root_canvas_ = output_device_->BeginPaint(current_frame()->root_damage_rect);
}

void SoftwareRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("cc", "SoftwareRenderer::FinishDrawingFrame");
  current_framebuffer_lock_ = nullptr;
  current_framebuffer_canvas_.reset();
  current_canvas_ = nullptr;
  root_canvas_ = nullptr;

  output_device_->EndPaint();
}

void SoftwareRenderer::SwapBuffers(std::vector<ui::LatencyInfo> latency_info) {
  DCHECK(visible_);
  TRACE_EVENT0("cc", "SoftwareRenderer::SwapBuffers");
  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(latency_info);
  output_surface_->SwapBuffers(std::move(output_frame));
}

bool SoftwareRenderer::FlippedFramebuffer() const {
  return false;
}

void SoftwareRenderer::EnsureScissorTestEnabled() {
  is_scissor_enabled_ = true;
}

void SoftwareRenderer::EnsureScissorTestDisabled() {
  is_scissor_enabled_ = false;
}

void SoftwareRenderer::BindFramebufferToOutputSurface() {
  DCHECK(!output_surface_->HasExternalStencilTest());
  current_framebuffer_lock_ = nullptr;
  current_framebuffer_canvas_.reset();
  current_canvas_ = root_canvas_;
}

bool SoftwareRenderer::BindFramebufferToTexture(
    const cc::ScopedResource* texture) {
  DCHECK(texture->id());

  // Explicitly release lock, otherwise we can crash when try to lock
  // same texture again.
  current_framebuffer_lock_ = nullptr;
  current_framebuffer_lock_ =
      std::make_unique<cc::ResourceProvider::ScopedWriteLockSoftware>(
          resource_provider_, texture->id());
  current_framebuffer_canvas_ =
      std::make_unique<SkCanvas>(current_framebuffer_lock_->sk_bitmap());
  current_canvas_ = current_framebuffer_canvas_.get();
  return true;
}

void SoftwareRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
}

void SoftwareRenderer::SetClipRect(const gfx::Rect& rect) {
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

void SoftwareRenderer::ClearCanvas(SkColor color) {
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

void SoftwareRenderer::ClearFramebuffer() {
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

void SoftwareRenderer::PrepareSurfaceForPass(
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

bool SoftwareRenderer::IsSoftwareResource(ResourceId resource_id) const {
  switch (resource_provider_->GetResourceType(resource_id)) {
    case cc::ResourceProvider::RESOURCE_TYPE_GPU_MEMORY_BUFFER:
    case cc::ResourceProvider::RESOURCE_TYPE_GL_TEXTURE:
      return false;
    case cc::ResourceProvider::RESOURCE_TYPE_BITMAP:
      return true;
  }

  LOG(FATAL) << "Invalid resource type.";
  return false;
}

void SoftwareRenderer::DoDrawQuad(const DrawQuad* quad,
                                  const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;

  TRACE_EVENT0("cc", "SoftwareRenderer::DoDrawQuad");
  bool do_save = draw_region || is_scissor_enabled_;
  SkAutoCanvasRestore canvas_restore(current_canvas_, do_save);
  if (is_scissor_enabled_) {
    SetClipRect(scissor_rect_);
  }

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
    current_paint_.setBlendMode(quad->shared_quad_state->blend_mode);
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
}

void SoftwareRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad) {
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

void SoftwareRenderer::DrawPictureQuad(const PictureDrawQuad* quad) {
  SkMatrix content_matrix;
  content_matrix.setRectToRect(gfx::RectFToSkRect(quad->tex_coord_rect),
                               gfx::RectFToSkRect(QuadVertexRect()),
                               SkMatrix::kFill_ScaleToFit);
  current_canvas_->concat(content_matrix);

  const bool needs_transparency =
      SkScalarRoundToInt(quad->shared_quad_state->opacity * 255) < 255;
  const bool disable_image_filtering =
      disable_picture_quad_image_filtering_ || quad->nearest_neighbor;

  TRACE_EVENT0("cc", "SoftwareRenderer::DrawPictureQuad");

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

void SoftwareRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad) {
  gfx::RectF visible_quad_vertex_rect = cc::MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->shared_quad_state->opacity *
                          SkColorGetA(quad->color));
  current_canvas_->drawRect(gfx::RectFToSkRect(visible_quad_vertex_rect),
                            current_paint_);
}

void SoftwareRenderer::DrawTextureQuad(const TextureDrawQuad* quad) {
  if (!IsSoftwareResource(quad->resource_id())) {
    DrawUnsupportedQuad(quad);
    return;
  }

  // TODO(skaslev): Add support for non-premultiplied alpha.
  cc::DisplayResourceProvider::ScopedReadLockSkImage lock(resource_provider_,
                                                          quad->resource_id());
  if (!lock.valid())
    return;
  const SkImage* image = lock.sk_image();
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

void SoftwareRenderer::DrawTileQuad(const TileDrawQuad* quad) {
  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  DCHECK(IsSoftwareResource(quad->resource_id()));

  cc::DisplayResourceProvider::ScopedReadLockSkImage lock(resource_provider_,
                                                          quad->resource_id());
  if (!lock.valid())
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

void SoftwareRenderer::DrawRenderPassQuad(const RenderPassDrawQuad* quad) {
  cc::ScopedResource* content_texture =
      render_pass_textures_[quad->render_pass_id].get();
  DCHECK(content_texture);
  DCHECK(content_texture->id());
  DCHECK(IsSoftwareResource(content_texture->id()));

  cc::DisplayResourceProvider::ScopedReadLockSoftware lock(
      resource_provider_, content_texture->id());
  if (!lock.valid())
    return;

  SkRect dest_rect = gfx::RectFToSkRect(QuadVertexRect());
  SkRect dest_visible_rect =
      gfx::RectFToSkRect(cc::MathUtil::ScaleRectProportional(
          QuadVertexRect(), gfx::RectF(quad->rect),
          gfx::RectF(quad->visible_rect)));
  SkRect content_rect = RectFToSkRect(quad->tex_coord_rect);

  const SkBitmap* content = lock.sk_bitmap();

  sk_sp<SkImage> filter_image;
  const cc::FilterOperations* filters = FiltersForPass(quad->render_pass_id);
  if (filters) {
    DCHECK(!filters->IsEmpty());
    sk_sp<SkImageFilter> image_filter =
        cc::RenderSurfaceFilters::BuildImageFilter(
            *filters, gfx::SizeF(content_texture->size()));
    if (image_filter) {
      SkIRect result_rect;
      // TODO(ajuma): Apply the filter in the same pass as the content where
      // possible (e.g. when there's no origin offset). See crbug.com/308201.
      filter_image =
          ApplyImageFilter(image_filter.get(), quad, *content, &result_rect);
      if (result_rect.isEmpty()) {
        return;
      }
      if (filter_image) {
        gfx::RectF rect = gfx::SkRectToRectF(SkRect::Make(result_rect));
        dest_rect = dest_visible_rect =
            gfx::RectFToSkRect(cc::MathUtil::ScaleRectProportional(
                QuadVertexRect(), gfx::RectF(quad->rect), rect));
        content_rect =
            SkRect::MakeWH(result_rect.width(), result_rect.height());
      }
    }
  }

  SkMatrix content_mat;
  content_mat.setRectToRect(content_rect, dest_rect,
                            SkMatrix::kFill_ScaleToFit);

  sk_sp<SkShader> shader;
  if (!filter_image) {
    shader =
        SkShader::MakeBitmapShader(*content, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode, &content_mat);
  } else {
    shader = filter_image->makeShader(SkShader::kClamp_TileMode,
                                      SkShader::kClamp_TileMode, &content_mat);
  }

  std::unique_ptr<cc::DisplayResourceProvider::ScopedReadLockSoftware>
      mask_lock;
  if (quad->mask_resource_id()) {
    mask_lock =
        std::make_unique<cc::DisplayResourceProvider::ScopedReadLockSoftware>(
            resource_provider_, quad->mask_resource_id());

    if (!mask_lock->valid())
      return;

    const SkBitmap* mask = mask_lock->sk_bitmap();

    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect = gfx::RectFToSkRect(
        gfx::ScaleRect(quad->mask_uv_rect, quad->mask_texture_size.width(),
                       quad->mask_texture_size.height()));

    SkMatrix mask_mat;
    mask_mat.setRectToRect(mask_rect, dest_rect, SkMatrix::kFill_ScaleToFit);

    SkPaint mask_paint;
    mask_paint.setShader(
        SkShader::MakeBitmapShader(*mask, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode, &mask_mat));

    SkLayerRasterizer::Builder builder;
    builder.addLayer(mask_paint);

    current_paint_.setRasterizer(builder.detach());
  }

  // If we have a background filter shader, render its results first.
  sk_sp<SkShader> background_filter_shader =
      GetBackgroundFilterShader(quad, SkShader::kClamp_TileMode);
  if (background_filter_shader) {
    SkPaint paint;
    paint.setShader(std::move(background_filter_shader));
    paint.setRasterizer(current_paint_.refRasterizer());
    current_canvas_->drawRect(dest_visible_rect, paint);
  }
  current_paint_.setShader(std::move(shader));
  current_canvas_->drawRect(dest_visible_rect, current_paint_);
}

void SoftwareRenderer::DrawUnsupportedQuad(const DrawQuad* quad) {
#ifdef NDEBUG
  current_paint_.setColor(SK_ColorWHITE);
#else
  current_paint_.setColor(SK_ColorMAGENTA);
#endif
  current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
  current_canvas_->drawRect(gfx::RectFToSkRect(QuadVertexRect()),
                            current_paint_);
}

void SoftwareRenderer::CopyDrawnRenderPass(
    std::unique_ptr<CopyOutputRequest> request) {
  // SoftwareRenderer supports RGBA_BITMAP only. For legacy reasons, if a
  // RGBA_TEXTURE request is being made, clients are prepared to accept
  // RGBA_BITMAP results.
  //
  // TODO(miu): Get rid of the legacy behavior and send empty results for
  // RGBA_TEXTURE requests once tab capture is moved into VIZ.
  // http://crbug.com/754872
  switch (request->result_format()) {
    case CopyOutputRequest::ResultFormat::RGBA_BITMAP:
    case CopyOutputRequest::ResultFormat::RGBA_TEXTURE:
      break;
  }

  gfx::Rect copy_rect = current_frame()->current_render_pass->output_rect;
  if (request->has_area())
    copy_rect.Intersect(request->area());
  gfx::Rect window_copy_rect = MoveFromDrawToWindowSpace(copy_rect);

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(
      window_copy_rect.width(), window_copy_rect.height(),
      current_canvas_->imageInfo().refColorSpace()));
  if (!current_canvas_->readPixels(bitmap, window_copy_rect.x(),
                                   window_copy_rect.y()))
    return;  // |request| auto-sends empty result on out-of-scope.

  request->SendResult(
      std::make_unique<CopyOutputSkBitmapResult>(copy_rect, bitmap));
}

void SoftwareRenderer::SetEnableDCLayers(bool enable) {
  NOTIMPLEMENTED();
}

void SoftwareRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

void SoftwareRenderer::GenerateMipmap() {
  NOTIMPLEMENTED();
}

bool SoftwareRenderer::ShouldApplyBackgroundFilters(
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
sk_sp<SkImage> SoftwareRenderer::ApplyImageFilter(
    SkImageFilter* filter,
    const RenderPassDrawQuad* quad,
    const SkBitmap& to_filter,
    SkIRect* auto_bounds) const {
  if (!filter)
    return nullptr;

  SkMatrix local_matrix;
  local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
  SkIRect dst_rect;
  if (auto_bounds) {
    dst_rect =
        filter->filterBounds(gfx::RectToSkIRect(quad->rect), local_matrix,
                             SkImageFilter::kForward_MapDirection);
    *auto_bounds = dst_rect;
  } else {
    dst_rect = to_filter.bounds();
  }

  SkImageInfo dst_info =
      SkImageInfo::MakeN32Premul(dst_rect.width(), dst_rect.height());
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(dst_info);

  if (!surface) {
    return nullptr;
  }

  SkPaint paint;
  // Treat subnormal float values as zero for performance.
  cc::ScopedSubnormalFloatDisabler disabler;
  paint.setImageFilter(filter->makeWithLocalMatrix(local_matrix));
  surface->getCanvas()->translate(-dst_rect.x(), -dst_rect.y());
  surface->getCanvas()->drawBitmap(to_filter, quad->rect.x(), quad->rect.y(),
                                   &paint);

  return surface->makeImageSnapshot();
}

SkBitmap SoftwareRenderer::GetBackdropBitmap(
    const gfx::Rect& bounding_rect) const {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(bounding_rect.width(),
                                                bounding_rect.height()));
  if (!current_canvas_->readPixels(bitmap, bounding_rect.x(),
                                   bounding_rect.y()))
    bitmap.reset();
  return bitmap;
}

gfx::Rect SoftwareRenderer::GetBackdropBoundingBoxForRenderPassQuad(
    const RenderPassDrawQuad* quad,
    const gfx::Transform& contents_device_transform,
    const cc::FilterOperations* background_filters,
    gfx::Rect* unclipped_rect) const {
  DCHECK(ShouldApplyBackgroundFilters(quad, background_filters));
  gfx::Rect backdrop_rect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
      contents_device_transform, QuadVertexRect()));

  SkMatrix matrix;
  matrix.setScale(quad->filters_scale.x(), quad->filters_scale.y());
  backdrop_rect = background_filters->MapRectReverse(backdrop_rect, matrix);

  *unclipped_rect = backdrop_rect;
  backdrop_rect.Intersect(MoveFromDrawToWindowSpace(
      current_frame()->current_render_pass->output_rect));

  return backdrop_rect;
}

sk_sp<SkShader> SoftwareRenderer::GetBackgroundFilterShader(
    const RenderPassDrawQuad* quad,
    SkShader::TileMode content_tile_mode) const {
  const cc::FilterOperations* background_filters =
      BackgroundFiltersForPass(quad->render_pass_id);
  if (!ShouldApplyBackgroundFilters(quad, background_filters))
    return nullptr;

  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(quad->rect));
  gfx::Transform contents_device_transform =
      current_frame()->window_matrix * current_frame()->projection_matrix *
      quad_rect_matrix;
  contents_device_transform.FlattenTo2d();

  gfx::Rect unclipped_rect;
  gfx::Rect backdrop_rect = GetBackdropBoundingBoxForRenderPassQuad(
      quad, contents_device_transform, background_filters, &unclipped_rect);

  // Figure out the transformations to move it back to pixel space.
  gfx::Transform contents_device_transform_inverse;
  if (!contents_device_transform.GetInverse(&contents_device_transform_inverse))
    return nullptr;

  SkMatrix filter_backdrop_transform =
      contents_device_transform_inverse.matrix();
  filter_backdrop_transform.preTranslate(backdrop_rect.x(), backdrop_rect.y());

  // Draw what's behind, and apply the filter to it.
  SkBitmap backdrop_bitmap = GetBackdropBitmap(backdrop_rect);

  gfx::Vector2dF clipping_offset =
      (unclipped_rect.top_right() - backdrop_rect.top_right()) +
      (backdrop_rect.bottom_left() - unclipped_rect.bottom_left());
  sk_sp<SkImageFilter> filter = cc::RenderSurfaceFilters::BuildImageFilter(
      *background_filters,
      gfx::SizeF(backdrop_bitmap.width(), backdrop_bitmap.height()),
      clipping_offset);
  sk_sp<SkImage> filter_backdrop_image =
      ApplyImageFilter(filter.get(), quad, backdrop_bitmap, nullptr);

  if (!filter_backdrop_image)
    return nullptr;

  return filter_backdrop_image->makeShader(content_tile_mode, content_tile_mode,
                                           &filter_backdrop_transform);
}

}  // namespace viz
