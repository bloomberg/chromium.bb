// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include "base/bits.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/paint/render_surface_filters.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/resource_fence.h"
#include "components/viz/service/display/resource_metadata.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/effects/SkShaderMaskFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace viz {
// Parameters needed to draw a RenderPassDrawQuad.
struct SkiaRenderer::DrawRenderPassDrawQuadParams {
  // The "in" parameters that will be used when apply filters.
  const cc::FilterOperations* filters = nullptr;

  // The "out" parameters returned by CalculateRPDQParams.
  // Root of the calculated image filter DAG to be applied to the render pass.
  sk_sp<SkImageFilter> image_filter;
  // Non-null |color_filter| should be applied when render pass is drawn. It is
  // a portion of the image filter DAG that was separated out because direct
  // color filter drawing is much faster than a colorFilterImageFilter drawing.
  // TODO(skbug.com/8700): Delete this after Skia side implementation is done.
  sk_sp<SkColorFilter> color_filter;
};

namespace {

bool IsTextureResource(DisplayResourceProvider* resource_provider,
                       ResourceId resource_id) {
  return !resource_provider->IsResourceSoftwareBacked(resource_id);
}

bool ApplyTransformAndScissorToTileRect(
    const gfx::Transform& contents_device_transform) {
  // This is slightly different than
  // gfx::Transform::IsPositiveScaleAndTranslation in that it also allows zero
  // scales. This is because in the common orthographic case the z scale is 0.
  if (!contents_device_transform.IsScaleOrTranslation())
    return false;
  return contents_device_transform.matrix().get(0, 0) >= 0.0 &&
         contents_device_transform.matrix().get(1, 1) >= 0.0 &&
         contents_device_transform.matrix().get(2, 2) >= 0.0;
}

}  // namespace

// Scoped helper class for building SkImage from resource id.
class SkiaRenderer::ScopedSkImageBuilder {
 public:
  ScopedSkImageBuilder(SkiaRenderer* skia_renderer,
                       ResourceId resource_id,
                       SkAlphaType alpha_type = kPremul_SkAlphaType);
  ~ScopedSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_; }

 private:
  base::Optional<DisplayResourceProvider::ScopedReadLockSkImage> lock_;
  const SkImage* sk_image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedSkImageBuilder);
};

SkiaRenderer::ScopedSkImageBuilder::ScopedSkImageBuilder(
    SkiaRenderer* skia_renderer,
    ResourceId resource_id,
    SkAlphaType alpha_type) {
  if (!resource_id)
    return;
  auto* resource_provider = skia_renderer->resource_provider_;
  if (!skia_renderer->is_using_ddl() || skia_renderer->non_root_surface_ ||
      !IsTextureResource(resource_provider, resource_id)) {
    // TODO(penghuang): remove this code when DDL is used everywhere.
    lock_.emplace(resource_provider, resource_id, alpha_type);
    sk_image_ = lock_->sk_image();
  } else {
    // Look up the image from promise_images_by resource_id and return the
    // reference. If the resource_id doesn't exist, this statement will
    // allocate it and return reference of it, and the reference will be used
    // to store the new created image later.
    auto& image = skia_renderer->promise_images_[resource_id];
    if (!image) {
      image = skia_renderer->lock_set_for_external_use_
                  ->LockResourceAndCreateSkImage(resource_id, alpha_type);
      LOG_IF(ERROR, !image) << "Failed to create the promise sk image.";
    }
    sk_image_ = image.get();
  }
}

class SkiaRenderer::ScopedYUVSkImageBuilder {
 public:
  ScopedYUVSkImageBuilder(SkiaRenderer* skia_renderer,
                          const YUVVideoDrawQuad* quad) {
    DCHECK(skia_renderer->is_using_ddl());
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->y_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->u_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->v_plane_resource_id()));
    DCHECK(quad->a_plane_resource_id() == kInvalidResourceId ||
           IsTextureResource(skia_renderer->resource_provider_,
                             quad->a_plane_resource_id()));

    YUVIds ids(quad->y_plane_resource_id(), quad->u_plane_resource_id(),
               quad->v_plane_resource_id(), quad->a_plane_resource_id());
    auto& image = skia_renderer->yuv_promise_images_[std::move(ids)];

    if (!image) {
      auto yuv_color_space = kRec601_SkYUVColorSpace;
      quad->video_color_space.ToSkYUVColorSpace(&yuv_color_space);

      const bool is_i420 =
          quad->u_plane_resource_id() != quad->v_plane_resource_id();
      const bool has_alpha = quad->a_plane_resource_id() != kInvalidResourceId;
      const size_t number_of_textures = (is_i420 ? 3 : 2) + (has_alpha ? 1 : 0);
      std::vector<ResourceMetadata> metadatas;
      metadatas.reserve(number_of_textures);
      auto y_metadata = skia_renderer->lock_set_for_external_use_->LockResource(
          quad->y_plane_resource_id());
      metadatas.push_back(std::move(y_metadata));
      auto u_metadata = skia_renderer->lock_set_for_external_use_->LockResource(
          quad->u_plane_resource_id());
      metadatas.push_back(std::move(u_metadata));
      if (is_i420) {
        auto v_metadata =
            skia_renderer->lock_set_for_external_use_->LockResource(
                quad->v_plane_resource_id());
        metadatas.push_back(std::move(v_metadata));
      }

      if (has_alpha) {
        auto a_metadata =
            skia_renderer->lock_set_for_external_use_->LockResource(
                quad->a_plane_resource_id());
        metadatas.push_back(std::move(a_metadata));
      }

      image = skia_renderer->skia_output_surface_->MakePromiseSkImageFromYUV(
          std::move(metadatas), yuv_color_space, has_alpha);
      LOG_IF(ERROR, !image) << "Failed to create the promise sk yuva image.";
    }
    sk_image_ = image.get();
  }

  ~ScopedYUVSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_; }

 private:
  std::unique_ptr<DisplayResourceProvider::ScopedReadLockSkImage> lock_;
  SkImage* sk_image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedYUVSkImageBuilder);
};

