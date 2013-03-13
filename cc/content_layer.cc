// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/content_layer.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "cc/bitmap_content_layer_updater.h"
#include "cc/bitmap_skpicture_content_layer_updater.h"
#include "cc/content_layer_client.h"
#include "cc/layer_painter.h"
#include "cc/layer_tree_host.h"

namespace cc {

ContentLayerPainter::ContentLayerPainter(ContentLayerClient* client)
    : m_client(client)
{
}

scoped_ptr<ContentLayerPainter> ContentLayerPainter::Create(ContentLayerClient* client)
{
    return make_scoped_ptr(new ContentLayerPainter(client));
}

void ContentLayerPainter::Paint(SkCanvas* canvas, gfx::Rect contentRect, gfx::RectF* opaque)
{
    base::TimeTicks paintStart = base::TimeTicks::HighResNow();
    m_client->paintContents(canvas, contentRect, *opaque);
    base::TimeTicks paintEnd = base::TimeTicks::HighResNow();
    double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart).InSecondsF();
    UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintDurationMS",
                                (paintEnd - paintStart).InMilliseconds(),
                                0, 120, 30);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintMegapixPerSecond",
                                pixelsPerSec / 1000000, 10, 210, 30);
}

scoped_refptr<ContentLayer> ContentLayer::Create(ContentLayerClient* client)
{
    return make_scoped_refptr(new ContentLayer(client));
}

ContentLayer::ContentLayer(ContentLayerClient* client)
    : TiledLayer()
    , m_client(client)
{
}

ContentLayer::~ContentLayer()
{
}

bool ContentLayer::DrawsContent() const
{
    return TiledLayer::DrawsContent() && m_client;
}

void ContentLayer::SetTexturePriorities(const PriorityCalculator& priorityCalc)
{
    // Update the tile data before creating all the layer's tiles.
    updateTileSizeAndTilingOption();

    TiledLayer::SetTexturePriorities(priorityCalc);
}

void ContentLayer::Update(ResourceUpdateQueue* queue, const OcclusionTracker* occlusion, RenderingStats* stats)
{
    {
        base::AutoReset<bool> ignoreSetNeedsCommit(&ignore_set_needs_commit_, true);

        createUpdaterIfNeeded();
    }

    TiledLayer::Update(queue, occlusion, stats);
    needs_display_ = false;
}

bool ContentLayer::NeedMoreUpdates()
{
    return needsIdlePaint();
}

LayerUpdater* ContentLayer::updater() const
{
    return m_updater.get();
}

void ContentLayer::createUpdaterIfNeeded()
{
    if (m_updater)
        return;
    scoped_ptr<LayerPainter> painter = ContentLayerPainter::Create(m_client).PassAs<LayerPainter>();
    if (layer_tree_host()->settings().acceleratePainting)
        m_updater = SkPictureContentLayerUpdater::create(painter.Pass());
    else if (layer_tree_host()->settings().perTilePaintingEnabled)
        m_updater = BitmapSkPictureContentLayerUpdater::create(painter.Pass());
    else
        m_updater = BitmapContentLayerUpdater::create(painter.Pass());
    m_updater->setOpaque(contents_opaque());

    unsigned textureFormat = layer_tree_host()->GetRendererCapabilities().best_texture_format;
    setTextureFormat(textureFormat);
}

void ContentLayer::SetContentsOpaque(bool opaque)
{
    Layer::SetContentsOpaque(opaque);
    if (m_updater)
        m_updater->setOpaque(opaque);
}

}  // namespace cc
