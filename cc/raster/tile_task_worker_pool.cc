// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/tile_task_worker_pool.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "cc/playback/display_list_raster_source.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/utils/SkPaintFilterCanvas.h"

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

namespace {

bool IsSupportedPlaybackToMemoryFormat(ResourceFormat format) {
  switch (format) {
    case RGBA_4444:
    case RGBA_8888:
    case BGRA_8888:
      return true;
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
    case RED_8:
    case LUMINANCE_F16:
      return false;
  }
  NOTREACHED();
  return false;
}

class SkipImageCanvas : public SkPaintFilterCanvas {
 public:
  explicit SkipImageCanvas(SkCanvas* canvas) : SkPaintFilterCanvas(canvas) {}

  bool onFilter(SkTCopyOnFirstWrite<SkPaint>* paint, Type type) const override {
    if (type == kBitmap_Type)
      return false;

    SkShader* shader = (*paint) ? (*paint)->getShader() : nullptr;
    return !shader || !shader->isABitmap();
  }

  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkTCopyOnFirstWrite<SkPaint> filteredPaint(paint);

    // To filter nested draws, we must unfurl pictures at this stage.
    if (onFilter(&filteredPaint, kPicture_Type))
      SkCanvas::onDrawPicture(picture, matrix, filteredPaint);
  }
};

class AutoSkipImageCanvas {
 public:
  AutoSkipImageCanvas(SkCanvas* canvas, bool include_images) : canvas_(canvas) {
    if (!include_images) {
      skip_image_canvas_ = skia::AdoptRef(new SkipImageCanvas(canvas));
      canvas_ = skip_image_canvas_.get();
    }
  }

  operator SkCanvas*() { return canvas_; }

 private:
  skia::RefPtr<SkCanvas> skip_image_canvas_;
  SkCanvas* canvas_;
};

}  // anonymous namespace

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
  SkColorType buffer_color_type = ResourceFormatToSkColorType(format);
  bool needs_copy = buffer_color_type != info.colorType();

  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  if (raster_source->CanUseLCDText()) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props = SkSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);
  }

  if (!stride)
    stride = info.minRowBytes();
  DCHECK_GT(stride, 0u);

  if (!needs_copy) {
    skia::RefPtr<SkSurface> surface = skia::AdoptRef(
        SkSurface::NewRasterDirect(info, memory, stride, &surface_props));
    AutoSkipImageCanvas canvas(surface->getCanvas(), include_images);
    raster_source->PlaybackToCanvas(canvas, canvas_bitmap_rect,
                                    canvas_playback_rect, scale);
    return;
  }

  skia::RefPtr<SkSurface> surface =
      skia::AdoptRef(SkSurface::NewRaster(info, &surface_props));
  AutoSkipImageCanvas canvas(surface->getCanvas(), include_images);
  // TODO(reveman): Improve partial raster support by reducing the size of
  // playback rect passed to PlaybackToCanvas. crbug.com/519070
  raster_source->PlaybackToCanvas(canvas, canvas_bitmap_rect,
                                  canvas_bitmap_rect, scale);

  {
    TRACE_EVENT0("cc", "TileTaskWorkerPool::PlaybackToMemory::ConvertPixels");

    SkImageInfo dst_info =
        SkImageInfo::Make(info.width(), info.height(), buffer_color_type,
                          info.alphaType(), info.profileType());
    bool rv = surface->getCanvas()->readPixels(dst_info, memory, stride, 0, 0);
    DCHECK(rv);
  }
}

}  // namespace cc
