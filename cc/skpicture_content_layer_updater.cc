// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/skpicture_content_layer_updater.h"

#include "base/debug/trace_event.h"
#include "cc/layer_painter.h"
#include "cc/prioritized_resource.h"
#include "cc/resource_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

SkPictureContentLayerUpdater::Resource::Resource(SkPictureContentLayerUpdater* updater, scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass())
    , m_updater(updater)
{
}

SkPictureContentLayerUpdater::Resource::~Resource()
{
}

void SkPictureContentLayerUpdater::Resource::update(ResourceUpdateQueue& queue, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats&)
{
    updater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

SkPictureContentLayerUpdater::SkPictureContentLayerUpdater(scoped_ptr<LayerPainter> painter)
    : ContentLayerUpdater(painter.Pass())
    , m_layerIsOpaque(false)
{
}

SkPictureContentLayerUpdater::~SkPictureContentLayerUpdater()
{
}

scoped_refptr<SkPictureContentLayerUpdater> SkPictureContentLayerUpdater::create(scoped_ptr<LayerPainter> painter)
{
    return make_scoped_refptr(new SkPictureContentLayerUpdater(painter.Pass()));
}

scoped_ptr<LayerUpdater::Resource> SkPictureContentLayerUpdater::createResource(PrioritizedResourceManager* manager)
{
    return scoped_ptr<LayerUpdater::Resource>(new Resource(this, PrioritizedResource::create(manager)));
}

void SkPictureContentLayerUpdater::prepareToUpdate(const gfx::Rect& contentRect, const gfx::Size&, float contentsWidthScale, float contentsHeightScale, gfx::Rect& resultingOpaqueRect, RenderingStats& stats)
{
    SkCanvas* canvas = m_picture.beginRecording(contentRect.width(), contentRect.height());
    paintContents(canvas, contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
    m_picture.endRecording();
}

void SkPictureContentLayerUpdater::drawPicture(SkCanvas* canvas)
{
    TRACE_EVENT0("cc", "SkPictureContentLayerUpdater::drawPicture");
    canvas->drawPicture(m_picture);
}

void SkPictureContentLayerUpdater::updateTexture(ResourceUpdateQueue& queue, PrioritizedResource* texture, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate)
{
    ResourceUpdate upload = ResourceUpdate::CreateFromPicture(
        texture, &m_picture, contentRect(), sourceRect, destOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void SkPictureContentLayerUpdater::setOpaque(bool opaque)
{
    m_layerIsOpaque = opaque;
}

}  // namespace cc