SkiaRenderer::SkiaRenderer(const RendererSettings* settings,
                           OutputSurface* output_surface,
                           DisplayResourceProvider* resource_provider,
                           SkiaOutputSurface* skia_output_surface,
                           DrawMode mode)
    : DirectRenderer(settings, output_surface, resource_provider),
      draw_mode_(mode),
      skia_output_surface_(skia_output_surface) {
  switch (draw_mode_) {
    case DrawMode::DDL: {
      DCHECK(skia_output_surface_);
      lock_set_for_external_use_.emplace(resource_provider,
                                         skia_output_surface);
      break;
    }
    case DrawMode::SKPRECORD: {
      DCHECK(output_surface_);
      context_provider_ = output_surface_->context_provider();
      const auto& context_caps = context_provider_->ContextCapabilities();
      use_swap_with_bounds_ = context_caps.swap_buffers_with_bounds;
      if (context_caps.sync_query) {
        sync_queries_ =
            base::Optional<SyncQueryCollection>(context_provider_->ContextGL());
      }
    }
  }
}

SkiaRenderer::~SkiaRenderer() = default;

bool SkiaRenderer::CanPartialSwap() {
  if (draw_mode_ != DrawMode::SKPRECORD)
    return false;

  DCHECK(context_provider_);
  if (use_swap_with_bounds_)
    return false;

  return context_provider_->ContextCapabilities().post_sub_buffer;
}

void SkiaRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::BeginDrawingFrame");
  if (draw_mode_ != DrawMode::SKPRECORD)
    return;

  // Copied from GLRenderer.
  scoped_refptr<ResourceFence> read_lock_fence;
  if (sync_queries_) {
    read_lock_fence = sync_queries_->StartNewFrame();
  } else {
    read_lock_fence =
        base::MakeRefCounted<DisplayResourceProvider::SynchronousFence>(
            context_provider_->ContextGL());
  }
  resource_provider_->SetReadLockFence(read_lock_fence.get());

  // Insert WaitSyncTokenCHROMIUM on quad resources prior to drawing the
  // frame, so that drawing can proceed without GL context switching
  // interruptions.
  for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider_->WaitSyncToken(resource_id);
    }
  }
}

void SkiaRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::FinishDrawingFrame");
  if (sync_queries_) {
    sync_queries_->EndCurrentFrame();
  }
  non_root_surface_ = nullptr;
  current_canvas_ = nullptr;
  current_surface_ = nullptr;

  swap_buffer_rect_ = current_frame()->root_damage_rect;

  if (use_swap_with_bounds_)
    swap_content_bounds_ = current_frame()->root_content_bounds;
}

void SkiaRenderer::SwapBuffers(std::vector<ui::LatencyInfo> latency_info) {
  DCHECK(visible_);
  TRACE_EVENT0("viz,benchmark", "SkiaRenderer::SwapBuffers");
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

  switch (draw_mode_) {
    case DrawMode::DDL: {
      skia_output_surface_->SkiaSwapBuffers(std::move(output_frame));
      break;
    }
    case DrawMode::SKPRECORD: {
      // write to skp files
      std::string file_name = "composited-frame.skp";
      SkFILEWStream file(file_name.c_str());
      DCHECK(file.isValid());

      auto data = root_picture_->serialize();
      file.write(data->data(), data->size());
      file.fsync();
      root_picture_ = nullptr;
      root_recorder_.reset();
    }
  }

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

  switch (draw_mode_) {
    case DrawMode::DDL: {
      root_canvas_ = skia_output_surface_->BeginPaintCurrentFrame();
      DCHECK(root_canvas_);
      break;
    }
    case DrawMode::SKPRECORD: {
      root_recorder_ = std::make_unique<SkPictureRecorder>();

      current_recorder_ = root_recorder_.get();
      current_picture_ = &root_picture_;
      root_canvas_ = root_recorder_->beginRecording(
          SkRect::MakeWH(current_frame()->device_viewport_size.width(),
                         current_frame()->device_viewport_size.height()));
      break;
    }
  }

  current_canvas_ = root_canvas_;
  current_surface_ = root_surface_.get();

  // For DDL mode, if overdraw feedback is enabled, the root canvas is the nway
  // canvas.
  if (settings_->show_overdraw_feedback && draw_mode_ != DrawMode::DDL) {
    const auto& size = current_frame()->device_viewport_size;
    overdraw_surface_ = root_canvas_->makeSurface(
        SkImageInfo::MakeA8(size.width(), size.height()));
    nway_canvas_ = std::make_unique<SkNWayCanvas>(size.width(), size.height());
    overdraw_canvas_ =
        std::make_unique<SkOverdrawCanvas>(overdraw_surface_->getCanvas());
    nway_canvas_->addCanvas(overdraw_canvas_.get());
    nway_canvas_->addCanvas(root_canvas_);
    current_canvas_ = nway_canvas_.get();
  }
}

