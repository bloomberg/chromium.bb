// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include <string>
#include <utility>

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
#include "components/viz/common/quads/stream_video_draw_quad.h"
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
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/effects/SkShaderMaskFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/src/core/SkColorFilterPriv.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace viz {

namespace {

// Smallest unit that impacts anti-aliasing output. We use this to determine
// when an exterior edge (with AA) has been clipped (no AA). The specific value
// was chosen to match that used by gl_renderer.
static const float kAAEpsilon = 1.0f / 1024.0f;

// The gfx::QuadF draw_region passed to DoDrawQuad, converted to Skia types
struct SkDrawRegion {
  SkDrawRegion() = default;
  explicit SkDrawRegion(const gfx::QuadF& draw_region);

  SkPoint points[4];
};

SkDrawRegion::SkDrawRegion(const gfx::QuadF& draw_region) {
  points[0] = gfx::PointFToSkPoint(draw_region.p1());
  points[1] = gfx::PointFToSkPoint(draw_region.p2());
  points[2] = gfx::PointFToSkPoint(draw_region.p3());
  points[3] = gfx::PointFToSkPoint(draw_region.p4());
}

bool IsTextureResource(DisplayResourceProvider* resource_provider,
                       ResourceId resource_id) {
  return !resource_provider->IsResourceSoftwareBacked(resource_id);
}

bool CanExplicitlyScissor(const DrawQuad* quad,
                          const gfx::QuadF* draw_region,
                          const gfx::Transform& contents_device_transform) {
  // PICTURE_CONTENT is not like the others, since it is executing a list of
  // draw calls into the canvas.
  if (quad->material == DrawQuad::PICTURE_CONTENT)
    return false;
  // Intersection with scissor and a quadrilateral is not necessarily a quad,
  // so don't complicate things
  if (draw_region)
    return false;

  // This is slightly different than
  // gfx::Transform::IsPositiveScaleAndTranslation in that it also allows zero
  // scales. This is because in the common orthographic case the z scale is 0.
  if (!contents_device_transform.IsScaleOrTranslation())
    return false;

  return contents_device_transform.matrix().get(0, 0) >= 0.0 &&
         contents_device_transform.matrix().get(1, 1) >= 0.0 &&
         contents_device_transform.matrix().get(2, 2) >= 0.0;
}

void ApplyExplicitScissor(const DrawQuad* quad,
                          const gfx::Rect& scissor_rect,
                          const gfx::Transform& device_transform,
                          unsigned* aa_flags,
                          gfx::RectF* vis_rect) {
  // Inset rectangular edges and turn off the AA for clipped edges. Operates in
  // the quad's space, so apply inverse of transform to get new scissor
  gfx::RectF scissor(scissor_rect);
  device_transform.TransformRectReverse(&scissor);

  float left_inset = scissor.x() - vis_rect->x();
  float top_inset = scissor.y() - vis_rect->y();
  float right_inset = vis_rect->right() - scissor.right();
  float bottom_inset = vis_rect->bottom() - scissor.bottom();

  if (left_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kLeft_QuadAAFlag;
  } else {
    left_inset = 0;
  }
  if (top_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kTop_QuadAAFlag;
  } else {
    top_inset = 0;
  }
  if (right_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kRight_QuadAAFlag;
  } else {
    right_inset = 0;
  }
  if (bottom_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kBottom_QuadAAFlag;
  } else {
    bottom_inset = 0;
  }

  vis_rect->Inset(left_inset, top_inset, right_inset, bottom_inset);
}

unsigned GetCornerAAFlags(const DrawQuad* quad,
                          const SkPoint& vertex,
                          unsigned edge_mask) {
  // Returns mask of SkCanvas::QuadAAFlags, with bits set for each edge of the
  // shared quad state's quad_layer_rect that vertex is touching.

  unsigned mask = SkCanvas::kNone_QuadAAFlags;
  if (std::abs(vertex.x()) < kAAEpsilon)
    mask |= SkCanvas::kLeft_QuadAAFlag;
  if (std::abs(vertex.x() - quad->shared_quad_state->quad_layer_rect.width()) <
      kAAEpsilon)
    mask |= SkCanvas::kRight_QuadAAFlag;
  if (std::abs(vertex.y()) < kAAEpsilon)
    mask |= SkCanvas::kTop_QuadAAFlag;
  if (std::abs(vertex.y() - quad->shared_quad_state->quad_layer_rect.height()) <
      kAAEpsilon)
    mask |= SkCanvas::kBottom_QuadAAFlag;
  // & with the overall edge_mask to take into account edges that were clipped
  // by the visible rect.
  return mask & edge_mask;
}

bool IsExteriorEdge(unsigned corner_mask1, unsigned corner_mask2) {
  return (corner_mask1 & corner_mask2) != 0;
}

unsigned GetRectilinearEdgeFlags(const DrawQuad* quad) {
  // In the normal case, turn on AA for edges that represent the outside of
  // the layer, and that aren't clipped by the visible rect.
  unsigned mask = SkCanvas::kNone_QuadAAFlags;
  if (quad->IsLeftEdge() &&
      std::abs(quad->rect.x() - quad->visible_rect.x()) < kAAEpsilon)
    mask |= SkCanvas::kLeft_QuadAAFlag;
  if (quad->IsTopEdge() &&
      std::abs(quad->rect.y() - quad->visible_rect.y()) < kAAEpsilon)
    mask |= SkCanvas::kTop_QuadAAFlag;
  if (quad->IsRightEdge() &&
      std::abs(quad->rect.right() - quad->visible_rect.right()) < kAAEpsilon)
    mask |= SkCanvas::kRight_QuadAAFlag;
  if (quad->IsBottomEdge() &&
      std::abs(quad->rect.bottom() - quad->visible_rect.bottom()) < kAAEpsilon)
    mask |= SkCanvas::kBottom_QuadAAFlag;

  return mask;
}

// This also modifies draw_region to clean up any degeneracies
void GetClippedEdgeFlags(const DrawQuad* quad,
                         unsigned* edge_mask,
                         SkDrawRegion* draw_region) {
  // Instead of trying to rotate vertices of draw_region to align with Skia's
  // edge label conventions, turn on an edge's label if it is aligned to any
  // exterior edge.
  unsigned p0Mask = GetCornerAAFlags(quad, draw_region->points[0], *edge_mask);
  unsigned p1Mask = GetCornerAAFlags(quad, draw_region->points[1], *edge_mask);
  unsigned p2Mask = GetCornerAAFlags(quad, draw_region->points[2], *edge_mask);
  unsigned p3Mask = GetCornerAAFlags(quad, draw_region->points[3], *edge_mask);

  unsigned mask = SkCanvas::kNone_QuadAAFlags;
  // The "top" is p0 to p1
  if (IsExteriorEdge(p0Mask, p1Mask))
    mask |= SkCanvas::kTop_QuadAAFlag;
  // The "right" is p1 to p2
  if (IsExteriorEdge(p1Mask, p2Mask))
    mask |= SkCanvas::kRight_QuadAAFlag;
  // The "bottom" is p2 to p3
  if (IsExteriorEdge(p2Mask, p3Mask))
    mask |= SkCanvas::kBottom_QuadAAFlag;
  // The "left" is p3 to p0
  if (IsExteriorEdge(p3Mask, p0Mask))
    mask |= SkCanvas::kLeft_QuadAAFlag;

  // If the clipped draw_region has adjacent non-AA edges that touch the
  // exterior edge (which should be AA'ed), move the degenerate vertex to the
  // appropriate index so that Skia knows to construct a coverage ramp at that
  // corner. This is not an ideal solution, but is the best hint we can give,
  // given our limited information post-BSP splitting.
  if (draw_region->points[2] == draw_region->points[3]) {
    // The BSP splitting always creates degenerate quads with the duplicate
    // vertex in the last two indices.
    if (p0Mask && !(mask & SkCanvas::kLeft_QuadAAFlag) &&
        !(mask & SkCanvas::kTop_QuadAAFlag)) {
      // Rewrite draw_region from p0,p1,p2,p2 to p0,p1,p2,p0; top edge stays off
      // right edge is preserved, bottom edge turns off, left edge turns on
      draw_region->points[3] = draw_region->points[0];
      mask = SkCanvas::kLeft_QuadAAFlag | (mask & SkCanvas::kRight_QuadAAFlag);
    } else if (p1Mask && !(mask & SkCanvas::kTop_QuadAAFlag) &&
               !(mask & SkCanvas::kRight_QuadAAFlag)) {
      // Rewrite draw_region to p0,p1,p1,p2; top edge stays off, right edge
      // turns on, bottom edge turns off, left edge is preserved
      draw_region->points[2] = draw_region->points[1];
      mask = SkCanvas::kRight_QuadAAFlag | (mask & SkCanvas::kLeft_QuadAAFlag);
    }
    // p2 could follow the same process, but if its adjacent edges are AA
    // (skipping the degenerate edge to p3), it's actually already in the
    // desired vertex ordering; and since p3 is in the same location, it's
    // equivalent to p2 so it doesn't need checking either.
  }  // Else not degenerate, so can't to correct non-AA corners touching AA edge

  *edge_mask = mask;
}

bool IsAAForcedOff(const DrawQuad* quad) {
  switch (quad->material) {
    case DrawQuad::PICTURE_CONTENT:
      return PictureDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::RENDER_PASS:
      return RenderPassDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::SOLID_COLOR:
      return SolidColorDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::TILED_CONTENT:
      return TileDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    default:
      return false;
  }
}

SkFilterQuality GetFilterQuality(const DrawQuad* quad) {
  bool nearest_neighbor;
  switch (quad->material) {
    case DrawQuad::PICTURE_CONTENT:
      nearest_neighbor = PictureDrawQuad::MaterialCast(quad)->nearest_neighbor;
      break;
    case DrawQuad::TEXTURE_CONTENT:
      nearest_neighbor = TextureDrawQuad::MaterialCast(quad)->nearest_neighbor;
      break;
    case DrawQuad::TILED_CONTENT:
      nearest_neighbor = TileDrawQuad::MaterialCast(quad)->nearest_neighbor;
      break;
    default:
      // Other quad types do not expose filter quality, so default to bilinear
      // TODO(penghuang): figure out how to set correct filter quality for YUV
      // and video stream quads.
      // TODO(michaelludwig): use kMedium for RPDQs that ask for mipmaps
      nearest_neighbor = false;
      break;
  }

  return nearest_neighbor ? kNone_SkFilterQuality : kLow_SkFilterQuality;
}

// Return a color filter that multiplies the incoming color by the fixed alpha
sk_sp<SkColorFilter> MakeOpacityFilter(float alpha, sk_sp<SkColorFilter> in) {
  SkColor alpha_as_color = SkColorSetA(SK_ColorWHITE, 255 * alpha);
  // MakeModeFilter treats fixed color as src, and input color as dst.
  // kDstIn is (srcAlpha * dstColor, srcAlpha * dstAlpha) so this makes the
  // output color equal to input color * alpha.
  sk_sp<SkColorFilter> opacity =
      SkColorFilter::MakeModeFilter(alpha_as_color, SkBlendMode::kDstIn);
  if (in) {
    return opacity->makeComposed(std::move(in));
  } else {
    return opacity;
  }
}

}  // namespace

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

// State calculated from a DrawQuad and current renderer state, that is common
// to all DrawQuad rendering.
struct SkiaRenderer::DrawQuadParams {
  DrawQuadParams() = default;
  DrawQuadParams(const gfx::Transform& cdt,
                 const gfx::RectF visible_rect,
                 unsigned aa_flags,
                 SkBlendMode blend_mode,
                 float opacity,
                 SkFilterQuality filter_quality,
                 const gfx::QuadF* draw_region,
                 bool enable_scissor_rect);

  // window_matrix * projection_matrix * quad_to_target_transform
  gfx::Transform content_device_transform;
  // The DrawQuad's visible_rect, possibly explicitly clipped by the scissor
  gfx::RectF visible_rect;
  // SkCanvas::QuadAAFlags, already taking into account settings
  // (but not certain quad type's force_antialias_off bit)
  unsigned aa_flags;
  // Final blend mode to use, respecting quad settings + opacity optimizations
  SkBlendMode blend_mode;
  // Final opacity of quad
  float opacity;
  // Resolved filter quality from quad settings
  SkFilterQuality filter_quality;
  // Optional restricted draw geometry, will point to a length 4 SkPoint array
  // with its points in CW order matching Skia's vertex/edge expectations.
  base::Optional<SkDrawRegion> draw_region;
  // True if renderer's scissor_rect_ should be used as a clipRect on the
  // canvas. Is false if is_scissor_enabled_ is false or the scissor was
  // explicitly applied to the visible geometry already.
  bool has_scissor_rect;

  SkPaint paint() const {
    SkPaint p;
    p.setFilterQuality(filter_quality);
    p.setBlendMode(blend_mode);
    p.setAlphaf(opacity);
    p.setAntiAlias(aa_flags != SkCanvas::kNone_QuadAAFlags);
    return p;
  }
};

SkiaRenderer::DrawQuadParams::DrawQuadParams(const gfx::Transform& cdt,
                                             const gfx::RectF visible_rect,
                                             unsigned aa_flags,
                                             SkBlendMode blend_mode,
                                             float opacity,
                                             SkFilterQuality filter_quality,
                                             const gfx::QuadF* draw_region,
                                             bool has_scissor_rect)
    : content_device_transform(cdt),
      visible_rect(visible_rect),
      aa_flags(aa_flags),
      blend_mode(blend_mode),
      opacity(opacity),
      filter_quality(filter_quality),
      has_scissor_rect(has_scissor_rect) {
  if (draw_region) {
    this->draw_region.emplace(*draw_region);
  }
}

// Scoped helper class for building SkImage from resource id.
class SkiaRenderer::ScopedSkImageBuilder {
 public:
  ScopedSkImageBuilder(SkiaRenderer* skia_renderer,
                       ResourceId resource_id,
                       SkAlphaType alpha_type = kPremul_SkAlphaType,
                       GrSurfaceOrigin origin = kTopLeft_GrSurfaceOrigin);
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
    SkAlphaType alpha_type,
    GrSurfaceOrigin origin) {
  if (!resource_id)
    return;
  auto* resource_provider = skia_renderer->resource_provider_;
  if (!skia_renderer->is_using_ddl() || skia_renderer->non_root_surface_ ||
      !IsTextureResource(resource_provider, resource_id)) {
    // TODO(penghuang): remove this code when DDL is used everywhere.
    lock_.emplace(resource_provider, resource_id, alpha_type, origin);
    sk_image_ = lock_->sk_image();
  } else {
    // Look up the image from promise_images_by resource_id and return the
    // reference. If the resource_id doesn't exist, this statement will
    // allocate it and return reference of it, and the reference will be used
    // to store the new created image later.
    auto& image = skia_renderer->promise_images_[resource_id];
    if (!image) {
      image =
          skia_renderer->lock_set_for_external_use_
              ->LockResourceAndCreateSkImage(resource_id, alpha_type, origin);
      LOG_IF(ERROR, !image) << "Failed to create the promise sk image.";
    }
    sk_image_ = image.get();
  }
}

class SkiaRenderer::ScopedYUVSkImageBuilder {
 public:
  ScopedYUVSkImageBuilder(SkiaRenderer* skia_renderer,
                          const YUVVideoDrawQuad* quad,
                          sk_sp<SkColorSpace> dst_color_space,
                          bool has_color_conversion_filter) {
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
      SkYUVColorSpace yuv_color_space;
      if (has_color_conversion_filter) {
        yuv_color_space = kIdentity_SkYUVColorSpace;
      } else {
        yuv_color_space = kRec601_SkYUVColorSpace;
        quad->video_color_space.ToSkYUVColorSpace(&yuv_color_space);
      }

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
          std::move(metadatas), yuv_color_space, dst_color_space, has_alpha);
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
  if (draw_mode_ == DrawMode::DDL)
    return output_surface_->capabilities().supports_post_sub_buffer;

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
  DrawQuadParams params = CalculateDrawQuadParams(quad, draw_region);
  if (MustFlushBatchedQuads(quad, params)) {
    FlushBatchedQuads();
  }

  // TODO(michaelludwig): By the end of the Skia API update, this switch will
  // hold all quad draw types and resemble the old DoDrawSingleQuad.
  switch (quad->material) {
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(DebugBorderDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawStreamVideoQuad(StreamVideoDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::TEXTURE_CONTENT:
      DrawTextureQuad(TextureDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::TILED_CONTENT:
      DrawTileDrawQuad(TileDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::YUV_VIDEO_CONTENT:
      DrawYUVVideoQuad(YUVVideoDrawQuad::MaterialCast(quad), params);
      break;
    default:
      // If we've reached here, the quad's type hasn't been updated to be
      // batch aware yet.
      DoSingleDrawQuad(quad, draw_region);
      break;
  }
}

void SkiaRenderer::PrepareCanvas(const gfx::Rect* scissor_rect,
                                 const gfx::Transform* cdt) {
  // Scissor is applied in the device space (CTM == I) and since no changes
  // to the canvas persist, CTM should already be the identity
  DCHECK(current_canvas_->getTotalMatrix() == SkMatrix::I());

  if (scissor_rect) {
    current_canvas_->clipRect(gfx::RectToSkRect(*scissor_rect));
  }

  if (cdt) {
    SkMatrix m;
    gfx::TransformToFlattenedSkMatrix(*cdt, &m);
    current_canvas_->concat(m);
  }
}

SkiaRenderer::DrawQuadParams SkiaRenderer::CalculateDrawQuadParams(
    const DrawQuad* quad,
    const gfx::QuadF* draw_region) {
  DrawQuadParams params(
      current_frame()->window_matrix * current_frame()->projection_matrix *
          quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->visible_rect), SkCanvas::kNone_QuadAAFlags,
      quad->shared_quad_state->blend_mode, quad->shared_quad_state->opacity,
      GetFilterQuality(quad), draw_region, is_scissor_enabled_);

  params.content_device_transform.FlattenTo2d();

  // Respect per-quad setting overrides as highest priority setting
  if (!IsAAForcedOff(quad)) {
    if (settings_->force_antialiasing) {
      // This setting makes the entire draw AA, so don't bother checking edges
      params.aa_flags = SkCanvas::kAll_QuadAAFlags;
    } else if (settings_->allow_antialiasing) {
      params.aa_flags = GetRectilinearEdgeFlags(quad);
      if (draw_region && params.aa_flags != SkCanvas::kNone_QuadAAFlags) {
        // Turn off interior edges' AA from the BSP splitting
        GetClippedEdgeFlags(quad, &params.aa_flags, &*params.draw_region);
      }
    }
  }

  if (!quad->ShouldDrawWithBlending()) {
    // The quad layer is src-over with 1.0 opacity and its needs_blending flag
    // has been set to false. However, even if the layer's opacity is 1.0, the
    // contents may not be (e.g. png or a color with alpha).
    if (quad->shared_quad_state->are_contents_opaque) {
      // Visually, this is the same as kSrc but Skia is faster with SrcOver
      params.blend_mode = SkBlendMode::kSrcOver;
    } else {
      // Replaces dst contents with the new color (e.g. no blending); this is
      // just as fast as srcover when there's no AA, but is slow when coverage
      // must be taken into account.
      params.blend_mode = SkBlendMode::kSrc;
    }
    params.opacity = 1.f;
  }

  // Applying the scissor explicitly means avoiding a clipRect() call and
  // allows more quads to be batched together in a DrawEdgeAAImageSet call
  if (is_scissor_enabled_ &&
      CanExplicitlyScissor(quad, draw_region,
                           params.content_device_transform)) {
    ApplyExplicitScissor(quad, scissor_rect_, params.content_device_transform,
                         &params.aa_flags, &params.visible_rect);
    params.has_scissor_rect = false;
  }

  return params;
}

SkCanvas::ImageSetEntry SkiaRenderer::MakeEntry(const DrawQuadParams& params,
                                                const SkImage* image,
                                                const gfx::RectF& src,
                                                int matrix_index,
                                                bool use_opacity) {
  return SkCanvas::ImageSetEntry(
      {sk_ref_sp(image), gfx::RectFToSkRect(src),
       gfx::RectFToSkRect(params.visible_rect), matrix_index,
       use_opacity ? params.opacity : 1.f, params.aa_flags,
       params.draw_region.has_value()});
}

bool SkiaRenderer::MustFlushBatchedQuads(const DrawQuad* new_quad,
                                         const DrawQuadParams& params) {
  if (batched_quads_.empty())
    return false;

  // TODO(michaelludwig) - Once other quad types are migrated from
  // DoDrawQuadLegacy, this check will be widened
  if (new_quad->material != DrawQuad::STREAM_VIDEO_CONTENT &&
      new_quad->material != DrawQuad::TEXTURE_CONTENT &&
      new_quad->material != DrawQuad::TILED_CONTENT)
    return true;

  if (batched_quad_state_.blend_mode != params.blend_mode ||
      batched_quad_state_.filter_quality != params.filter_quality)
    return true;

  bool no_scissor =
      !batched_quad_state_.has_scissor_rect && !params.has_scissor_rect;
  bool same_scissor = batched_quad_state_.has_scissor_rect &&
                      params.has_scissor_rect &&
                      batched_quad_state_.scissor_rect == scissor_rect_;

  if (!no_scissor && !same_scissor)
    return true;

  return false;
}

void SkiaRenderer::AddQuadToBatch(const DrawQuadParams& params,
                                  const SkImage* image,
                                  const gfx::RectF& tex_coords) {
  // Configure batch state if it's the first
  if (batched_quads_.empty()) {
    if (params.has_scissor_rect) {
      batched_quad_state_.scissor_rect = scissor_rect_;
      batched_quad_state_.has_scissor_rect = true;
    } else {
      batched_quad_state_.has_scissor_rect = false;
    }

    batched_quad_state_.blend_mode = params.blend_mode;
    batched_quad_state_.filter_quality = params.filter_quality;
  }

  // Add entry, with optional clip quad and shared transform
  if (params.draw_region.has_value()) {
    for (int i = 0; i < 4; ++i) {
      batched_draw_regions_.push_back(params.draw_region->points[i]);
    }
  }

  SkMatrix m;
  gfx::TransformToFlattenedSkMatrix(params.content_device_transform, &m);
  std::vector<SkMatrix>& cdts = batched_cdt_matrices_;
  if (cdts.empty() || cdts[cdts.size() - 1] != m) {
    cdts.push_back(m);
  }
  int matrix_index = cdts.size() - 1;

  batched_quads_.push_back(MakeEntry(params, image, tex_coords, matrix_index));
}

void SkiaRenderer::FlushBatchedQuads() {
  TRACE_EVENT0("viz", "SkiaRenderer::FlushBatchedQuads");

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(batched_quad_state_.has_scissor_rect
                    ? &batched_quad_state_.scissor_rect
                    : nullptr,
                nullptr);

  SkPaint paint;
  paint.setFilterQuality(batched_quad_state_.filter_quality);
  paint.setBlendMode(batched_quad_state_.blend_mode);
  current_canvas_->experimental_DrawEdgeAAImageSet(
      &batched_quads_.front(), batched_quads_.size(),
      &batched_draw_regions_.front(), &batched_cdt_matrices_.front(), &paint,
      SkCanvas::kFast_SrcRectConstraint);

  batched_quads_.clear();
  batched_draw_regions_.clear();
  batched_cdt_matrices_.clear();
}

void SkiaRenderer::DrawColoredQuad(const DrawQuadParams& params,
                                   SkColor color) {
  DCHECK(batched_quads_.empty());
  TRACE_EVENT0("viz", "SkiaRenderer::DrawColoredQuad");

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(params.has_scissor_rect ? &scissor_rect_ : nullptr,
                &params.content_device_transform);

  color = SkColorSetA(color, params.opacity * SkColorGetA(color));
  const SkPoint* draw_region =
      params.draw_region.has_value() ? params.draw_region->points : nullptr;
  current_canvas_->experimental_DrawEdgeAAQuad(
      gfx::RectFToSkRect(params.visible_rect), draw_region,
      static_cast<SkCanvas::QuadAAFlags>(params.aa_flags), color,
      params.blend_mode);
}

void SkiaRenderer::DrawSingleImage(const DrawQuadParams& params,
                                   const SkImage* image,
                                   const gfx::RectF& src,
                                   const SkPaint* paint) {
  DCHECK(batched_quads_.empty());
  TRACE_EVENT0("viz", "SkiaRenderer::DrawSingleImage");

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(params.has_scissor_rect ? &scissor_rect_ : nullptr,
                &params.content_device_transform);
  // Use -1 for matrix index since the cdt is set on the canvas.
  // Assume params.opacity has been somehow handled in the SkPaint
  // (setAlphaf, color filter, image filter node, etc.).
  SkCanvas::ImageSetEntry entry =
      MakeEntry(params, image, src, -1, false /* use params.opacity */);
  const SkPoint* draw_region =
      params.draw_region.has_value() ? params.draw_region->points : nullptr;
  current_canvas_->experimental_DrawEdgeAAImageSet(&entry, 1, draw_region,
                                                   nullptr, paint);
}

void SkiaRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                                       const DrawQuadParams& params) {
  DCHECK(batched_quads_.empty());

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  // We need to apply the matrix manually to have pixel-sized stroke width.
  PrepareCanvas(params.has_scissor_rect ? &scissor_rect_ : nullptr, nullptr);
  SkMatrix cdt;
  gfx::TransformToFlattenedSkMatrix(params.content_device_transform, &cdt);

  SkPath path;
  if (params.draw_region.has_value()) {
    path.addPoly(params.draw_region->points, 4, true /* close */);
  } else {
    path.addRect(gfx::RectFToSkRect(params.visible_rect));
  }
  path.transform(cdt);

  SkPaint paint = params.paint();
  paint.setColor(quad->color);  // Must correct alpha afterwards
  paint.setAlphaf(params.opacity * paint.getAlphaf());
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeJoin(SkPaint::kMiter_Join);
  paint.setStrokeWidth(quad->width);
  current_canvas_->drawPath(path, paint);
}

void SkiaRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                                      const DrawQuadParams& params) {
  DrawColoredQuad(params, quad->color);
}

void SkiaRenderer::DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                                       const DrawQuadParams& params) {
  DCHECK(!MustFlushBatchedQuads(quad, params));
  ScopedSkImageBuilder builder(this, quad->resource_id(),
                               kUnpremul_SkAlphaType);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;

  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  gfx::RectF visible_uv_rect = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), params.visible_rect);
  AddQuadToBatch(params, image, visible_uv_rect);
}

