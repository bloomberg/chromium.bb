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
    bool partial_update) {
  bitmap_.setConfig(
      SkBitmap::kARGB_8888_Config, source_rect.width(), source_rect.height());
  bitmap_.allocPixels();
  bitmap_.setIsOpaque(updater_->layer_is_opaque());
  SkDevice device(bitmap_);
  SkCanvas canvas(&device);

  updater_->PaintContentsRect(&canvas, source_rect);

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
    gfx::Rect source_rect) {
  // Translate the origin of content_rect to that of source_rect.
  canvas->translate(content_rect().x() - source_rect.x(),
                    content_rect().y() - source_rect.y());

  base::TimeTicks start_time =
      rendering_stats_instrumentation_->StartRecording();

  DrawPicture(canvas);

  base::TimeDelta duration =
      rendering_stats_instrumentation_->EndRecording(start_time);
  rendering_stats_instrumentation_->AddRaster(
      duration,
      source_rect.width() * source_rect.height(),
      false);

  // TODO: Clarify if this needs to be saved here. crbug.com/223693
  rendering_stats_instrumentation_->AddPaint(duration, 0);
}

}  // namespace cc