void SkiaRenderer::BindFramebufferToTexture(const RenderPassId render_pass_id) {
  auto iter = render_pass_backings_.find(render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);
  // This function is called after AllocateRenderPassResourceIfNeeded, so there
  // should be backing ready.
  RenderPassBacking& backing = iter->second;
  switch (draw_mode_) {
    case DrawMode::DDL: {
      non_root_surface_ = nullptr;
      current_canvas_ = skia_output_surface_->BeginPaintRenderPass(
          render_pass_id, backing.size, backing.format, backing.generate_mipmap,
          backing.color_space.ToSkColorSpace());
      break;
    }
    case DrawMode::SKPRECORD: {
      current_recorder_ = backing.recorder.get();
      current_picture_ = &backing.picture;
      current_canvas_ = current_recorder_->beginRecording(
          SkRect::MakeWH(backing.size.width(), backing.size.height()));
    }
  }
}

void SkiaRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
}

void SkiaRenderer::ClearCanvas(SkColor color) {
  if (!current_canvas_)
    return;

  if (is_scissor_enabled_) {
    // Limit the clear with the scissor rect.
    SkAutoCanvasRestore autoRestore(current_canvas_, true /* do_save */);
    current_canvas_->clipRect(gfx::RectToSkRect(scissor_rect_));
    current_canvas_->clear(color);
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
  TRACE_EVENT0("viz", "SkiaRenderer::DoDrawQuad");
  if (quad->material == DrawQuad::TILED_CONTENT) {
    AddTileQuadToBatch(TileDrawQuad::MaterialCast(quad), draw_region);
    return;
  }
  // If the current quad is not tiled content then we must flush any
  // bufferred tiled content quads.
  if (!batched_tiles_.empty())
    DrawBatchedTileQuads();

  base::Optional<SkAutoCanvasRestore> auto_canvas_restore;
  const gfx::Rect* scissor_rect =
      is_scissor_enabled_ ? &scissor_rect_ : nullptr;
  gfx::Transform contents_device_transform =
      current_frame()->window_matrix * current_frame()->projection_matrix *
      quad->shared_quad_state->quad_to_target_transform;
  PrepareCanvasForDrawQuads(contents_device_transform, draw_region,
                            scissor_rect, &auto_canvas_restore);

  SkPaint paint;
  if (settings_->force_antialiasing ||
      !IsScaleAndIntegerTranslate(current_canvas_->getTotalMatrix())) {
    // TODO(danakj): Until we can enable AA only on exterior edges of the
    // layer, disable AA if any interior edges are present. crbug.com/248175
    bool all_four_edges_are_exterior =
        quad->IsTopEdge() && quad->IsLeftEdge() && quad->IsBottomEdge() &&
        quad->IsRightEdge();
    if (settings_->allow_antialiasing &&
        (settings_->force_antialiasing || all_four_edges_are_exterior))
      paint.setAntiAlias(true);
    paint.setFilterQuality(kLow_SkFilterQuality);
  }

  paint.setAlpha(quad->shared_quad_state->opacity * 255);
  paint.setBlendMode(
      static_cast<SkBlendMode>(quad->shared_quad_state->blend_mode));

  switch (quad->material) {
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(DebugBorderDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::PICTURE_CONTENT:
      DrawPictureQuad(PictureDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(RenderPassDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::TEXTURE_CONTENT:
      DrawTextureQuad(TextureDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::TILED_CONTENT:
      NOTREACHED();
      break;
    case DrawQuad::SURFACE_CONTENT:
      // Surface content should be fully resolved to other quad types before
      // reaching a direct renderer.
      NOTREACHED();
      break;
    case DrawQuad::YUV_VIDEO_CONTENT:
      DrawYUVVideoQuad(YUVVideoDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::INVALID:
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawUnsupportedQuad(quad, &paint);
      NOTREACHED();
      break;
    case DrawQuad::VIDEO_HOLE:
      // VideoHoleDrawQuad should only be used by Cast, and should
      // have been replaced by cast-specific OverlayProcessor before
      // reach here. In non-cast build, an untrusted render could send such
      // Quad and the quad would then reach here unexpectedly. Therefore
      // we should skip NOTREACHED() so an untrusted render is not capable
      // of causing a crash.
      DrawUnsupportedQuad(quad, &paint);
      break;
  }

  current_canvas_->resetMatrix();
}

bool SkiaRenderer::MustDrawBatchedTileQuads(
    const DrawQuad* new_quad,
    const gfx::Transform& contents_device_transform,
    bool apply_transform_and_scissor,
    const gfx::QuadF* draw_region) {
  DCHECK(new_quad->material == DrawQuad::TILED_CONTENT);
  DCHECK(apply_transform_and_scissor ==
         ApplyTransformAndScissorToTileRect(contents_device_transform));

  if (batched_tiles_.empty())
    return false;

  bool has_draw_region = draw_region != nullptr;

  if (apply_transform_and_scissor) {
    if (!batched_tile_state_.contents_device_transform.IsIdentity())
      return true;
    DCHECK(!batched_tile_state_.has_scissor_rect);
  } else {
    if (batched_tile_state_.contents_device_transform !=
            contents_device_transform ||
        batched_tile_state_.has_scissor_rect != is_scissor_enabled_ ||
        (is_scissor_enabled_ &&
         batched_tile_state_.scissor_rect != scissor_rect_))
      return true;
  }

  if (batched_tile_state_.blend_mode != new_quad->shared_quad_state->blend_mode)
    return true;

  if (batched_tile_state_.has_draw_region != has_draw_region ||
      (has_draw_region && batched_tile_state_.draw_region != *draw_region))
    return true;

  if (TileDrawQuad::MaterialCast(new_quad)->nearest_neighbor !=
      batched_tile_state_.is_nearest_neighbor)
    return true;

  return false;
}

void SkiaRenderer::PrepareCanvasForDrawQuads(
    gfx::Transform contents_device_transform,
    const gfx::QuadF* draw_region,
    const gfx::Rect* scissor_rect,
    base::Optional<SkAutoCanvasRestore>* auto_canvas_restore) {
  if (draw_region || scissor_rect) {
    auto_canvas_restore->emplace(current_canvas_, true /* do_save */);
    if (scissor_rect)
      current_canvas_->clipRect(gfx::RectToSkRect(*scissor_rect));
  }

  contents_device_transform.FlattenTo2d();
  SkMatrix sk_device_matrix;
  gfx::TransformToFlattenedSkMatrix(contents_device_transform,
                                    &sk_device_matrix);
  current_canvas_->setMatrix(sk_device_matrix);

  if (draw_region) {
    SkPath draw_region_clip_path;
    SkPoint clip_points[4];
    QuadFToSkPoints(*draw_region, clip_points);
    draw_region_clip_path.addPoly(clip_points, 4, true);
    current_canvas_->clipPath(draw_region_clip_path);
  }
}

void SkiaRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                                       SkPaint* paint) {
  DCHECK(paint);
  // We need to apply the matrix manually to have pixel-sized stroke width.
  SkPoint vertices[4];
  gfx::RectToSkRect(quad->rect).toQuad(vertices);
  SkPoint transformed_vertices[4];
  current_canvas_->getTotalMatrix().mapPoints(transformed_vertices, vertices,
                                              4);
  current_canvas_->resetMatrix();

  paint->setColor(quad->color);
  paint->setAlpha(quad->shared_quad_state->opacity * SkColorGetA(quad->color));
  paint->setStyle(SkPaint::kStroke_Style);
  paint->setStrokeWidth(quad->width);
  current_canvas_->drawPoints(SkCanvas::kPolygon_PointMode, 4,
                              transformed_vertices, *paint);
}

void SkiaRenderer::DrawPictureQuad(const PictureDrawQuad* quad,
                                   SkPaint* paint) {
  DCHECK(paint);
  SkMatrix content_matrix;
  content_matrix.setRectToRect(gfx::RectFToSkRect(quad->tex_coord_rect),
                               gfx::RectToSkRect(quad->rect),
                               SkMatrix::kFill_ScaleToFit);
  current_canvas_->concat(content_matrix);

  const bool needs_transparency =
      SkScalarRoundToInt(quad->shared_quad_state->opacity * 255) < 255;
  const bool disable_image_filtering =
      disable_picture_quad_image_filtering_ || quad->nearest_neighbor;

  TRACE_EVENT0("viz", "SkiaRenderer::DrawPictureQuad");

  SkCanvas* raster_canvas = current_canvas_;

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

  SkAutoCanvasRestore auto_canvas_restore(raster_canvas, true /* do_save */);
  raster_canvas->translate(-quad->content_rect.x(), -quad->content_rect.y());
  raster_canvas->clipRect(gfx::RectToSkRect(quad->content_rect));
  raster_canvas->scale(quad->contents_scale, quad->contents_scale);
  quad->display_item_list->Raster(raster_canvas);
}

void SkiaRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                                      SkPaint* paint) {
  DCHECK(paint);
  paint->setColor(quad->color);
  paint->setAlpha(quad->shared_quad_state->opacity * SkColorGetA(quad->color));
  current_canvas_->drawRect(gfx::RectToSkRect(quad->visible_rect), *paint);
}

void SkiaRenderer::DrawTextureQuad(const TextureDrawQuad* quad,
                                   SkPaint* paint) {
  DCHECK(paint);
  ScopedSkImageBuilder builder(
      this, quad->resource_id(),
      quad->premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  gfx::RectF visible_uv_rect = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect sk_uv_rect = gfx::RectFToSkRect(visible_uv_rect);
  SkRect quad_rect = gfx::RectToSkRect(quad->visible_rect);

  if (quad->y_flipped) {
    current_canvas_->translate(0, quad_rect.bottom());
    current_canvas_->scale(1, -1);
  }

  bool blend_background =
      quad->background_color != SK_ColorTRANSPARENT && !image->isOpaque();
  bool needs_layer = blend_background && (paint->getAlpha() != 0xFF);
  base::Optional<SkAutoCanvasRestore> auto_canvas_restore;
  if (needs_layer) {
    auto_canvas_restore.emplace(current_canvas_, false /* do_save */);
    current_canvas_->saveLayerAlpha(&quad_rect, paint->getAlpha());
    paint->setAlpha(0xFF);
  }
  if (blend_background) {
    SkPaint background_paint;
    background_paint.setColor(quad->background_color);
    current_canvas_->drawRect(quad_rect, background_paint);
  }
  paint->setFilterQuality(quad->nearest_neighbor ? kNone_SkFilterQuality
                                                 : kLow_SkFilterQuality);
  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, paint);
}

void SkiaRenderer::AddTileQuadToBatch(const TileDrawQuad* quad,
                                      const gfx::QuadF* draw_region) {
  gfx::Transform contents_device_transform =
      current_frame()->window_matrix * current_frame()->projection_matrix *
      quad->shared_quad_state->quad_to_target_transform;
  bool apply_transform_and_scissor =
      ApplyTransformAndScissorToTileRect(contents_device_transform);
  if (MustDrawBatchedTileQuads(quad, contents_device_transform,
                               apply_transform_and_scissor, draw_region))
    DrawBatchedTileQuads();

  if (batched_tiles_.empty()) {
    if (draw_region) {
      batched_tile_state_.draw_region = *draw_region;
    }
    batched_tile_state_.blend_mode = quad->shared_quad_state->blend_mode;
    batched_tile_state_.is_nearest_neighbor = quad->nearest_neighbor;
    batched_tile_state_.has_draw_region = (draw_region != nullptr);
    if (apply_transform_and_scissor) {
      batched_tile_state_.contents_device_transform = gfx::Transform();
      batched_tile_state_.has_scissor_rect = false;
    } else {
      batched_tile_state_.contents_device_transform = contents_device_transform;
      batched_tile_state_.has_scissor_rect = is_scissor_enabled_;
      batched_tile_state_.scissor_rect = scissor_rect_;
    }
  }

  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  ScopedSkImageBuilder builder(
      this, quad->resource_id(),
      quad->is_premultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF visible_tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));

  unsigned aa_flags = SkCanvas::kNone_QuadAAFlags;
  if (settings_->allow_antialiasing || settings_->force_antialiasing) {
    if (quad->IsLeftEdge())
      aa_flags |= SkCanvas::kLeft_QuadAAFlag;
    if (quad->IsTopEdge())
      aa_flags |= SkCanvas::kTop_QuadAAFlag;
    if (quad->IsRightEdge())
      aa_flags |= SkCanvas::kRight_QuadAAFlag;
    if (quad->IsBottomEdge())
      aa_flags |= SkCanvas::kBottom_QuadAAFlag;
  }
  gfx::RectF quad_rect = gfx::RectF(quad->visible_rect);
  if (apply_transform_and_scissor) {
    contents_device_transform.TransformRect(&quad_rect);
    if (is_scissor_enabled_) {
      float left_inset = scissor_rect_.x() - quad_rect.x();
      float top_inset = scissor_rect_.y() - quad_rect.y();
      float right_inset = quad_rect.right() - scissor_rect_.right();
      float bottom_inset = quad_rect.bottom() - scissor_rect_.bottom();
      if (left_inset > 0) {
        aa_flags &= ~SkCanvas::kLeft_QuadAAFlag;
      } else {
        left_inset = 0;
      }
      if (top_inset > 0)
        aa_flags &= ~SkCanvas::kTop_QuadAAFlag;
      else
        top_inset = 0;
      if (right_inset > 0)
        aa_flags &= ~SkCanvas::kRight_QuadAAFlag;
      else
        right_inset = 0;
      if (bottom_inset > 0)
        aa_flags &= ~SkCanvas::kBottom_QuadAAFlag;
      else
        bottom_inset = 0;
      float scale_x = visible_tex_coord_rect.width() / quad_rect.width();
      float scale_y = visible_tex_coord_rect.height() / quad_rect.height();
      quad_rect.Inset(left_inset, top_inset, right_inset, bottom_inset);
      visible_tex_coord_rect.Inset(left_inset * scale_x, top_inset * scale_y,
                                   right_inset * scale_x,
                                   bottom_inset * scale_y);
    }
  }
  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  batched_tiles_.push_back(SkCanvas::ImageSetEntry{
      sk_ref_sp(image), uv_rect, gfx::RectFToSkRect(quad_rect),
      quad->shared_quad_state->opacity, aa_flags});
}

void SkiaRenderer::DrawBatchedTileQuads() {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawBatchedTileQuads");
  const gfx::QuadF* draw_region = batched_tile_state_.has_draw_region
                                      ? &batched_tile_state_.draw_region
                                      : nullptr;
  const gfx::Rect* scissor_rect = batched_tile_state_.has_scissor_rect
                                      ? &batched_tile_state_.scissor_rect
                                      : nullptr;
  base::Optional<SkAutoCanvasRestore> auto_canvas_restore;
  PrepareCanvasForDrawQuads(batched_tile_state_.contents_device_transform,
                            draw_region, scissor_rect, &auto_canvas_restore);

  SkFilterQuality filter_quality = batched_tile_state_.is_nearest_neighbor
                                       ? kNone_SkFilterQuality
                                       : kLow_SkFilterQuality;
  current_canvas_->experimental_DrawImageSetV1(
      &batched_tiles_.front(), batched_tiles_.size(), filter_quality,
      batched_tile_state_.blend_mode);
  current_canvas_->resetMatrix();
  batched_tiles_.clear();
}

void SkiaRenderer::DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                                    SkPaint* paint) {
  DCHECK(paint);
  if (draw_mode_ != DrawMode::DDL) {
    NOTIMPLEMENTED();
    return;
  }

  DCHECK(resource_provider_);
  ScopedYUVSkImageBuilder builder(this, quad);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF visible_tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->ya_tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));

  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  // TODO(penghuang): figure out how to set correct filter quality.
  paint->setFilterQuality(kLow_SkFilterQuality);
  current_canvas_->drawImageRect(image, uv_rect,
                                 gfx::RectToSkRect(quad->visible_rect), paint);
}

