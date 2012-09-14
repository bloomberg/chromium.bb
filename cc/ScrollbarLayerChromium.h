// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef ScrollbarLayerChromium_h
#define ScrollbarLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#include "LayerTextureUpdater.h"
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

namespace cc {

class Scrollbar;
class ScrollbarThemeComposite;
class CCTextureUpdateQueue;

class ScrollbarLayerChromium : public LayerChromium {
public:
    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;
    static PassRefPtr<ScrollbarLayerChromium> create(PassOwnPtr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);

    // LayerChromium interface
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual void setLayerTreeHost(CCLayerTreeHost*) OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

    int scrollLayerId() const { return m_scrollLayerId; }
    void setScrollLayerId(int id) { m_scrollLayerId = id; }

    virtual ScrollbarLayerChromium* toScrollbarLayerChromium() OVERRIDE { return this; }

protected:
    ScrollbarLayerChromium(PassOwnPtr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);

private:
    void updatePart(LayerTextureUpdater*, LayerTextureUpdater::Texture*, const IntRect&, CCTextureUpdateQueue&, CCRenderingStats&);
    void createTextureUpdaterIfNeeded();

    OwnPtr<WebKit::WebScrollbar> m_scrollbar;
    WebKit::WebScrollbarThemePainter m_painter;
    OwnPtr<WebKit::WebScrollbarThemeGeometry> m_geometry;
    int m_scrollLayerId;

    GC3Denum m_textureFormat;

    RefPtr<LayerTextureUpdater> m_backTrackUpdater;
    RefPtr<LayerTextureUpdater> m_foreTrackUpdater;
    RefPtr<LayerTextureUpdater> m_thumbUpdater;

    // All the parts of the scrollbar except the thumb
    OwnPtr<LayerTextureUpdater::Texture> m_backTrack;
    OwnPtr<LayerTextureUpdater::Texture> m_foreTrack;
    OwnPtr<LayerTextureUpdater::Texture> m_thumb;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
