// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef ScrollbarLayerChromium_h
#define ScrollbarLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#if defined(OS_CHROMEOS)
#include "caching_bitmap_canvas_layer_texture_updater.h"
#else
#include "BitmapCanvasLayerTextureUpdater.h"
#endif
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

namespace cc {

class Scrollbar;
class ScrollbarThemeComposite;
class CCTextureUpdateQueue;

#if defined(OS_CHROMEOS)
typedef CachingBitmapCanvasLayerTextureUpdater ScrollLayerTextureUpdater;
#else
typedef BitmapCanvasLayerTextureUpdater ScrollLayerTextureUpdater;
#endif

class ScrollbarLayerChromium : public LayerChromium {
public:
    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;

    static scoped_refptr<ScrollbarLayerChromium> create(PassOwnPtr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);

    // LayerChromium interface
    virtual bool needsContentsScale() const OVERRIDE;
    virtual IntSize contentBounds() const OVERRIDE;
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual void setLayerTreeHost(CCLayerTreeHost*) OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

    int scrollLayerId() const { return m_scrollLayerId; }
    void setScrollLayerId(int id) { m_scrollLayerId = id; }

    virtual ScrollbarLayerChromium* toScrollbarLayerChromium() OVERRIDE;

protected:
    ScrollbarLayerChromium(PassOwnPtr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);
    virtual ~ScrollbarLayerChromium();

private:
    void updatePart(ScrollLayerTextureUpdater*, LayerTextureUpdater::Texture*, const IntRect&, CCTextureUpdateQueue&, CCRenderingStats&);
    void createTextureUpdaterIfNeeded();

    OwnPtr<WebKit::WebScrollbar> m_scrollbar;
    WebKit::WebScrollbarThemePainter m_painter;
    OwnPtr<WebKit::WebScrollbarThemeGeometry> m_geometry;
    int m_scrollLayerId;

    GC3Denum m_textureFormat;

    RefPtr<ScrollLayerTextureUpdater> m_backTrackUpdater;
    RefPtr<ScrollLayerTextureUpdater> m_foreTrackUpdater;
    RefPtr<ScrollLayerTextureUpdater> m_thumbUpdater;

    // All the parts of the scrollbar except the thumb
    OwnPtr<LayerTextureUpdater::Texture> m_backTrack;
    OwnPtr<LayerTextureUpdater::Texture> m_foreTrack;
    OwnPtr<LayerTextureUpdater::Texture> m_thumb;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
