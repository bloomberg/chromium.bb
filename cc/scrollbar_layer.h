// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef ScrollbarLayerChromium_h
#define ScrollbarLayerChromium_h

#include "caching_bitmap_canvas_layer_texture_updater.h"
#include "cc/layer.h"
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

namespace cc {

class Scrollbar;
class ScrollbarThemeComposite;
class CCTextureUpdateQueue;

class ScrollbarLayerChromium : public LayerChromium {
public:
    virtual scoped_ptr<CCLayerImpl> createCCLayerImpl() OVERRIDE;

    static scoped_refptr<ScrollbarLayerChromium> create(scoped_ptr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, scoped_ptr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);

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
    ScrollbarLayerChromium(scoped_ptr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, scoped_ptr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);
    virtual ~ScrollbarLayerChromium();

private:
    void updatePart(CachingBitmapCanvasLayerTextureUpdater*, LayerTextureUpdater::Texture*, const IntRect&, CCTextureUpdateQueue&, CCRenderingStats&);
    void createTextureUpdaterIfNeeded();

    scoped_ptr<WebKit::WebScrollbar> m_scrollbar;
    WebKit::WebScrollbarThemePainter m_painter;
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> m_geometry;
    int m_scrollLayerId;

    GC3Denum m_textureFormat;

    scoped_refptr<CachingBitmapCanvasLayerTextureUpdater> m_backTrackUpdater;
    scoped_refptr<CachingBitmapCanvasLayerTextureUpdater> m_foreTrackUpdater;
    scoped_refptr<CachingBitmapCanvasLayerTextureUpdater> m_thumbUpdater;

    // All the parts of the scrollbar except the thumb
    scoped_ptr<LayerTextureUpdater::Texture> m_backTrack;
    scoped_ptr<LayerTextureUpdater::Texture> m_foreTrack;
    scoped_ptr<LayerTextureUpdater::Texture> m_thumb;
};

}
#endif