bool SkiaRenderer::CalculateRPDQParams(sk_sp<SkImage> content,
                                       const RenderPassDrawQuad* quad,
                                       DrawRenderPassDrawQuadParams* params) {
  if (!params->filters)
    return true;

  DCHECK(!params->filters->IsEmpty());
  auto paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
      *params->filters, gfx::SizeF(content->width(), content->height()));
  auto filter = paint_filter ? paint_filter->cached_sk_filter_ : nullptr;

  // If the first imageFilter is a colorFilterImageFilter, pull it off and store
  // it to be used later. Fall through to allow the remainder of the DAG (if
  // any) to be applied. Applying the colorFilter as part of the final draw is
  // much more efficient than applying it as a colorFilterImageFilter. This
  // optimization would be covered by skbug.com/8700. Delete color_filter
  // related code after Skia side implementation is done.
  if (filter) {
    SkColorFilter* colorfilter_rawptr = nullptr;
    filter->asColorFilter(&colorfilter_rawptr);
    sk_sp<SkColorFilter> color_filter(colorfilter_rawptr);

    if (color_filter) {
      params->color_filter = color_filter;
      filter = sk_ref_sp(filter->getInput(0));
    }
  }

  // If after applying the filter we would be clipped out, skip the draw.
  if (filter) {
    gfx::Rect clip_rect = quad->shared_quad_state->clip_rect;
    if (clip_rect.IsEmpty()) {
      clip_rect = current_draw_rect_;
    }
    gfx::Transform transform =
        quad->shared_quad_state->quad_to_target_transform;
    gfx::QuadF clip_quad = gfx::QuadF(gfx::RectF(clip_rect));
    gfx::QuadF local_clip =
        cc::MathUtil::InverseMapQuadToLocalSpace(transform, clip_quad);

    SkMatrix local_matrix;
    local_matrix.setTranslate(quad->filters_origin.x(),
                              quad->filters_origin.y());
    local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
    gfx::RectF dst_rect(params->filters
                            ? params->filters->MapRect(quad->rect, local_matrix)
                            : quad->rect);

    dst_rect.Intersect(local_clip.BoundingBox());
    // If we've been fully clipped out (by crop rect or clipping), there's
    // nothing to draw.
    if (dst_rect.IsEmpty())
      return false;

    params->image_filter = filter->makeWithLocalMatrix(local_matrix);
  }
  return true;
}