void SkiaRenderer::DrawTextureQuad(const TextureDrawQuad* quad,
                                   const DrawQuadParams& params) {
  ScopedSkImageBuilder builder(
      this, quad->resource_id(),
      quad->premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      quad->y_flipped ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  gfx::RectF visible_uv_rect = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), params.visible_rect);

  // There are two scenarios where a texture quad cannot be put into a batch:
  // 1. It needs to be blended with a constant background color.
  // 2. The vertex opacities are not all 1s.
  bool blend_background =
      quad->background_color != SK_ColorTRANSPARENT && !image->isOpaque();
  bool vertex_alpha =
      quad->vertex_opacity[0] < 1.f || quad->vertex_opacity[1] < 1.f ||
      quad->vertex_opacity[2] < 1.f || quad->vertex_opacity[3] < 1.f;

  if (!blend_background && !vertex_alpha) {
    // This is a simple texture draw and can go into the batching system
    // TODO(michaelludwig) - Batched quads always use the fast rect constraint,
    // which maybe isn't appropriate for texture quads and we need to track
    // the constraint needed for a batch?
    DCHECK(!MustFlushBatchedQuads(quad, params));
    AddQuadToBatch(params, image, visible_uv_rect);
    return;
  }
  // This needs a color filter for background blending and/or a mask filter
  // to simulate the vertex opacity, which requires configuring a full SkPaint
  // and is incompatible with anything batched
  if (!batched_quads_.empty())
    FlushBatchedQuads();

  SkPaint paint = params.paint();
  float quad_alpha = params.opacity;
  if (vertex_alpha) {
    // If they are all the same value, combine it with the overall opacity,
    // otherwise use a mask filter to emulate vertex opacity interpolation
    if (quad->vertex_opacity[0] == quad->vertex_opacity[1] &&
        quad->vertex_opacity[0] == quad->vertex_opacity[2] &&
        quad->vertex_opacity[0] == quad->vertex_opacity[3]) {
      quad_alpha *= quad->vertex_opacity[0];
    } else {
      // The only occurrences of non-constant vertex opacities come from unit
      // tests and src/chrome/browser/android/compositor/decoration_title.cc,
      // but they always produce the effect of a linear alpha gradient.
      // All signs indicate point order is [BL, TL, TR, BR]
      SkPoint gradient_pts[2];
      if (quad->vertex_opacity[0] == quad->vertex_opacity[1] &&
          quad->vertex_opacity[2] == quad->vertex_opacity[3]) {
        // Left to right gradient
        float y = params.visible_rect.y() + 0.5f * params.visible_rect.height();
        gradient_pts[0] = {params.visible_rect.x(), y};
        gradient_pts[1] = {params.visible_rect.right(), y};
      } else if (quad->vertex_opacity[0] == quad->vertex_opacity[3] &&
                 quad->vertex_opacity[1] == quad->vertex_opacity[2]) {
        // Top to bottom gradient
        float x = params.visible_rect.x() + 0.5f * params.visible_rect.width();
        gradient_pts[0] = {x, params.visible_rect.y()};
        gradient_pts[1] = {x, params.visible_rect.bottom()};
      } else {
        // Not sure how to emulate
        NOTIMPLEMENTED();
        return;
      }

      float a1 = quad->vertex_opacity[0] * quad_alpha;
      float a2 = quad->vertex_opacity[2] * quad_alpha;
      SkColor gradient_colors[2] = {SkColor4f({a1, a1, a1, a1}).toSkColor(),
                                    SkColor4f({a2, a2, a2, a2}).toSkColor()};
      sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
          gradient_pts, gradient_colors, nullptr, 2, SkShader::kClamp_TileMode);
      paint.setMaskFilter(SkShaderMaskFilter::Make(std::move(gradient)));
      // shared quad opacity was folded into the gradient, so this will shorten
      // any color filter chain needed for background blending
      quad_alpha = 1.f;
    }
  }

  // From gl_renderer, the final src color will be
  // vertexAlpha * (textureColor + backgroundColor * (1 - textureAlpha)), where
  // vertexAlpha is the quad's alpha * interpolated per-vertex alpha
  if (blend_background) {
    // Add a color filter that does DstOver blending between texture and the
    // background color. Then, modulate by quad's opacity *after* blending.
    sk_sp<SkColorFilter> cf = SkColorFilter::MakeModeFilter(
        quad->background_color, SkBlendMode::kDstOver);
    if (quad_alpha < 1.f) {
      cf = MakeOpacityFilter(quad_alpha, std::move(cf));
      quad_alpha = 1.f;
      DCHECK(cf);
    }
    paint.setColorFilter(std::move(cf));
  }

  // Override the default paint opacity since it may not be params.opacity
  paint.setAlphaf(quad_alpha);

  DrawSingleImage(params, image, visible_uv_rect, &paint);
}

