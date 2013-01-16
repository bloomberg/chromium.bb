// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/bitmap_skpicture_content_layer_updater.h"

#include "base/time.h"
#include "cc/layer_painter.h"
#include "cc/prioritized_resource.h"
#include "cc/rendering_stats.h"
#include "cc/resource_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

BitmapSkPictureContentLayerUpdater::Resource::Resource(BitmapSkPictureContentLayerUpdater* updater, scoped_ptr<PrioritizedResource> texture)
    : ContentLayerUpdater::Resource(texture.Pass())
    , m_updater(updater)
{
}

void BitmapSkPictureContentLayerUpdater::Resource::update(ResourceUpdateQueue& queue, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats& stats)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, sourceRect.width(), sourceRect.height());
    m_bitmap.allocPixels();
    m_bitmap.setIsOpaque(m_updater->layerIsOpaque());
    SkDevice device(m_bitmap);
    SkCanvas canvas(&device);
    base::TimeTicks paintBeginTime = base::TimeTicks::Now();
    updater()->paintContentsRect(&canvas, sourceRect, stats);
    stats.totalPaintTime += base::TimeTicks::Now() - paintBeginTime;

    ResourceUpdate upload = ResourceUpdate::Create(
        texture(), &m_bitmap, sourceRect, sourceRect, destOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

scoped_refptr<BitmapSkPictureContentLayerUpdater> BitmapSkPictureContentLayerUpdater::create(scoped_ptr<LayerPainter> painter)
{
    return make_scoped_refptr(new BitmapSkPictureContentLayerUpdater(painter.Pass()));
}

BitmapSkPictureContentLayerUpdater::BitmapSkPictureContentLayerUpdater(scoped_ptr<LayerPainter> painter)
    : SkPictureContentLayerUpdater(painter.Pass())
{
}

BitmapSkPictureContentLayerUpdater::~BitmapSkPictureContentLayerUpdater()
{
}

scoped_ptr<LayerUpdater::Resource> BitmapSkPictureContentLayerUpdater::createResource(PrioritizedResourceManager* manager)
{
    return scoped_ptr<LayerUpdater::Resource>(new Resource(this, PrioritizedResource::create(manager)));
}

void BitmapSkPictureContentLayerUpdater::paintContentsRect(SkCanvas* canvas, const gfx::Rect& sourceRect, RenderingStats& stats)
{
    // Translate the origin of contentRect to that of sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x(),
                      contentRect().y() - sourceRect.y());
    base::TimeTicks rasterizeBeginTime = base::TimeTicks::Now();
    drawPicture(canvas);
    stats.totalRasterizeTime += base::TimeTicks::Now() - rasterizeBeginTime;
    stats.totalPixelsRasterized += sourceRect.width() * sourceRect.height();
}

}  // namespace cc