const TileDrawQuad* SkiaRenderer::CanPassBeDrawnDirectly(
    const RenderPass* pass) {
  return DirectRenderer::CanPassBeDrawnDirectly(pass, resource_provider_);
}

void SkiaRenderer::DrawRenderPassQuad(const RenderPassDrawQuad* quad,
                                      SkPaint* paint) {
  DCHECK(paint);
  auto bypass = render_pass_bypass_quads_.find(quad->render_pass_id);
  // When Render Pass has a single quad inside we would draw that directly.
  if (bypass != render_pass_bypass_quads_.end()) {
    TileDrawQuad* tile_quad = &bypass->second;
    ScopedSkImageBuilder builder(this, tile_quad->resource_id(),
                                 tile_quad->is_premultiplied
                                     ? kPremul_SkAlphaType
                                     : kUnpremul_SkAlphaType);
    sk_sp<SkImage> content_image = sk_ref_sp(builder.sk_image());
    DrawRenderPassQuadInternal(quad, content_image, paint);
  } else {
    auto iter = render_pass_backings_.find(quad->render_pass_id);
    DCHECK(render_pass_backings_.end() != iter);
    // This function is called after AllocateRenderPassResourceIfNeeded, so
    // there should be backing ready.
    RenderPassBacking& backing = iter->second;

    sk_sp<SkImage> content_image;
    switch (draw_mode_) {
      case DrawMode::DDL: {
        content_image = skia_output_surface_->MakePromiseSkImageFromRenderPass(
            quad->render_pass_id, backing.size, backing.format,
            backing.generate_mipmap, backing.color_space.ToSkColorSpace());
        break;
      }
      case DrawMode::SKPRECORD: {
        content_image = SkImage::MakeFromPicture(
            backing.picture,
            SkISize::Make(backing.size.width(), backing.size.height()), nullptr,
            nullptr, SkImage::BitDepth::kU8,
            backing.color_space.ToSkColorSpace());
        return;
      }
    }

    // Currently the only trigger for generate_mipmap for render pass is
    // trilinear filtering. It only affects GPU backed implementations and thus
    // requires medium filter quality level.
    if (backing.generate_mipmap)
      paint->setFilterQuality(kMedium_SkFilterQuality);
    DrawRenderPassQuadInternal(quad, content_image, paint);
  }
}

