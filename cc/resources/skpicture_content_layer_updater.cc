// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/skpicture_content_layer_updater.h"

#include "base/debug/trace_event.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

SkPictureContentLayerUpdater::Resource::Resource(
    SkPictureContentLayerUpdater* updater,
    scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass()), updater_(updater) {}

SkPictureContentLayerUpdater::Resource::~Resource() {}

void SkPictureContentLayerUpdater::Resource::Update(ResourceUpdateQueue* queue,
                                                    gfx::Rect source_rect,
                                                    gfx::Vector2d dest_offset,
                                                    bool partial_update,
                                                    RenderingStats*) {
  updater_->UpdateTexture(
      queue, texture(), source_rect, dest_offset, partial_update);
}

SkPictureContentLayerUpdater::SkPictureContentLayerUpdater(
    scoped_ptr<LayerPainter> painter)
    : ContentLayerUpdater(painter.Pass()), layer_is_opaque_(false) {}

SkPictureContentLayerUpdater::~SkPictureContentLayerUpdater() {}

scoped_refptr<SkPictureContentLayerUpdater>
SkPictureContentLayerUpdater::Create(scoped_ptr<LayerPainter> painter) {
  return make_scoped_refptr(new SkPictureContentLayerUpdater(painter.Pass()));
}

scoped_ptr<LayerUpdater::Resource> SkPictureContentLayerUpdater::CreateResource(
    PrioritizedResourceManager* manager) {
  return scoped_ptr<LayerUpdater::Resource>(
      new Resource(this, PrioritizedResource::Create(manager)));
}

void SkPictureContentLayerUpdater::PrepareToUpdate(
    gfx::Rect content_rect,
    gfx::Size,
    float contentsWidthScale,
    float contents_height_scale,
    gfx::Rect* resultingOpaqueRect,
    RenderingStats* stats) {
  SkCanvas* canvas =
      picture_.beginRecording(content_rect.width(), content_rect.height());
  PaintContents(canvas,
                content_rect,
                contentsWidthScale,
                contents_height_scale,
                resultingOpaqueRect,
                stats);
  picture_.endRecording();
}

void SkPictureContentLayerUpdater::DrawPicture(SkCanvas* canvas) {
  TRACE_EVENT0("cc", "SkPictureContentLayerUpdater::drawPicture");
  canvas->drawPicture(picture_);
}

void SkPictureContentLayerUpdater::UpdateTexture(ResourceUpdateQueue* queue,
                                                 PrioritizedResource* texture,
                                                 gfx::Rect source_rect,
                                                 gfx::Vector2d dest_offset,
                                                 bool partial_update) {
  ResourceUpdate upload = ResourceUpdate::CreateFromPicture(
      texture, &picture_, content_rect(), source_rect, dest_offset);
  if (partial_update)
    queue->appendPartialUpload(upload);
  else
    queue->appendFullUpload(upload);
}

void SkPictureContentLayerUpdater::SetOpaque(bool opaque) {
  layer_is_opaque_ = opaque;
}

}  // namespace cc
