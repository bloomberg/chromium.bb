// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/overdraw_metrics.h"

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "cc/base/math_util.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace cc {

OverdrawMetrics::OverdrawMetrics(bool record_metrics_for_frame)
    : record_metrics_for_frame_(record_metrics_for_frame),
      pixels_painted_(0),
      pixels_uploaded_opaque_(0),
      pixels_uploaded_translucent_(0),
      tiles_culled_for_upload_(0),
      contents_texture_use_bytes_(0),
      render_surface_texture_use_bytes_(0),
      pixels_drawn_opaque_(0),
      pixels_drawn_translucent_(0),
      pixels_culled_for_drawing_(0) {}

static inline float WedgeProduct(gfx::PointF p1, gfx::PointF p2) {
  return p1.x() * p2.y() - p1.y() * p2.x();
}

// Calculates area of an arbitrary convex polygon with up to 8 points.
static inline float PolygonArea(gfx::PointF points[8], int num_points) {
  if (num_points < 3)
    return 0;

  float area = 0;
  for (int i = 0; i < num_points; ++i)
    area += WedgeProduct(points[i], points[(i+1)%num_points]);
  return fabs(0.5f * area);
}

// Takes a given quad, maps it by the given transformation, and gives the area
// of the resulting polygon.
static inline float AreaOfMappedQuad(const gfx::Transform& transform,
                                     const gfx::QuadF& quad) {
  gfx::PointF clipped_quad[8];
  int num_vertices_in_clipped_quad = 0;
  MathUtil::MapClippedQuad(transform,
                           quad,
                           clipped_quad,
                           &num_vertices_in_clipped_quad);
  return PolygonArea(clipped_quad, num_vertices_in_clipped_quad);
}

void OverdrawMetrics::DidPaint(gfx::Rect painted_rect) {
  if (!record_metrics_for_frame_)
    return;

  pixels_painted_ +=
      static_cast<float>(painted_rect.width()) * painted_rect.height();
}

void OverdrawMetrics::DidCullTilesForUpload(int count) {
  if (record_metrics_for_frame_)
    tiles_culled_for_upload_ += count;
}

void OverdrawMetrics::DidUpload(const gfx::Transform& transform_to_target,
                                gfx::Rect upload_rect,
                                gfx::Rect opaque_rect) {
  if (!record_metrics_for_frame_)
    return;

  float upload_area =
      AreaOfMappedQuad(transform_to_target, gfx::QuadF(upload_rect));
  float upload_opaque_area =
      AreaOfMappedQuad(transform_to_target,
                       gfx::QuadF(gfx::IntersectRects(opaque_rect,
                                                      upload_rect)));

  pixels_uploaded_opaque_ += upload_opaque_area;
  pixels_uploaded_translucent_ += upload_area - upload_opaque_area;
}

void OverdrawMetrics::DidUseContentsTextureMemoryBytes(
    size_t contents_texture_use_bytes) {
  if (!record_metrics_for_frame_)
    return;

  contents_texture_use_bytes_ += contents_texture_use_bytes;
}

void OverdrawMetrics::DidUseRenderSurfaceTextureMemoryBytes(
    size_t render_surface_use_bytes) {
  if (!record_metrics_for_frame_)
    return;

  render_surface_texture_use_bytes_ += render_surface_use_bytes;
}

void OverdrawMetrics::DidCullForDrawing(
    const gfx::Transform& transform_to_target,
    gfx::Rect before_cull_rect,
    gfx::Rect after_cull_rect) {
  if (!record_metrics_for_frame_)
    return;

  float before_cull_area =
      AreaOfMappedQuad(transform_to_target, gfx::QuadF(before_cull_rect));
  float after_cull_area =
      AreaOfMappedQuad(transform_to_target, gfx::QuadF(after_cull_rect));

  pixels_culled_for_drawing_ += before_cull_area - after_cull_area;
}

void OverdrawMetrics::DidDraw(const gfx::Transform& transform_to_target,
                              gfx::Rect after_cull_rect,
                              gfx::Rect opaque_rect) {
  if (!record_metrics_for_frame_)
    return;

  float after_cull_area =
      AreaOfMappedQuad(transform_to_target, gfx::QuadF(after_cull_rect));
  float after_cull_opaque_area =
      AreaOfMappedQuad(transform_to_target,
                       gfx::QuadF(gfx::IntersectRects(opaque_rect,
                                                      after_cull_rect)));

  pixels_drawn_opaque_ += after_cull_opaque_area;
  pixels_drawn_translucent_ += after_cull_area - after_cull_opaque_area;
}