void SkiaRenderer::DrawRenderPassQuadInternal(const RenderPassDrawQuad* quad,
                                              sk_sp<SkImage> content_image,
                                              SkPaint* paint) {
  DCHECK(paint);
  DrawRenderPassDrawQuadParams params;
  params.filters = FiltersForPass(quad->render_pass_id);
  bool can_draw = CalculateRPDQParams(content_image, quad, &params);

  if (!can_draw)
    return;

  // Add color filter.
  if (params.color_filter)
    paint->setColorFilter(params.color_filter);
  // Add image filter.
  if (params.image_filter)
    paint->setImageFilter(params.image_filter);

  SkRect content_rect = RectFToSkRect(quad->tex_coord_rect);
  SkRect dest_visible_rect = gfx::RectToSkRect(quad->visible_rect);

  // Prepare mask.
  ScopedSkImageBuilder mask_image_builder(this, quad->mask_resource_id());
  const SkImage* mask_image = mask_image_builder.sk_image();
  DCHECK_EQ(!!quad->mask_resource_id(), !!mask_image);
  SkMatrix mask_to_dest_matrix;
  sk_sp<SkMaskFilter> mask_filter;
  if (mask_image) {
    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect = gfx::RectFToSkRect(
        gfx::ScaleRect(quad->mask_uv_rect, quad->mask_texture_size.width(),
                       quad->mask_texture_size.height()));
    mask_to_dest_matrix.setRectToRect(mask_rect, gfx::RectToSkRect(quad->rect),
                                      SkMatrix::kFill_ScaleToFit);
    mask_filter =
        SkShaderMaskFilter::Make(mask_image->makeShader(&mask_to_dest_matrix));
    DCHECK(mask_filter);
  }

  const cc::FilterOperations* backdrop_filters =
      BackgroundFiltersForPass(quad->render_pass_id);
  // Without backdrop effect.
  if (!ShouldApplyBackgroundFilters(quad, backdrop_filters)) {
    if (mask_filter)
      paint->setMaskFilter(mask_filter);

    // Draw now that all the filters are set up correctly.
    current_canvas_->drawImageRect(content_image, content_rect,
                                   dest_visible_rect, paint);
    return;
  }

  // Draw render pass with backdrop effects.
  // Update the backdrop filter to include "regular" filters and opacity.
  cc::FilterOperations backdrop_filters_plus_effects = *backdrop_filters;
  if (params.filters) {
    for (const auto& filter_op : params.filters->operations())
      backdrop_filters_plus_effects.Append(filter_op);
  }
  if (quad->shared_quad_state->opacity < 1.0) {
    backdrop_filters_plus_effects.Append(
        cc::FilterOperation::CreateOpacityFilter(
            quad->shared_quad_state->opacity));
  }

  auto background_paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
      backdrop_filters_plus_effects,
      gfx::SizeF(content_image->width(), content_image->height()));
  auto background_image_filter =
      background_paint_filter ? background_paint_filter->cached_sk_filter_
                              : nullptr;
  DCHECK(background_image_filter);
  SkMatrix content_to_dest_matrix;
  content_to_dest_matrix.setRectToRect(
      content_rect, gfx::RectToSkRect(quad->rect), SkMatrix::kFill_ScaleToFit);
  SkMatrix local_matrix;
  local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
  local_matrix.postConcat(content_to_dest_matrix);
  background_image_filter =
      background_image_filter->makeWithLocalMatrix(local_matrix);

  SkAutoCanvasRestore auto_canvas_restore(current_canvas_, true /* do_save */);
  current_canvas_->clipRect(gfx::RectToSkRect(quad->rect));

  SkPaint tmp_paint;
  tmp_paint.setMaskFilter(mask_filter);
  SkCanvas::SaveLayerRec rec(&dest_visible_rect, &tmp_paint,
                             background_image_filter.get(),
                             SkCanvas::kInitWithPrevious_SaveLayerFlag);
  // Lift content in the current_canvas_ into a new layer with
  // background_image_filter, and then paint content_image in the layer,
  // and then the current_canvas_->restore() will drop the layer into the
  // canvas.
  SkAutoCanvasRestore auto_canvas_restore_for_save_layer(current_canvas_,
                                                         false /* do_save */);
  current_canvas_->saveLayer(rec);
  // TODO(916317): need to draw the backdrop once first here. Also need to
  // pull the backdrop-filter bounds rect in and clip the backdrop image.
  current_canvas_->drawImageRect(content_image, content_rect, dest_visible_rect,
                                 paint);
}

