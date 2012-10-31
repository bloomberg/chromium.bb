// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/bitmap_content_layer_updater.h"

#include "cc/layer_painter.h"
#include "cc/resource_update.h"
#include "cc/resource_update_queue.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

BitmapContentLayerUpdater::Resource::Resource(BitmapContentLayerUpdater* updater, scoped_ptr<PrioritizedTexture> texture)
    : LayerUpdater::Resource(texture.Pass())
    , m_updater(updater)
{
}

BitmapContentLayerUpdater::Resource::~Resource()
{
}

void BitmapContentLayerUpdater::Resource::update(ResourceUpdateQueue& queue, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats&)
{
    updater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

scoped_refptr<BitmapContentLayerUpdater> BitmapContentLayerUpdater::create(scoped_ptr<LayerPainter> painter)
{
    return make_scoped_refptr(new BitmapContentLayerUpdater(painter.Pass()));
}

BitmapContentLayerUpdater::BitmapContentLayerUpdater(scoped_ptr<LayerPainter> painter)
    : ContentLayerUpdater(painter.Pass())
    , m_opaque(false)
{
}

BitmapContentLayerUpdater::~BitmapContentLayerUpdater()
{
}

scoped_ptr<LayerUpdater::Resource> BitmapContentLayerUpdater::createResource(PrioritizedTextureManager* manager)
{
    return scoped_ptr<LayerUpdater::Resource>(new Resource(this, PrioritizedTexture::create(manager)));
}

void BitmapContentLayerUpdater::prepareToUpdate(const gfx::Rect& contentRect, const gfx::Size& tileSize, float contentsWidthScale, float contentsHeightScale, gfx::Rect& resultingOpaqueRect, RenderingStats& stats)
{
    if (m_canvasSize != contentRect.size()) {
        m_canvasSize = contentRect.size();
        m_canvas = make_scoped_ptr(skia::CreateBitmapCanvas(m_canvasSize.width(), m_canvasSize.height(), m_opaque));
    }

    paintContents(m_canvas.get(), contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
}

void BitmapContentLayerUpdater::updateTexture(ResourceUpdateQueue& queue, PrioritizedTexture* texture, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate)
{
    ResourceUpdate upload = ResourceUpdate::Create(
        texture,
        &m_canvas->getDevice()->accessBitmap(false),
        contentRect(),
        sourceRect,
        destOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void BitmapContentLayerUpdater::setOpaque(bool opaque)
{
    if (opaque != m_opaque) {
        m_canvas.reset();
        m_canvasSize = gfx::Size();
    }
    m_opaque = opaque;
}

} // namespace cc
