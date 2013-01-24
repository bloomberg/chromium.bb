// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_SCROLLBAR_LAYER_H_
#define CC_SCROLLBAR_LAYER_H_

#include "cc/cc_export.h"
#include "cc/contents_scaling_layer.h"
#include "cc/layer_updater.h"
#include "cc/scrollbar_theme_painter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {
class CachingBitmapContentLayerUpdater;
class ResourceUpdateQueue;
class Scrollbar;
class ScrollbarThemeComposite;

class CC_EXPORT ScrollbarLayer : public ContentsScalingLayer {
public:
    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

    static scoped_refptr<ScrollbarLayer> create(
        scoped_ptr<WebKit::WebScrollbar>,
        scoped_ptr<ScrollbarThemePainter>,
        scoped_ptr<WebKit::WebScrollbarThemeGeometry>,
        int scrollLayerId);

    int scrollLayerId() const { return m_scrollLayerId; }
    void setScrollLayerId(int id);

    WebKit::WebScrollbar::Orientation orientation() const;

    // Layer interface
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual void setLayerTreeHost(LayerTreeHost*) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;
    virtual void calculateContentsScale(
        float idealContentsScale,
        float* contentsScaleX,
        float* contentsScaleY,
        gfx::Size* contentBounds) OVERRIDE;

    virtual ScrollbarLayer* toScrollbarLayer() OVERRIDE;

protected:
    ScrollbarLayer(
        scoped_ptr<WebKit::WebScrollbar>,
        scoped_ptr<ScrollbarThemePainter>,
        scoped_ptr<WebKit::WebScrollbarThemeGeometry>,
        int scrollLayerId);
    virtual ~ScrollbarLayer();

private:
    void updatePart(CachingBitmapContentLayerUpdater*, LayerUpdater::Resource*, const gfx::Rect&, ResourceUpdateQueue&, RenderingStats&);
    void createUpdaterIfNeeded();
    gfx::Rect scrollbarLayerRectToContentRect(const gfx::Rect& layerRect) const;

    bool isDirty() const { return !m_dirtyRect.IsEmpty(); }

    int maxTextureSize();
    float clampScaleToMaxTextureSize(float scale);

    scoped_ptr<WebKit::WebScrollbar> m_scrollbar;
    scoped_ptr<ScrollbarThemePainter> m_painter;
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> m_geometry;
    gfx::Size m_thumbSize;
    int m_scrollLayerId;

    unsigned m_textureFormat;

    gfx::RectF m_dirtyRect;

    scoped_refptr<CachingBitmapContentLayerUpdater> m_backTrackUpdater;
    scoped_refptr<CachingBitmapContentLayerUpdater> m_foreTrackUpdater;
    scoped_refptr<CachingBitmapContentLayerUpdater> m_thumbUpdater;

    // All the parts of the scrollbar except the thumb
    scoped_ptr<LayerUpdater::Resource> m_backTrack;
    scoped_ptr<LayerUpdater::Resource> m_foreTrack;
    scoped_ptr<LayerUpdater::Resource> m_thumb;
};

}
#endif  // CC_SCROLLBAR_LAYER_H_