void SkiaRenderer::DrawUnsupportedQuad(const DrawQuad* quad, SkPaint* paint) {
  DCHECK(paint);
  // TODO(weiliangc): Make sure unsupported quads work. (crbug.com/644851)
  NOTIMPLEMENTED();
#ifdef NDEBUG
  paint->setColor(SK_ColorWHITE);
#else
  paint->setColor(SK_ColorMAGENTA);
#endif
  paint->setAlpha(quad->shared_quad_state->opacity * 255);
  current_canvas_->drawRect(gfx::RectToSkRect(quad->rect), *paint);
}

void SkiaRenderer::CopyDrawnRenderPass(
    const copy_output::RenderPassGeometry& geometry,
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(weiliangc): Make copy request work. (crbug.com/644851)
  TRACE_EVENT0("viz", "SkiaRenderer::CopyDrawnRenderPass");

  switch (draw_mode_) {
    case DrawMode::DDL: {
      // Root framebuffer uses id 0 in SkiaOutputSurface.
      RenderPassId render_pass_id = 0;
      const auto* const render_pass = current_frame()->current_render_pass;
      if (render_pass != current_frame()->root_render_pass) {
        render_pass_id = render_pass->id;
      }
      skia_output_surface_->CopyOutput(render_pass_id, geometry,
                                       render_pass->color_space,
                                       std::move(request));
      break;
    }
    case DrawMode::SKPRECORD: {
      NOTIMPLEMENTED();
      break;
    }
  }
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
  if (!batched_tiles_.empty())
    DrawBatchedTileQuads();
  switch (draw_mode_) {
    case DrawMode::DDL: {
      // Skia doesn't support releasing the last promise image ref on the DDL
      // recordering thread. So we clear all cached promise images before
      // SubmitPaint to the GPU thread.
      promise_images_.clear();
      yuv_promise_images_.clear();
      gpu::SyncToken sync_token = skia_output_surface_->SubmitPaint();
      lock_set_for_external_use_->UnlockResources(sync_token);
      break;
    }
    case DrawMode::SKPRECORD: {
      current_canvas_->flush();
      sk_sp<SkPicture> picture = current_recorder_->finishRecordingAsPicture();
      *current_picture_ = picture;
    }
  }
}

