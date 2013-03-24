// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/bitmap_content_layer_updater.h"

#include "cc/debug/rendering_stats.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update.h"
#include "cc/resources/resource_update_queue.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

BitmapContentLayerUpdater::Resource::Resource(
    BitmapContentLayerUpdater* updater,
    scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass()), updater_(updater) {}

BitmapContentLayerUpdater::Resource::~Resource() {}

void BitmapContentLayerUpdater::Resource::Update(ResourceUpdateQueue* queue,
                                                 gfx::Rect source_rect,
                                                 gfx::Vector2d dest_offset,
                                                 bool partial_update,
                                                 RenderingStats* stats) {
  updater_->UpdateTexture(
      queue, texture(), source_rect, dest_offset, partial_update);
}

scoped_refptr<BitmapContentLayerUpdater> BitmapContentLayerUpdater::Create(
    scoped_ptr<LayerPainter> painter) {
  return make_scoped_refptr(new BitmapContentLayerUpdater(painter.Pass()));
}

BitmapContentLayerUpdater::BitmapContentLayerUpdater(
    scoped_ptr<LayerPainter> painter)
    : ContentLayerUpdater(painter.Pass()), opaque_(false) {}

BitmapContentLayerUpdater::~BitmapContentLayerUpdater() {}

scoped_ptr<LayerUpdater::Resource> BitmapContentLayerUpdater::CreateResource(
    PrioritizedResourceManager* manager) {
  return scoped_ptr<LayerUpdater::Resource>(
      new Resource(this, PrioritizedResource::Create(manager)));
}

void BitmapContentLayerUpdater::PrepareToUpdate(
    gfx::Rect content_rect,
    gfx::Size tile_size,
    float contents_width_scale,
    float contents_height_scale,
    gfx::Rect* resulting_opaque_rect,
    RenderingStats* stats) {
  if (canvas_size_ != content_rect.size()) {
    canvas_size_ = content_rect.size();
    canvas_ = make_scoped_ptr(skia::CreateBitmapCanvas(
        canvas_size_.width(), canvas_size_.height(), opaque_));
  }

  if (stats) {
    stats->total_pixels_rasterized +=
        content_rect.width() * content_rect.height();
  }

  PaintContents(canvas_.get(),
                content_rect,
                contents_width_scale,
                contents_height_scale,
                resulting_opaque_rect,
                stats);
}

void BitmapContentLayerUpdater::UpdateTexture(ResourceUpdateQueue* queue,
                                              PrioritizedResource* texture,
                                              gfx::Rect source_rect,
                                              gfx::Vector2d dest_offset,
                                              bool partial_update) {
  ResourceUpdate upload =
      ResourceUpdate::Create(texture,
                             &canvas_->getDevice()->accessBitmap(false),
                             content_rect(),
                             source_rect,
                             dest_offset);
  if (partial_update)
    queue->AppendPartialUpload(upload);
  else
    queue->AppendFullUpload(upload);
}

void BitmapContentLayerUpdater::SetOpaque(bool opaque) {
  if (opaque != opaque_) {
    canvas_.reset();
    canvas_size_ = gfx::Size();
  }
  opaque_ = opaque;
}

}  // namespace cc
