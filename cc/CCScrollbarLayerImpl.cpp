// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCScrollbarLayerImpl.h"

#include "CCQuadSink.h"
#include "CCScrollbarAnimationController.h"
#include "CCTextureDrawQuad.h"

using WebKit::WebRect;
using WebKit::WebScrollbar;

namespace cc {

PassOwnPtr<CCScrollbarLayerImpl> CCScrollbarLayerImpl::create(int id)
{
    return adoptPtr(new CCScrollbarLayerImpl(id));
}

CCScrollbarLayerImpl::CCScrollbarLayerImpl(int id)
    : CCLayerImpl(id)
    , m_scrollbar(this)
    , m_backTrackResourceId(0)
    , m_foreTrackResourceId(0)
    , m_thumbResourceId(0)
    , m_scrollbarOverlayStyle(WebScrollbar::ScrollbarOverlayStyleDefault)
    , m_orientation(WebScrollbar::Horizontal)
    , m_controlSize(WebScrollbar::RegularScrollbar)
    , m_pressedPart(WebScrollbar::NoPart)
    , m_hoveredPart(WebScrollbar::NoPart)
    , m_isScrollableAreaActive(false)
    , m_isScrollViewScrollbar(false)
    , m_enabled(false)
    , m_isCustomScrollbar(false)
    , m_isOverlayScrollbar(false)
{
}

void CCScrollbarLayerImpl::setScrollbarGeometry(PassOwnPtr<CCScrollbarGeometryFixedThumb> geometry)
{
    m_geometry = geometry;
}

void CCScrollbarLayerImpl::setScrollbarData(WebScrollbar* scrollbar)
{
    m_scrollbarOverlayStyle = scrollbar->scrollbarOverlayStyle();
    m_orientation = scrollbar->orientation();
    m_controlSize = scrollbar->controlSize();
    m_pressedPart = scrollbar->pressedPart();
    m_hoveredPart = scrollbar->hoveredPart();
    m_isScrollableAreaActive = scrollbar->isScrollableAreaActive();
    m_isScrollViewScrollbar = scrollbar->isScrollViewScrollbar();
    m_enabled = scrollbar->enabled();
    m_isCustomScrollbar = scrollbar->isCustomScrollbar();
    m_isOverlayScrollbar = scrollbar->isOverlay();

    scrollbar->getTickmarks(m_tickmarks);

    m_geometry->update(scrollbar);
}

static FloatRect toUVRect(const WebRect& r, const IntRect& bounds)
{
    return FloatRect(static_cast<float>(r.x) / bounds.width(), static_cast<float>(r.y) / bounds.height(),
                     static_cast<float>(r.width) / bounds.width(), static_cast<float>(r.height) / bounds.height());
}

void CCScrollbarLayerImpl::appendQuads(CCQuadSink& quadSink, CCAppendQuadsData& appendQuadsData)
{
    bool premultipledAlpha = false;
    bool flipped = false;
    FloatRect uvRect(0, 0, 1, 1);
    IntRect boundsRect(IntPoint(), contentBounds());

    CCSharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    WebRect thumbRect, backTrackRect, foreTrackRect;
    m_geometry->splitTrack(&m_scrollbar, m_geometry->trackRect(&m_scrollbar), backTrackRect, thumbRect, foreTrackRect);
    if (!m_geometry->hasThumb(&m_scrollbar))
        thumbRect = WebRect();

    if (m_thumbResourceId && !thumbRect.isEmpty()) {
        OwnPtr<CCTextureDrawQuad> quad = CCTextureDrawQuad::create(sharedQuadState, IntRect(thumbRect.x, thumbRect.y, thumbRect.width, thumbRect.height), m_thumbResourceId, premultipledAlpha, uvRect, flipped);
        quad->setNeedsBlending();
        quadSink.append(quad.release(), appendQuadsData);
    }

    if (!m_backTrackResourceId)
        return;

    // We only paint the track in two parts if we were given a texture for the forward track part.
    if (m_foreTrackResourceId && !foreTrackRect.isEmpty())
        quadSink.append(CCTextureDrawQuad::create(sharedQuadState, IntRect(foreTrackRect.x, foreTrackRect.y, foreTrackRect.width, foreTrackRect.height), m_foreTrackResourceId, premultipledAlpha, toUVRect(foreTrackRect, boundsRect), flipped), appendQuadsData);

    // Order matters here: since the back track texture is being drawn to the entire contents rect, we must append it after the thumb and
    // fore track quads. The back track texture contains (and displays) the buttons.
    if (!boundsRect.isEmpty())
        quadSink.append(CCTextureDrawQuad::create(sharedQuadState, IntRect(boundsRect), m_backTrackResourceId, premultipledAlpha, uvRect, flipped), appendQuadsData);
}

void CCScrollbarLayerImpl::didLoseContext()
{
    m_backTrackResourceId = 0;
    m_foreTrackResourceId = 0;
    m_thumbResourceId = 0;
}

bool CCScrollbarLayerImpl::CCScrollbar::isOverlay() const
{
    return m_owner->m_isOverlayScrollbar;
}

int CCScrollbarLayerImpl::CCScrollbar::value() const
{
    return m_owner->m_currentPos;
}

WebKit::WebPoint CCScrollbarLayerImpl::CCScrollbar::location() const
{
    return WebKit::WebPoint();
}

WebKit::WebSize CCScrollbarLayerImpl::CCScrollbar::size() const
{
    return WebKit::WebSize(m_owner->contentBounds().width(), m_owner->contentBounds().height());
}

bool CCScrollbarLayerImpl::CCScrollbar::enabled() const
{
    return m_owner->m_enabled;
}

int CCScrollbarLayerImpl::CCScrollbar::maximum() const
{
    return m_owner->m_maximum;
}

int CCScrollbarLayerImpl::CCScrollbar::totalSize() const
{
    return m_owner->m_totalSize;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollViewScrollbar() const
{
    return m_owner->m_isScrollViewScrollbar;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollableAreaActive() const
{
    return m_owner->m_isScrollableAreaActive;
}

void CCScrollbarLayerImpl::CCScrollbar::getTickmarks(WebKit::WebVector<WebRect>& tickmarks) const
{
    tickmarks = m_owner->m_tickmarks;
}

WebScrollbar::ScrollbarControlSize CCScrollbarLayerImpl::CCScrollbar::controlSize() const
{
    return m_owner->m_controlSize;
}

WebScrollbar::ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::pressedPart() const
{
    return m_owner->m_pressedPart;
}

WebScrollbar::ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::hoveredPart() const
{
    return m_owner->m_hoveredPart;
}

WebScrollbar::ScrollbarOverlayStyle CCScrollbarLayerImpl::CCScrollbar::scrollbarOverlayStyle() const
{
    return m_owner->m_scrollbarOverlayStyle;
}

WebScrollbar::Orientation CCScrollbarLayerImpl::CCScrollbar::orientation() const
{
    return m_owner->m_orientation;
}

bool CCScrollbarLayerImpl::CCScrollbar::isCustomScrollbar() const
{
    return m_owner->m_isCustomScrollbar;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