void SkiaRenderer::GenerateMipmap() {
  // This is a no-op since setting FilterQuality to high during drawing of
  // RenderPassDrawQuad is what actually generates generate_mipmap.
}

bool SkiaRenderer::ShouldApplyBackgroundFilters(
    const RenderPassDrawQuad* quad,
    const cc::FilterOperations* backdrop_filters) const {
  if (!backdrop_filters)
    return false;
  DCHECK(!backdrop_filters->IsEmpty());
  return true;
}

GrContext* SkiaRenderer::GetGrContext() {
  switch (draw_mode_) {
    case DrawMode::DDL:
      return nullptr;
    case DrawMode::SKPRECORD:
      return nullptr;
  }
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
    bool size_appropriate = backing.size.width() >= requirements.size.width() &&
                            backing.size.height() >= requirements.size.height();
    bool mipmap_appropriate =
        !requirements.generate_mipmap || backing.generate_mipmap;
    if (!size_appropriate || !mipmap_appropriate)
      passes_to_delete.push_back(pair.first);
  }

  // Delete RenderPass backings from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i) {
    auto it = render_pass_backings_.find(passes_to_delete[i]);
    render_pass_backings_.erase(it);
  }

  if (is_using_ddl() && !passes_to_delete.empty()) {
    skia_output_surface_->RemoveRenderPassResource(std::move(passes_to_delete));
  }
}

void SkiaRenderer::AllocateRenderPassResourceIfNeeded(
    const RenderPassId& render_pass_id,
    const RenderPassRequirements& requirements) {
  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end())
    return;

  // TODO(penghuang): check supported format correctly.
  gpu::Capabilities caps;
  caps.texture_format_bgra8888 = true;
  GrContext* gr_context = GetGrContext();
  switch (draw_mode_) {
    case DrawMode::DDL:
      break;
    case DrawMode::SKPRECORD: {
      render_pass_backings_.emplace(
          render_pass_id,
          RenderPassBacking(requirements.size, requirements.generate_mipmap,
                            current_frame()->current_render_pass->color_space));
      return;
    }
  }
  render_pass_backings_.emplace(
      render_pass_id,
      RenderPassBacking(gr_context, caps, requirements.size,
                        requirements.generate_mipmap,
                        current_frame()->current_render_pass->color_space));
}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    GrContext* gr_context,
    const gpu::Capabilities& caps,
    const gfx::Size& size,
    bool generate_mipmap,
    const gfx::ColorSpace& color_space)
    : size(size), generate_mipmap(generate_mipmap), color_space(color_space) {
  if (color_space.IsHDR()) {
    // If a platform does not support half-float renderbuffers then it should
    // not should request HDR rendering.
    // DCHECK(caps.texture_half_float_linear);
    // DCHECK(caps.color_buffer_half_float_rgba);
    format = RGBA_F16;
  } else {
    format = PlatformColor::BestSupportedTextureFormat(caps);
  }

  // For DDL, we don't need create teh render_pass_surface here, and we will
  // create the SkSurface by SkiaOutputSurface on Gpu thread.
  if (!gr_context)
    return;

  constexpr uint32_t flags = 0;
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  int msaa_sample_count = 0;
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing*/, format);
  SkImageInfo image_info =
      SkImageInfo::Make(size.width(), size.height(), color_type,
                        kPremul_SkAlphaType, color_space.ToSkColorSpace());
  render_pass_surface = SkSurface::MakeRenderTarget(
      gr_context, SkBudgeted::kNo, image_info, msaa_sample_count,
      kTopLeft_GrSurfaceOrigin, &surface_props, generate_mipmap);
}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    const gfx::Size& size,
    bool generate_mipmap,
    const gfx::ColorSpace& color_space)
    : size(size), generate_mipmap(generate_mipmap), color_space(color_space) {
  recorder = std::make_unique<SkPictureRecorder>();
}

SkiaRenderer::RenderPassBacking::~RenderPassBacking() {}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    SkiaRenderer::RenderPassBacking&& other)
    : size(other.size),
      generate_mipmap(other.generate_mipmap),
      color_space(other.color_space),
      format(other.format) {
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
  recorder = std::move(other.recorder);
}

SkiaRenderer::RenderPassBacking& SkiaRenderer::RenderPassBacking::operator=(
    SkiaRenderer::RenderPassBacking&& other) {
  size = other.size;
  generate_mipmap = other.generate_mipmap;
  color_space = other.color_space;
  format = other.format;
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
  recorder = std::move(other.recorder);
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
  return it->second.size;
}

}  // namespace viz
