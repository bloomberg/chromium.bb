// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/content_layer.h"

#include "CCLayerTreeHost.h"
#include "CCSettings.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "cc/bitmap_canvas_layer_texture_updater.h"
#include "cc/bitmap_skpicture_canvas_layer_texture_updater.h"
#include "cc/content_layer_client.h"
#include "cc/frame_buffer_skpicture_canvas_layer_texture_updater.h"
#include "cc/layer_painter.h"

namespace cc {

ContentLayerPainter::ContentLayerPainter(ContentLayerChromiumClient* client)
    : m_client(client)
{
}

scoped_ptr<ContentLayerPainter> ContentLayerPainter::create(ContentLayerChromiumClient* client)
{
    return make_scoped_ptr(new ContentLayerPainter(client));
}

void ContentLayerPainter::paint(SkCanvas* canvas, const IntRect& contentRect, FloatRect& opaque)
{
    base::TimeTicks paintStart = base::TimeTicks::HighResNow();
    m_client->paintContents(canvas, contentRect, opaque);
    base::TimeTicks paintEnd = base::TimeTicks::HighResNow();
    double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart).InSecondsF();
    HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintDurationMS", (paintEnd - paintStart).InMilliseconds(), 0, 120, 30);
    HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);
}

scoped_refptr<ContentLayerChromium> ContentLayerChromium::create(ContentLayerChromiumClient* client)
{
    return make_scoped_refptr(new ContentLayerChromium(client));
}

ContentLayerChromium::ContentLayerChromium(ContentLayerChromiumClient* client)
    : TiledLayerChromium()
    , m_client(client)
{
}

ContentLayerChromium::~ContentLayerChromium()
{
}

bool ContentLayerChromium::drawsContent() const
{
    return TiledLayerChromium::drawsContent() && m_client;
}

void ContentLayerChromium::setTexturePriorities(const CCPriorityCalculator& priorityCalc)
{
    // Update the tile data before creating all the layer's tiles.
    updateTileSizeAndTilingOption();

    TiledLayerChromium::setTexturePriorities(priorityCalc);
}

void ContentLayerChromium::update(CCTextureUpdateQueue& queue, const CCOcclusionTracker* occlusion, CCRenderingStats& stats)
{
    createTextureUpdaterIfNeeded();
    TiledLayerChromium::update(queue, occlusion, stats);
    m_needsDisplay = false;
}

bool ContentLayerChromium::needMoreUpdates()
{
    return needsIdlePaint();
}

LayerTextureUpdater* ContentLayerChromium::textureUpdater() const
{
    return m_textureUpdater.get();
}

void ContentLayerChromium::createTextureUpdaterIfNeeded()
{
    if (m_textureUpdater)
        return;
    scoped_ptr<LayerPainterChromium> painter = ContentLayerPainter::create(m_client).PassAs<LayerPainterChromium>();
    if (layerTreeHost()->settings().acceleratePainting)
        m_textureUpdater = FrameBufferSkPictureCanvasLayerTextureUpdater::create(painter.Pass());
    else if (CCSettings::perTilePaintingEnabled())
        m_textureUpdater = BitmapSkPictureCanvasLayerTextureUpdater::create(painter.Pass());
    else
        m_textureUpdater = BitmapCanvasLayerTextureUpdater::create(painter.Pass());
    m_textureUpdater->setOpaque(contentsOpaque());

    GLenum textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
    setSampledTexelFormat(textureUpdater()->sampledTexelFormat(textureFormat));
}

void ContentLayerChromium::setContentsOpaque(bool opaque)
{
    LayerChromium::setContentsOpaque(opaque);
    if (m_textureUpdater)
        m_textureUpdater->setOpaque(opaque);
}

}
