// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include "base/bits.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_fence.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/renderer_utils.h"
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
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/vk/GrVkBackendContext.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#endif

namespace viz {

SkiaRenderer::SkiaRenderer(const RendererSettings* settings,
                           OutputSurface* output_surface,
                           cc::DisplayResourceProvider* resource_provider)
    : DirectRenderer(settings, output_surface, resource_provider) {
#if BUILDFLAG(ENABLE_VULKAN)
  use_swap_with_bounds_ = false;
#else
  const auto& context_caps =
      output_surface_->context_provider()->ContextCapabilities();
  use_swap_with_bounds_ = context_caps.swap_buffers_with_bounds;
  if (context_caps.sync_query)
    sync_queries_ = base::Optional<SyncQueryCollection>(
        output_surface->context_provider()->ContextGL());
#endif
}

SkiaRenderer::~SkiaRenderer() {
#if BUILDFLAG(ENABLE_VULKAN)
  return;
#endif
}

bool SkiaRenderer::CanPartialSwap() {
#if BUILDFLAG(ENABLE_VULKAN)
  return false;
#endif
  if (use_swap_with_bounds_)
    return false;
  auto* context_provider = output_surface_->context_provider();
  return context_provider->ContextCapabilities().post_sub_buffer;
}

void SkiaRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::BeginDrawingFrame");
#if BUILDFLAG(ENABLE_VULKAN)
  return;
#else
  // Copied from GLRenderer.
  scoped_refptr<ResourceFence> read_lock_fence;
  if (sync_queries_) {
    read_lock_fence = sync_queries_->StartNewFrame();
  } else {
    read_lock_fence =
        base::MakeRefCounted<cc::DisplayResourceProvider::SynchronousFence>(
            output_surface_->context_provider()->ContextGL());
  }
  resource_provider_->SetReadLockFence(read_lock_fence.get());

  // Insert WaitSyncTokenCHROMIUM on quad resources prior to drawing the frame,
  // so that drawing can proceed without GL context switching interruptions.
  for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider_->WaitSyncToken(resource_id);
    }
  }
#endif
}

void SkiaRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::FinishDrawingFrame");
  if (sync_queries_) {
    sync_queries_->EndCurrentFrame();
  }

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
  non_root_surface_ = nullptr;
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
  non_root_surface_ = nullptr;

  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

#if BUILDFLAG(ENABLE_VULKAN)
  gpu::VulkanSurface* vulkan_surface = output_surface_->GetVulkanSurface();
  gpu::VulkanSwapChain* swap_chain = vulkan_surface->GetSwapChain();
  VkImage image = swap_chain->GetCurrentImage(swap_chain->current_image());

  GrVkImageInfo info_;
  info_.fImage = image;
  info_.fAlloc = {VK_NULL_HANDLE, 0, 0, 0};
  info_.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  info_.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
  info_.fFormat = VK_FORMAT_B8G8R8A8_UNORM;
  info_.fLevelCount = 1;

  GrBackendRenderTarget render_target(
      current_frame()->device_viewport_size.width(),
      current_frame()->device_viewport_size.height(), 0, 0, info_);

  GrContext* gr_context =
      output_surface_->vulkan_context_provider()->GetGrContext();
  root_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gr_context, render_target, kTopLeft_GrSurfaceOrigin, nullptr,
      &surface_props);
#else
  // TODO(weiliangc): Set up correct can_use_lcd_text for SkSurfaceProps flags.
  // How to setup is in ResourceProvider. (http://crbug.com/644851)

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

    root_surface_ = SkSurface::MakeFromBackendRenderTarget(
        gr_context, render_target, kBottomLeft_GrSurfaceOrigin, nullptr,
        &surface_props);
  }
#endif

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

