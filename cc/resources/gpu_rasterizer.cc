// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/gpu_rasterizer.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/output/context_provider.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_gpu_raster.h"
#include "cc/resources/tile_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {

// static
scoped_ptr<GpuRasterizer> GpuRasterizer::Create(
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text,
    bool threaded_gpu_rasterization_enabled,
    int msaa_sample_count) {
  return make_scoped_ptr<GpuRasterizer>(new GpuRasterizer(
      context_provider, resource_provider, use_distance_field_text,
      threaded_gpu_rasterization_enabled, msaa_sample_count));
}

GpuRasterizer::GpuRasterizer(ContextProvider* context_provider,
                             ResourceProvider* resource_provider,
                             bool use_distance_field_text,
                             bool threaded_gpu_rasterization_enabled,
                             int msaa_sample_count)
    : resource_provider_(resource_provider),
      use_distance_field_text_(use_distance_field_text),
      threaded_gpu_rasterization_enabled_(threaded_gpu_rasterization_enabled),
      msaa_sample_count_(msaa_sample_count) {
}

GpuRasterizer::~GpuRasterizer() {
}

PrepareTilesMode GpuRasterizer::GetPrepareTilesMode() {
  return threaded_gpu_rasterization_enabled_
             ? PrepareTilesMode::RASTERIZE_PRIORITIZED_TILES
             : PrepareTilesMode::PREPARE_NONE;
}

ContextProvider* GpuRasterizer::GetContextProvider(bool worker_context) {
  return worker_context
             ? resource_provider_->output_surface()->worker_context_provider()
             : resource_provider_->output_surface()->context_provider();
}

void GpuRasterizer::RasterizeTiles(
    const TileVector& tiles,
    ResourcePool* resource_pool,
    ResourceFormat resource_format,
    const UpdateTileDrawInfoCallback& update_tile_draw_info) {
  ScopedGpuRaster gpu_raster(GetContextProvider(false));

  ScopedResourceWriteLocks locks;

  for (Tile* tile : tiles) {
    RasterSource::SolidColorAnalysis analysis;

    if (tile->use_picture_analysis())
      PerformSolidColorAnalysis(tile, &analysis);

    scoped_ptr<ScopedResource> resource;
    if (!analysis.is_solid_color) {
      resource = resource_pool->AcquireResource(tile->desired_texture_size(),
                                                resource_format);
      AddToMultiPictureDraw(tile, resource.get(), &locks);
    }
    update_tile_draw_info.Run(tile, resource.Pass(), analysis);
  }

  // If MSAA is enabled, tell Skia to resolve each render target after draw.
  multi_picture_draw_.draw(msaa_sample_count_ > 0);
}

void GpuRasterizer::RasterizeSource(
    bool use_worker_context,
    ResourceProvider::ScopedWriteLockGr* write_lock,
    const RasterSource* raster_source,
    const gfx::Rect& rect,
    float scale) {
  // Play back raster_source into temp SkPicture.
  SkPictureRecorder recorder;
  gfx::Size size = write_lock->resource()->size;
  const int flags = SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag;
  skia::RefPtr<SkCanvas> canvas = skia::SharePtr(
      recorder.beginRecording(size.width(), size.height(), NULL, flags));
  canvas->save();
  raster_source->PlaybackToCanvas(canvas.get(), rect, scale);
  canvas->restore();
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());

  // Turn on distance fields for layers that have ever animated.
  bool use_distance_field_text =
      use_distance_field_text_ ||
      raster_source->ShouldAttemptToUseDistanceFieldText();

  // Playback picture into resource.
  {
    ScopedGpuRaster gpu_raster(GetContextProvider(use_worker_context));
    write_lock->InitSkSurface(use_worker_context, use_distance_field_text,
                              raster_source->CanUseLCDText(),
                              msaa_sample_count_);

    SkSurface* sk_surface = write_lock->sk_surface();

    // Allocating an SkSurface will fail after a lost context.  Pretend we
    // rasterized, as the contents of the resource don't matter anymore.
    if (!sk_surface)
      return;

    SkMultiPictureDraw multi_picture_draw;
    multi_picture_draw.add(sk_surface->getCanvas(), picture.get());
    multi_picture_draw.draw(msaa_sample_count_ > 0);
    write_lock->ReleaseSkSurface();
  }
}

void GpuRasterizer::PerformSolidColorAnalysis(
    const Tile* tile,
    RasterSource::SolidColorAnalysis* analysis) {
  const void* tile_id = static_cast<const void*>(tile);
  frame_viewer_instrumentation::ScopedAnalyzeTask analyze_task(
      tile_id, tile->combined_priority().resolution,
      tile->source_frame_number(), tile->layer_id());

  DCHECK(tile->raster_source());

  tile->raster_source()->PerformSolidColorAnalysis(
      tile->content_rect(), tile->contents_scale(), analysis);

  // Record the solid color prediction.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                        analysis->is_solid_color);
}

void GpuRasterizer::AddToMultiPictureDraw(const Tile* tile,
                                          const ScopedResource* resource,
                                          ScopedResourceWriteLocks* locks) {
  const void* tile_id = static_cast<const void*>(tile);
  frame_viewer_instrumentation::ScopedRasterTask raster_task(
      tile_id, tile->combined_priority().resolution,
      tile->source_frame_number(), tile->layer_id());

  DCHECK(tile->raster_source());

  // Turn on distance fields for layers that have ever animated.
  bool use_distance_field_text =
      use_distance_field_text_ ||
      tile->raster_source()->ShouldAttemptToUseDistanceFieldText();
  scoped_ptr<ResourceProvider::ScopedWriteLockGr> lock(
      new ResourceProvider::ScopedWriteLockGr(resource_provider_,
                                              resource->id()));

  lock->InitSkSurface(false, use_distance_field_text,
                      tile->raster_source()->CanUseLCDText(),
                      msaa_sample_count_);

  SkSurface* sk_surface = lock->sk_surface();

  // Allocating an SkSurface will fail after a lost context.  Pretend we
  // rasterized, as the contents of the resource don't matter anymore.
  if (!sk_surface)
    return;

  locks->push_back(lock.Pass());

  SkRTreeFactory factory;
  SkPictureRecorder recorder;
  gfx::Size size = resource->size();
  const int flags = SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag;
  skia::RefPtr<SkCanvas> canvas = skia::SharePtr(
      recorder.beginRecording(size.width(), size.height(), &factory, flags));

  canvas->save();
  tile->raster_source()->PlaybackToCanvas(canvas.get(), tile->content_rect(),
                                          tile->contents_scale());
  canvas->restore();

  // Add the canvas and recorded picture to |multi_picture_draw_|.
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());
  multi_picture_draw_.add(sk_surface->getCanvas(), picture.get());
}

}  // namespace cc
