// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCROLLBAR_LAYER_IMPL_H_
#define CC_SCROLLBAR_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "cc/scrollbar_geometry_fixed_thumb.h"
#include <public/WebRect.h>
#include <public/WebScrollbar.h>
#include <public/WebVector.h>

namespace cc {

class ScrollView;

class CC_EXPORT ScrollbarLayerImpl : public LayerImpl {
public:
    static scoped_ptr<ScrollbarLayerImpl> create(LayerTreeImpl* treeImpl, int id);
    virtual ~ScrollbarLayerImpl();

    ScrollbarGeometryFixedThumb* scrollbarGeometry() const { return m_geometry.get(); }
    void setScrollbarGeometry(scoped_ptr<ScrollbarGeometryFixedThumb>);
    void setScrollbarData(WebKit::WebScrollbar*);

    void setBackTrackResourceId(ResourceProvider::ResourceId id) { m_backTrackResourceId = id; }
    void setForeTrackResourceId(ResourceProvider::ResourceId id) { m_foreTrackResourceId = id; }
    void setThumbResourceId(ResourceProvider::ResourceId id) { m_thumbResourceId = id; }

    float currentPos() const { return m_currentPos; }
    void setCurrentPos(float currentPos) { m_currentPos = currentPos; }

    int totalSize() const { return m_totalSize; }
    void setTotalSize(int totalSize) { m_totalSize = totalSize; }

    int maximum() const { return m_maximum; }
    void setMaximum(int maximum) { m_maximum = maximum; }

    WebKit::WebScrollbar::Orientation orientation() const { return m_orientation; }

    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;

    virtual void didLoseOutputSurface() OVERRIDE;

protected:
    ScrollbarLayerImpl(LayerTreeImpl* treeImpl, int id);

private:
    // nested class only to avoid namespace problem
    class Scrollbar : public WebKit::WebScrollbar {
    public:
        explicit Scrollbar(ScrollbarLayerImpl* owner) : m_owner(owner) { }

        // WebScrollbar implementation
        virtual bool isOverlay() const;
        virtual int value() const;
        virtual WebKit::WebPoint location() const;
        virtual WebKit::WebSize size() const;
        virtual bool enabled() const;
        virtual int maximum() const;
        virtual int totalSize() const;
        virtual bool isScrollViewScrollbar() const;
        virtual bool isScrollableAreaActive() const;
        virtual void getTickmarks(WebKit::WebVector<WebKit::WebRect>& tickmarks) const;
        virtual WebScrollbar::ScrollbarControlSize controlSize() const;
        virtual WebScrollbar::ScrollbarPart pressedPart() const;
        virtual WebScrollbar::ScrollbarPart hoveredPart() const;
        virtual WebScrollbar::ScrollbarOverlayStyle scrollbarOverlayStyle() const;
        virtual WebScrollbar::Orientation orientation() const;
        virtual bool isCustomScrollbar() const;

    private:
        ScrollbarLayerImpl* m_owner;

    };

    virtual const char* layerTypeAsString() const OVERRIDE;

    gfx::Rect scrollbarLayerRectToContentRect(const gfx::Rect& layerRect) const;

    Scrollbar m_scrollbar;

    ResourceProvider::ResourceId m_backTrackResourceId;
    ResourceProvider::ResourceId m_foreTrackResourceId;
    ResourceProvider::ResourceId m_thumbResourceId;

    scoped_ptr<ScrollbarGeometryFixedThumb> m_geometry;

    // Data to implement Scrollbar
    WebKit::WebScrollbar::ScrollbarOverlayStyle m_scrollbarOverlayStyle;
    WebKit::WebVector<WebKit::WebRect> m_tickmarks;
    WebKit::WebScrollbar::Orientation m_orientation;
    WebKit::WebScrollbar::ScrollbarControlSize m_controlSize;
    WebKit::WebScrollbar::ScrollbarPart m_pressedPart;
    WebKit::WebScrollbar::ScrollbarPart m_hoveredPart;

    float m_currentPos;
    int m_totalSize;
    int m_maximum;

    bool m_isScrollableAreaActive;
    bool m_isScrollViewScrollbar;
    bool m_enabled;
    bool m_isCustomScrollbar;
    bool m_isOverlayScrollbar;
};

}
#endif  // CC_SCROLLBAR_LAYER_IMPL_H_