void SkiaRenderer::BindFramebufferToTexture(const RenderPassId render_pass_id) {
  auto iter = render_pass_backings_.find(render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);
  // This function is called after AllocateRenderPassResourceIfNeeded, so there
  // should be backing ready.
  RenderPassBacking& backing = iter->second;
  non_root_surface_ = backing.render_pass_surface;
  current_canvas_ = non_root_surface_->getCanvas();
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

void SkiaRenderer::DoDrawQuad(const DrawQuad* quad,
                              const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;
  if (draw_region) {
    current_canvas_->save();
  }

  TRACE_EVENT0("viz", "SkiaRenderer::DoDrawQuad");
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

  TRACE_EVENT0("viz", "SkiaRenderer::DrawPictureQuad");

  SkCanvas* raster_canvas = current_canvas_;

  std::unique_ptr<SkCanvas> color_transform_canvas;
  // TODO(enne): color transform needs to be replicated in gles2_cmd_decoder
  color_transform_canvas = SkCreateColorSpaceXformCanvas(
      current_canvas_, gfx::ColorSpace::CreateSRGB().ToSkColorSpace());
  raster_canvas = color_transform_canvas.get();

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
  auto iter = render_pass_backings_.find(quad->render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);
  // This function is called after AllocateRenderPassResourceIfNeeded, so there
  // should be backing ready.
  RenderPassBacking& content_texture = iter->second;

  sk_sp<SkImage> content =
      content_texture.render_pass_surface->makeImageSnapshot();

  SkRect dest_rect = gfx::RectFToSkRect(QuadVertexRect());
  SkRect dest_visible_rect =
      gfx::RectFToSkRect(cc::MathUtil::ScaleRectProportional(
          QuadVertexRect(), gfx::RectF(quad->rect),
          gfx::RectF(quad->visible_rect)));
  SkRect content_rect = RectFToSkRect(quad->tex_coord_rect);

  current_canvas_->drawImageRect(content, content_rect, dest_visible_rect,
                                 &current_paint_);

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

void SkiaRenderer::UpdateRenderPassTextures(
    const RenderPassList& render_passes_in_draw_order,
    const base::flat_map<RenderPassId, RenderPassRequirements>&
        render_passes_in_frame) {
  std::vector<RenderPassId> passes_to_delete;
  for (const auto& pair : render_pass_backings_) {
    auto render_pass_it = render_passes_in_frame.find(pair.first);
    if (render_pass_it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(pair.first);
      continue;
    }

    const RenderPassRequirements& requirements = render_pass_it->second;
    const RenderPassBacking& backing = pair.second;
    SkSurface* backing_surface = pair.second.render_pass_surface.get();
    bool size_appropriate =
        backing_surface->width() >= requirements.size.width() &&
        backing_surface->height() >= requirements.size.height();
    bool mipmap_appropriate = !requirements.mipmap || backing.mipmap;
    if (!size_appropriate || !mipmap_appropriate)
      passes_to_delete.push_back(pair.first);
  }

  // Delete RenderPass backings from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i) {
    auto it = render_pass_backings_.find(passes_to_delete[i]);
    render_pass_backings_.erase(it);
  }
}

void SkiaRenderer::AllocateRenderPassResourceIfNeeded(
    const RenderPassId& render_pass_id,
    const RenderPassRequirements& requirements) {
  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end())
    return;

#if BUILDFLAG(ENABLE_VULKAN)
  GrContext* gr_context =
      output_surface_->vulkan_context_provider()->GetGrContext();
  // TODO(penghuang): check supported format correctly.
  bool capability_bgra8888 = true;
#else
  ContextProvider* context_provider = output_surface_->context_provider();
  bool capability_bgra8888 =
      context_provider->ContextCapabilities().texture_format_bgra8888;
  GrContext* gr_context = context_provider->GrContext();
#endif
  render_pass_backings_.insert(std::pair<RenderPassId, RenderPassBacking>(
      render_pass_id,
      RenderPassBacking(gr_context, requirements.size, requirements.mipmap,
                        capability_bgra8888,
                        current_frame()->current_render_pass->color_space)));
}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    GrContext* gr_context,
    const gfx::Size& size,
    bool mipmap,
    bool capability_bgra8888,
    const gfx::ColorSpace& color_space)
    : mipmap(mipmap), color_space(color_space) {
  ResourceFormat format;
  if (color_space.IsHDR()) {
    // If a platform does not support half-float renderbuffers then it should
    // not should request HDR rendering.
    // DCHECK(caps.texture_half_float_linear);
    // DCHECK(caps.color_buffer_half_float_rgba);
    format = RGBA_F16;
  } else {
    format = PlatformColor::BestSupportedTextureFormat(capability_bgra8888);
  }
  SkColorType color_type = ResourceFormatToClosestSkColorType(format);

  constexpr uint32_t flags = 0;
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  int msaa_sample_count = 0;
  SkImageInfo image_info = SkImageInfo::Make(
      size.width(), size.height(), color_type, kPremul_SkAlphaType, nullptr);
  render_pass_surface = SkSurface::MakeRenderTarget(
      gr_context, SkBudgeted::kNo, image_info, msaa_sample_count,
      kTopLeft_GrSurfaceOrigin, &surface_props, mipmap);
}

SkiaRenderer::RenderPassBacking::~RenderPassBacking() {}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    SkiaRenderer::RenderPassBacking&& other)
    : mipmap(other.mipmap), color_space(other.color_space) {
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
}

SkiaRenderer::RenderPassBacking& SkiaRenderer::RenderPassBacking::operator=(
    SkiaRenderer::RenderPassBacking&& other) {
  mipmap = other.mipmap;
  color_space = other.color_space;
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
  return *this;
}

bool SkiaRenderer::IsRenderPassResourceAllocated(
    const RenderPassId& render_pass_id) const {
  auto it = render_pass_backings_.find(render_pass_id);
  return it != render_pass_backings_.end();
}

gfx::Size SkiaRenderer::GetRenderPassBackingPixelSize(
    const RenderPassId& render_pass_id) {
  auto it = render_pass_backings_.find(render_pass_id);
  DCHECK(it != render_pass_backings_.end());
  SkSurface* texture = it->second.render_pass_surface.get();
  return gfx::Size(texture->width(), texture->height());
}

}  // namespace viz