void OverdrawMetrics::RecordMetrics(
    const LayerTreeHost* layer_tree_host) const {
  if (record_metrics_for_frame_)
    RecordMetricsInternal<LayerTreeHost>(UpdateAndCommit, layer_tree_host);
}

void OverdrawMetrics::RecordMetrics(
    const LayerTreeHostImpl* layer_tree_host_impl) const {
  if (record_metrics_for_frame_) {
    RecordMetricsInternal<LayerTreeHostImpl>(DrawingToScreen,
                                             layer_tree_host_impl);
  }
}

static gfx::Size DeviceViewportSize(const LayerTreeHost* host) {
  return host->device_viewport_size();
}
static gfx::Size DeviceViewportSize(const LayerTreeHostImpl* host_impl) {
  return host_impl->device_viewport_size();
}

template <typename LayerTreeHostType>
void OverdrawMetrics::RecordMetricsInternal(
    MetricsType metrics_type,
    const LayerTreeHostType* layer_tree_host) const {
  // This gives approximately 10x the percentage of pixels to fill the viewport
  // once.
  float normalization = 1000.f / (DeviceViewportSize(layer_tree_host).width() *
                                  DeviceViewportSize(layer_tree_host).height());
  // This gives approximately 100x the percentage of tiles to fill the viewport
  // once, if all tiles were 256x256.
  float tile_normalization =
      10000.f / (DeviceViewportSize(layer_tree_host).width() / 256.f *
                 DeviceViewportSize(layer_tree_host).height() / 256.f);
  // This gives approximately 10x the percentage of bytes to fill the viewport
  // once, assuming 4 bytes per pixel.
  float byte_normalization = normalization / 4;

  switch (metrics_type) {
    case DrawingToScreen: {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.pixelCountOpaque_Draw",
          static_cast<int>(normalization * pixels_drawn_opaque_),
          100, 1000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.pixelCountTranslucent_Draw",
          static_cast<int>(normalization * pixels_drawn_translucent_),
          100, 1000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.pixelCountCulled_Draw",
          static_cast<int>(normalization * pixels_culled_for_drawing_),
          100, 1000000, 50);

      TRACE_COUNTER_ID1("cc",
                        "DrawPixelsCulled",
                        layer_tree_host,
                        pixels_culled_for_drawing_);
      TRACE_EVENT2("cc",
                   "OverdrawMetrics",
                   "PixelsDrawnOpaque",
                   pixels_drawn_opaque_,
                   "PixelsDrawnTranslucent",
                   pixels_drawn_translucent_);
      break;
    }
    case UpdateAndCommit: {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.pixelCountPainted",
          static_cast<int>(normalization * pixels_painted_),
          100, 1000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.pixelCountOpaque_Upload",
          static_cast<int>(normalization * pixels_uploaded_opaque_),
          100, 1000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.pixelCountTranslucent_Upload",
          static_cast<int>(normalization * pixels_uploaded_translucent_),
          100, 1000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.tileCountCulled_Upload",
          static_cast<int>(tile_normalization * tiles_culled_for_upload_),
          100, 10000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.renderSurfaceTextureBytes_ViewportScaled",
          static_cast<int>(
              byte_normalization * render_surface_texture_use_bytes_),
          10, 1000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.renderSurfaceTextureBytes_Unscaled",
          static_cast<int>(render_surface_texture_use_bytes_ / 1000),
          1000, 100000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.contentsTextureBytes_ViewportScaled",
          static_cast<int>(byte_normalization * contents_texture_use_bytes_),
          10, 1000000, 50);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.contentsTextureBytes_Unscaled",
          static_cast<int>(contents_texture_use_bytes_ / 1000),
          1000, 100000000, 50); {
        TRACE_COUNTER_ID1("cc",
                          "UploadTilesCulled",
                          layer_tree_host,
                          tiles_culled_for_upload_);
        TRACE_EVENT2("cc",
                     "OverdrawMetrics",
                     "PixelsUploadedOpaque",
                     pixels_uploaded_opaque_,
                     "PixelsUploadedTranslucent",
                     pixels_uploaded_translucent_);
      }
      {
        // This must be in a different scope than the TRACE_EVENT2 above.
        TRACE_EVENT1("cc",
                     "OverdrawPaintMetrics",
                     "PixelsPainted",
                     pixels_painted_);
      }
      {
        // This must be in a different scope than the TRACE_EVENTs above.
        TRACE_EVENT2("cc",
                     "OverdrawPaintMetrics",
                     "ContentsTextureBytes",
                     contents_texture_use_bytes_,
                     "RenderSurfaceTextureBytes",
                     render_surface_texture_use_bytes_);
      }
      break;
    }
  }
}

}  // namespace cc
