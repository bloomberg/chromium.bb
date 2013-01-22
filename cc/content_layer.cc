// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/content_layer.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
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

scoped_ptr<ContentLayerPainter> ContentLayerPainter::create(ContentLayerClient* client)
{
    return make_scoped_ptr(new ContentLayerPainter(client));
}

void ContentLayerPainter::paint(SkCanvas* canvas, gfx::Rect contentRect, gfx::RectF& opaque)
{
    base::TimeTicks paintStart = base::TimeTicks::HighResNow();
    m_client->paintContents(canvas, contentRect, opaque);
    base::TimeTicks paintEnd = base::TimeTicks::HighResNow();
    double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart).InSecondsF();
    UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintDurationMS",
                                (paintEnd - paintStart).InMilliseconds(),
                                0, 120, 30);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintMegapixPerSecond",
                                pixelsPerSec / 1000000, 10, 210, 30);
}

const int ContentLayer::kLCDTextMaxChangeCount = 1;

scoped_refptr<ContentLayer> ContentLayer::create(ContentLayerClient* client)
{
    return make_scoped_refptr(new ContentLayer(client));
}

ContentLayer::ContentLayer(ContentLayerClient* client)
    : TiledLayer()
    , m_client(client)
    , m_useLCDText(false)
    , m_lcdTextChangeCount(0)
{
}

ContentLayer::~ContentLayer()
{
}

bool ContentLayer::drawsContent() const
{
    return TiledLayer::drawsContent() && m_client;
}

void ContentLayer::setTexturePriorities(const PriorityCalculator& priorityCalc)
{
    // Update the tile data before creating all the layer's tiles.
    updateTileSizeAndTilingOption();

    TiledLayer::setTexturePriorities(priorityCalc);
}

void ContentLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats)
{
    {
        base::AutoReset<bool> ignoreSetNeedsCommit(&m_ignoreSetNeedsCommit, true);

        createUpdaterIfNeeded();
        updateUseLCDText();
    }

    TiledLayer::update(queue, occlusion, stats);
    m_needsDisplay = false;
}

bool ContentLayer::needMoreUpdates()
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
    scoped_ptr<LayerPainter> painter = ContentLayerPainter::create(m_client).PassAs<LayerPainter>();
    if (layerTreeHost()->settings().acceleratePainting)
        m_updater = SkPictureContentLayerUpdater::create(painter.Pass());
    else if (layerTreeHost()->settings().perTilePaintingEnabled)
        m_updater = BitmapSkPictureContentLayerUpdater::create(painter.Pass());
    else
        m_updater = BitmapContentLayerUpdater::create(painter.Pass());
    m_updater->setOpaque(contentsOpaque());

    unsigned textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
}

void ContentLayer::setContentsOpaque(bool opaque)
{
    Layer::setContentsOpaque(opaque);
    if (m_updater)
        m_updater->setOpaque(opaque);
}

void ContentLayer::updateUseLCDText()
{
    if (m_useLCDText == drawProperties().can_use_lcd_text)
        return;

    if (!useLCDTextWillChange())
        return;

    m_useLCDText = drawProperties().can_use_lcd_text;
    useLCDTextDidChange();
}

bool ContentLayer::useLCDTextWillChange() const
{
    // Always allow disabling LCD text.
    if (m_useLCDText)
        return true;
 
    return m_lcdTextChangeCount < kLCDTextMaxChangeCount;
}

void ContentLayer::useLCDTextDidChange()
{
    if (m_lcdTextChangeCount > 0) {
        // Do not record the first time LCD text is enabled because
        // it does not really cause any invalidation.
        TRACE_EVENT_INSTANT0("cc", "ContentLayer::canUseLCDTextDidChange");
    }
    ++m_lcdTextChangeCount;
 
    // Need to repaint the layer with different text AA setting.
    setNeedsDisplay();
}

}  // namespace cc
