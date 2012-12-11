// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_SCROLLBAR_LAYER_H_
#define CC_SCROLLBAR_LAYER_H_

#include "cc/caching_bitmap_content_layer_updater.h"
#include "cc/cc_export.h"
#include "cc/contents_scaling_layer.h"
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

namespace cc {

class ResourceUpdateQueue;
class Scrollbar;
class ScrollbarThemeComposite;

class CC_EXPORT ScrollbarLayer : public ContentsScalingLayer {
public:
    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

    static scoped_refptr<ScrollbarLayer> create(scoped_ptr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, scoped_ptr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);

    int scrollLayerId() const { return m_scrollLayerId; }
    void setScrollLayerId(int id);

    // Layer interface
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual void setLayerTreeHost(LayerTreeHost*) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;
    virtual void setContentsScale(float contentsScale) OVERRIDE;

    virtual ScrollbarLayer* toScrollbarLayer() OVERRIDE;

protected:
    ScrollbarLayer(scoped_ptr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, scoped_ptr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);
    virtual ~ScrollbarLayer();

private:
    void updatePart(CachingBitmapContentLayerUpdater*, LayerUpdater::Resource*, const gfx::Rect&, ResourceUpdateQueue&, RenderingStats&);
    void createUpdaterIfNeeded();
    gfx::Rect scrollbarLayerRectToContentRect(const gfx::Rect& layerRect) const;

    int maxTextureSize();
    float clampScaleToMaxTextureSize(float scale);

    scoped_ptr<WebKit::WebScrollbar> m_scrollbar;
    WebKit::WebScrollbarThemePainter m_painter;
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> m_geometry;
    int m_scrollLayerId;

    GLenum m_textureFormat;

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
