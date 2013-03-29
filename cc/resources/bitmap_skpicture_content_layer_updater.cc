// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/bitmap_skpicture_content_layer_updater.h"

#include "base/time.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

BitmapSkPictureContentLayerUpdater::Resource::Resource(
    BitmapSkPictureContentLayerUpdater* updater,
    scoped_ptr<PrioritizedResource> texture)
    : ContentLayerUpdater::Resource(texture.Pass()), updater_(updater) {}

void BitmapSkPictureContentLayerUpdater::Resource::Update(
    ResourceUpdateQueue* queue,
    gfx::Rect source_rect,
    gfx::Vector2d dest_offset,
    bool partial_update,
    RenderingStats* stats) {
  bitmap_.setConfig(
      SkBitmap::kARGB_8888_Config, source_rect.width(), source_rect.height());
  bitmap_.allocPixels();
  bitmap_.setIsOpaque(updater_->layer_is_opaque());
  SkDevice device(bitmap_);
  SkCanvas canvas(&device);
  base::TimeTicks paint_begin_time;
  if (stats)
    paint_begin_time = base::TimeTicks::Now();
  updater_->PaintContentsRect(&canvas, source_rect, stats);
  if (stats)
    stats->total_paint_time += base::TimeTicks::Now() - paint_begin_time;

  ResourceUpdate upload = ResourceUpdate::Create(
      texture(), &bitmap_, source_rect, source_rect, dest_offset);
  if (partial_update)
    queue->AppendPartialUpload(upload);
  else
    queue->AppendFullUpload(upload);
}

scoped_refptr<BitmapSkPictureContentLayerUpdater>
BitmapSkPictureContentLayerUpdater::Create(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation) {
  return make_scoped_refptr(
      new BitmapSkPictureContentLayerUpdater(painter.Pass(),
                                             stats_instrumentation));
}

BitmapSkPictureContentLayerUpdater::BitmapSkPictureContentLayerUpdater(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation)
    : SkPictureContentLayerUpdater(painter.Pass(), stats_instrumentation) {}

BitmapSkPictureContentLayerUpdater::~BitmapSkPictureContentLayerUpdater() {}

scoped_ptr<LayerUpdater::Resource>
BitmapSkPictureContentLayerUpdater::CreateResource(
    PrioritizedResourceManager* manager) {
  return scoped_ptr<LayerUpdater::Resource>(
      new Resource(this, PrioritizedResource::Create(manager)));
}

void BitmapSkPictureContentLayerUpdater::PaintContentsRect(
    SkCanvas* canvas,
    gfx::Rect source_rect,
    RenderingStats* stats) {
  // Translate the origin of content_rect to that of source_rect.
  canvas->translate(content_rect().x() - source_rect.x(),
                    content_rect().y() - source_rect.y());
  base::TimeTicks rasterize_begin_time;
  if (stats)
    rasterize_begin_time = base::TimeTicks::Now();
  DrawPicture(canvas);
  if (stats) {
    stats->total_rasterize_time +=
        base::TimeTicks::Now() - rasterize_begin_time;
    stats->total_pixels_rasterized +=
        source_rect.width() * source_rect.height();
  }
}

}  // namespace cc
