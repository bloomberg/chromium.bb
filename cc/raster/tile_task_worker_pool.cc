// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/tile_task_worker_pool.h"

#include "base/trace_event/trace_event.h"
#include "cc/playback/display_list_raster_source.h"
#include "cc/raster/texture_compressor.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDrawFilter.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cc {

TileTaskWorkerPool::TileTaskWorkerPool() {}

TileTaskWorkerPool::~TileTaskWorkerPool() {}

// static
void TileTaskWorkerPool::ScheduleTasksOnOriginThread(TileTaskClient* client,
                                                     TaskGraph* graph) {
  TRACE_EVENT0("cc", "TileTaskWorkerPool::ScheduleTasksOnOriginThread");

  for (TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end(); ++it) {
    TaskGraph::Node& node = *it;
    TileTask* task = static_cast<TileTask*>(node.task);

    if (!task->HasBeenScheduled()) {
      task->WillSchedule();
      task->ScheduleOnOriginThread(client);
      task->DidSchedule();
    }
  }
}

static bool IsSupportedPlaybackToMemoryFormat(ResourceFormat format) {
  switch (format) {
    case RGBA_4444:
    case RGBA_8888:
    case BGRA_8888:
    case ETC1:
      return true;
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case RED_8:
      return false;
  }
  NOTREACHED();
  return false;
}

class SkipImageFilter : public SkDrawFilter {
 public:
  bool filter(SkPaint* paint, Type type) override {
    if (type == kBitmap_Type)
      return false;

    SkShader* shader = paint->getShader();
    return !shader || !shader->isABitmap();
  }
};

// static
void TileTaskWorkerPool::PlaybackToMemory(
    void* memory,
    ResourceFormat format,
    const gfx::Size& size,
    size_t stride,
    const DisplayListRasterSource* raster_source,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    float scale,
    bool include_images) {
  TRACE_EVENT0("cc", "TileTaskWorkerPool::PlaybackToMemory");

  DCHECK(IsSupportedPlaybackToMemoryFormat(format)) << format;

  // Uses kPremul_SkAlphaType since the result is not known to be opaque.
  SkImageInfo info =
      SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType);

  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  if (raster_source->CanUseLCDText()) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props = SkSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);
  }

  if (!stride)
    stride = info.minRowBytes();
  DCHECK_GT(stride, 0u);

  skia::RefPtr<SkDrawFilter> image_filter;
  if (!include_images)
    image_filter = skia::AdoptRef(new SkipImageFilter);

  {
    TRACE_EVENT0("cc", "TileTaskWorkerPool::PlaybackToMemory::ConvertPixels");

    switch (format) {
      case RGBA_8888:
      case BGRA_8888: {
        skia::RefPtr<SkSurface> surface = skia::AdoptRef(
            SkSurface::NewRasterDirect(info, memory, stride, &surface_props));
        skia::RefPtr<SkCanvas> canvas = skia::SharePtr(surface->getCanvas());
        canvas->setDrawFilter(image_filter.get());
        raster_source->PlaybackToCanvas(canvas.get(), canvas_bitmap_rect,
                                        canvas_playback_rect, scale);
        return;
      }
      case RGBA_4444:
      case ETC1: {
        skia::RefPtr<SkSurface> surface =
            skia::AdoptRef(SkSurface::NewRaster(info, &surface_props));
        skia::RefPtr<SkCanvas> canvas = skia::SharePtr(surface->getCanvas());
        canvas->setDrawFilter(image_filter.get());
        // TODO(reveman): Improve partial raster support by reducing the size of
        // playback rect passed to PlaybackToCanvas. crbug.com/519070
        raster_source->PlaybackToCanvas(canvas.get(), canvas_bitmap_rect,
                                        canvas_bitmap_rect, scale);

        if (format == ETC1) {
          DCHECK_EQ(size.width() % 4, 0);
          DCHECK_EQ(size.height() % 4, 0);
          scoped_ptr<TextureCompressor> texture_compressor =
              TextureCompressor::Create(TextureCompressor::kFormatETC1);
          texture_compressor->Compress(
              reinterpret_cast<const uint8_t*>(
                  surface->peekPixels(nullptr, nullptr)),
              reinterpret_cast<uint8_t*>(memory), size.width(), size.height(),
              TextureCompressor::kQualityHigh);
        } else {
          SkImageInfo dst_info = SkImageInfo::Make(
              info.width(), info.height(), ResourceFormatToSkColorType(format),
              info.alphaType(), info.profileType());
          bool rv = canvas->readPixels(dst_info, memory, stride, 0, 0);
          DCHECK(rv);
        }
        return;
      }
      case ALPHA_8:
      case LUMINANCE_8:
      case RGB_565:
      case RED_8:
        NOTREACHED();
        return;
    }

    NOTREACHED();
  }
}

}  // namespace cc