void SkiaRenderer::DrawTileDrawQuad(const TileDrawQuad* quad,
                                    const DrawQuadParams& params) {
  DCHECK(!MustFlushBatchedQuads(quad, params));

  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  ScopedSkImageBuilder builder(
      this, quad->resource_id(),
      quad->is_premultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;

  gfx::RectF vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect), params.visible_rect);
  AddQuadToBatch(params, image, vis_tex_coords);
}

void SkiaRenderer::DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                                    const DrawQuadParams& params) {
  // Since YUV quads always use a color filter, they require a complex skPaint
  // that precludes batching. If this changes, we could add YUV quads that don't
  // require a filter to the batch instead of drawing one at a time.
  DCHECK(batched_quads_.empty());
  if (draw_mode_ != DrawMode::DDL) {
    NOTIMPLEMENTED();
    return;
  }

  gfx::ColorSpace src_color_space = quad->video_color_space;
  // Invalid or unspecified color spaces should be treated as REC709.
  if (!src_color_space.IsValid())
    src_color_space = gfx::ColorSpace::CreateREC709();
  gfx::ColorSpace dst_color_space =
      current_frame()->current_render_pass->color_space;
  sk_sp<SkColorFilter> color_filter =
      GetColorFilter(src_color_space, dst_color_space);

  DCHECK(resource_provider_);
  ScopedYUVSkImageBuilder builder(this, quad, dst_color_space.ToSkColorSpace(),
                                  !!color_filter);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;

  gfx::RectF visible_tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->ya_tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));

  SkPaint paint = params.paint();
  if (color_filter)
    paint.setColorFilter(color_filter);

  DrawSingleImage(params, image, visible_tex_coord_rect, &paint);
}

