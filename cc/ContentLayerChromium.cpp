// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerChromium.h"

#include "BitmapCanvasLayerTextureUpdater.h"
#include "BitmapSkPictureCanvasLayerTextureUpdater.h"
#include "CCLayerTreeHost.h"
#include "CCSettings.h"
#include "FrameBufferSkPictureCanvasLayerTextureUpdater.h"
#include "LayerPainterChromium.h"
#include <public/Platform.h>
#include <wtf/CurrentTime.h>

namespace WebCore {

ContentLayerPainter::ContentLayerPainter(ContentLayerDelegate* delegate)
    : m_delegate(delegate)
{
}

PassOwnPtr<ContentLayerPainter> ContentLayerPainter::create(ContentLayerDelegate* delegate)
{
    return adoptPtr(new ContentLayerPainter(delegate));
}

void ContentLayerPainter::paint(SkCanvas* canvas, const IntRect& contentRect, FloatRect& opaque)
{
    double paintStart = currentTime();
    m_delegate->paintContents(canvas, contentRect, opaque);
    double paintEnd = currentTime();
    double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart);
    WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelContentPaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
    WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelContentPaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);
}

PassRefPtr<ContentLayerChromium> ContentLayerChromium::create(ContentLayerDelegate* delegate)
{
    return adoptRef(new ContentLayerChromium(delegate));
}

ContentLayerChromium::ContentLayerChromium(ContentLayerDelegate* delegate)
    : TiledLayerChromium()
    , m_delegate(delegate)
{
}

ContentLayerChromium::~ContentLayerChromium()
{
}

bool ContentLayerChromium::drawsContent() const
{
    return TiledLayerChromium::drawsContent() && m_delegate;
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

void ContentLayerChromium::createTextureUpdaterIfNeeded()
{
    if (m_textureUpdater)
        return;
    if (layerTreeHost()->settings().acceleratePainting)
        m_textureUpdater = FrameBufferSkPictureCanvasLayerTextureUpdater::create(ContentLayerPainter::create(m_delegate));
    else if (CCSettings::perTilePaintingEnabled())
        m_textureUpdater = BitmapSkPictureCanvasLayerTextureUpdater::create(ContentLayerPainter::create(m_delegate));
    else
        m_textureUpdater = BitmapCanvasLayerTextureUpdater::create(ContentLayerPainter::create(m_delegate));
    m_textureUpdater->setOpaque(opaque());

    GC3Denum textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
    setSampledTexelFormat(textureUpdater()->sampledTexelFormat(textureFormat));
}

void ContentLayerChromium::setOpaque(bool opaque)
{
    LayerChromium::setOpaque(opaque);
    if (m_textureUpdater)
        m_textureUpdater->setOpaque(opaque);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
