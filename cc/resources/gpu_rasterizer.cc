// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/gpu_rasterizer.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
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
    bool tile_prepare_enabled) {
  return make_scoped_ptr<GpuRasterizer>(
      new GpuRasterizer(context_provider, resource_provider,
                        use_distance_field_text, tile_prepare_enabled));
}

GpuRasterizer::GpuRasterizer(ContextProvider* context_provider,
                             ResourceProvider* resource_provider,
                             bool use_distance_field_text,
                             bool tile_prepare_enabled)
    : context_provider_(context_provider),
      resource_provider_(resource_provider),
      use_distance_field_text_(use_distance_field_text),
      tile_prepare_enabled_(tile_prepare_enabled) {
  DCHECK(context_provider_);
}

GpuRasterizer::~GpuRasterizer() {
}

PrepareTilesMode GpuRasterizer::GetPrepareTilesMode() {
  return tile_prepare_enabled_ ? PrepareTilesMode::PREPARE_PRIORITIZED_TILES
                               : PrepareTilesMode::PREPARE_NONE;
}

void GpuRasterizer::RasterizeTiles(
    const TileVector& tiles,
    ResourcePool* resource_pool,
    const UpdateTileDrawInfoCallback& update_tile_draw_info) {
  ScopedGpuRaster gpu_raster(context_provider_);

  ScopedResourceWriteLocks locks;

  for (Tile* tile : tiles) {
    // TODO(hendrikw): Don't create resources for solid color tiles.
    // See crbug.com/445919
    scoped_ptr<ScopedResource> resource =
        resource_pool->AcquireResource(tile->desired_texture_size());
    const ScopedResource* const_resource = resource.get();

    RasterSource::SolidColorAnalysis analysis;

    if (tile->use_picture_analysis())
      PerformSolidColorAnalysis(tile, &analysis);

    if (!analysis.is_solid_color)
      AddToMultiPictureDraw(tile, const_resource, &locks);

    update_tile_draw_info.Run(tile, resource.Pass(), analysis);
  }

  multi_picture_draw_.draw();
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
  SkSurface* sk_surface = lock->GetSkSurface(
      use_distance_field_text, tile->raster_source()->CanUseLCDText());

  locks->push_back(lock.Pass());

  if (!sk_surface)
    return;

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