void SkiaRenderer::DoSingleDrawQuad(const DrawQuad* quad,
                                    const gfx::QuadF* draw_region) {
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
      NOTREACHED();
      break;
    case DrawQuad::PICTURE_CONTENT:
      DrawPictureQuad(PictureDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(RenderPassDrawQuad::MaterialCast(quad), &paint);
      break;
    case DrawQuad::SOLID_COLOR:
      NOTREACHED();
      break;
    case DrawQuad::TEXTURE_CONTENT:
      NOTREACHED();
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
      NOTREACHED();
      break;
    case DrawQuad::STREAM_VIDEO_CONTENT:
      NOTREACHED();
      break;
    case DrawQuad::INVALID:
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

sk_sp<SkColorFilter> SkiaRenderer::GetColorFilter(const gfx::ColorSpace& src,
                                                  const gfx::ColorSpace& dst) {
  sk_sp<SkColorFilter>& color_filter = color_filter_cache_[dst][src];
  if (!color_filter) {
    std::unique_ptr<gfx::ColorTransform> transform =
        gfx::ColorTransform::NewColorTransform(
            src, dst, gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
    // TODO(backer): Support lookup table transforms (e.g.
    // COLOR_CONVERSION_MODE_LUT).
    if (!transform->CanGetShaderSource())
      return nullptr;
    const char* hdr = R"(
void main(inout half4 color) {
  // un-premultiply alpha
  if (color.a > 0)
    color.rgb /= color.a;
)";
    const char* ftr = R"(
  // premultiply alpha
  color.rgb *= color.a;
}
)";

    std::string shader = hdr + transform->GetSkShaderSource() + ftr;
    color_filter =
        SkRuntimeColorFilterFactory(SkString(shader.c_str(), shader.size()))
            .make(SkData::MakeEmpty());
  }
  return color_filter;
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

  // If we need to apply filter, apply opacity as the last step of image filter
  // so it is uniform across.
  if (filter && quad->shared_quad_state->opacity != 1.f) {
    sk_sp<SkColorFilter> cf =
        MakeOpacityFilter(quad->shared_quad_state->opacity, nullptr);
    filter = SkColorFilterImageFilter::Make(cf, filter);
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
  // The flag |force_anti_aliasing_off| is also expected to disable alpha
  // blending, so switch from kSrcOver blending to kSrc.
  if (quad->force_anti_aliasing_off) {
    paint->setAntiAlias(false);
    if (paint->isSrcOver()) {
      paint->setBlendMode(SkBlendMode::kSrc);
    }
  }
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

  // If this render pass has filter, reset paint to opaque. Render Pass should
  // apply their opacity as last step, this is done by using a color filter.
  // This is required for reflector filter.
  if (params.image_filter)
    paint->setAlpha(255);

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
  if (!batched_quads_.empty())
    FlushBatchedQuads();
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
