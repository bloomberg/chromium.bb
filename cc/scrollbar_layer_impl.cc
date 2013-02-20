// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_layer_impl.h"

#include "cc/layer_tree_impl.h"
#include "cc/layer_tree_settings.h"
#include "cc/quad_sink.h"
#include "cc/scrollbar_animation_controller.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "ui/gfx/rect_conversions.h"

using WebKit::WebRect;
using WebKit::WebScrollbar;

namespace cc {

scoped_ptr<ScrollbarLayerImpl> ScrollbarLayerImpl::create(LayerTreeImpl* treeImpl, int id, scoped_ptr<ScrollbarGeometryFixedThumb> geometry)
{
    return make_scoped_ptr(new ScrollbarLayerImpl(treeImpl, id, geometry.Pass()));
}

ScrollbarLayerImpl::ScrollbarLayerImpl(LayerTreeImpl* treeImpl, int id, scoped_ptr<ScrollbarGeometryFixedThumb> geometry)
    : ScrollbarLayerImplBase(treeImpl, id)
    , m_scrollbar(this)
    , m_backTrackResourceId(0)
    , m_foreTrackResourceId(0)
    , m_thumbResourceId(0)
    , m_geometry(geometry.Pass())
    , m_currentPos(0)
    , m_totalSize(0)
    , m_maximum(0)
    , m_scrollLayerId(-1)
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

ScrollbarLayerImpl::~ScrollbarLayerImpl()
{
}

ScrollbarLayerImpl* ScrollbarLayerImpl::toScrollbarLayer()
{
    return this;
}

void ScrollbarLayerImpl::setScrollbarData(WebScrollbar* scrollbar)
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
}

void ScrollbarLayerImpl::setThumbSize(gfx::Size size)
{
    m_thumbSize = size;
    if (!m_geometry) {
        // In impl-side painting, the ScrollbarLayerImpl in the pending tree
        // simply holds properties that are later pushed to the active tree's
        // layer, but it doesn't hold geometry or append quads.
        DCHECK(layerTreeImpl()->IsPendingTree());
        return;
    }
    m_geometry->setThumbSize(size);
}

float ScrollbarLayerImpl::currentPos() const
{
    return m_currentPos;
}

int ScrollbarLayerImpl::totalSize() const
{
    return m_totalSize;
}

int ScrollbarLayerImpl::maximum() const
{
    return m_maximum;
}

WebKit::WebScrollbar::Orientation ScrollbarLayerImpl::orientation() const
{
    return m_orientation;
}

static gfx::RectF toUVRect(const gfx::Rect& r, const gfx::Rect& bounds)
{
    return gfx::ScaleRect(r, 1.0 / bounds.width(), 1.0 / bounds.height());
}

gfx::Rect ScrollbarLayerImpl::scrollbarLayerRectToContentRect(const gfx::Rect& layerRect) const
{
    // Don't intersect with the bounds as in layerRectToContentRect() because
    // layerRect here might be in coordinates of the containing layer.
    gfx::RectF contentRect = gfx::ScaleRect(layerRect, contentsScaleX(), contentsScaleY());
    return gfx::ToEnclosingRect(contentRect);
}

scoped_ptr<LayerImpl> ScrollbarLayerImpl::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return ScrollbarLayerImpl::create(treeImpl, id(), m_geometry.Pass()).PassAs<LayerImpl>();
}

void ScrollbarLayerImpl::pushPropertiesTo(LayerImpl* layer)
{
    LayerImpl::pushPropertiesTo(layer);

    ScrollbarLayerImpl* scrollbarLayer = static_cast<ScrollbarLayerImpl*>(layer);

    scrollbarLayer->setScrollbarData(&m_scrollbar);
    scrollbarLayer->setThumbSize(m_thumbSize);

    scrollbarLayer->setBackTrackResourceId(m_backTrackResourceId);
    scrollbarLayer->setForeTrackResourceId(m_foreTrackResourceId);
    scrollbarLayer->setThumbResourceId(m_thumbResourceId);
}

void ScrollbarLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    bool premultipledAlpha = true;
    bool flipped = false;
    gfx::PointF uvTopLeft(0.f, 0.f);
    gfx::PointF uvBottomRight(1.f, 1.f);
    gfx::Rect boundsRect(gfx::Point(), bounds());
    gfx::Rect contentBoundsRect(gfx::Point(), contentBounds());

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    WebRect thumbRect, backTrackRect, foreTrackRect;
    m_geometry->splitTrack(&m_scrollbar, m_geometry->trackRect(&m_scrollbar), backTrackRect, thumbRect, foreTrackRect);
    if (!m_geometry->hasThumb(&m_scrollbar))
        thumbRect = WebRect();

    if (layerTreeImpl()->settings().solidColorScrollbars) {
        int thicknessOverride = layerTreeImpl()->settings().solidColorScrollbarThicknessDIP;
        if (thicknessOverride != -1) {
            if (m_scrollbar.orientation() == WebScrollbar::Vertical)
                thumbRect.width = thicknessOverride;
            else
                thumbRect.height = thicknessOverride;
        }
        gfx::Rect quadRect(scrollbarLayerRectToContentRect(thumbRect));
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(sharedQuadState, quadRect, layerTreeImpl()->settings().solidColorScrollbarColor);
        quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
        return;
    }

    if (m_thumbResourceId && !thumbRect.isEmpty()) {
        gfx::Rect quadRect(scrollbarLayerRectToContentRect(thumbRect));
        gfx::Rect opaqueRect;
        const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
        scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
        quad->SetNew(sharedQuadState, quadRect, opaqueRect, m_thumbResourceId, premultipledAlpha, uvTopLeft, uvBottomRight, opacity, flipped);
        quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
    }

    if (!m_backTrackResourceId)
        return;

    // We only paint the track in two parts if we were given a texture for the forward track part.
    if (m_foreTrackResourceId && !foreTrackRect.isEmpty()) {
        gfx::Rect quadRect(scrollbarLayerRectToContentRect(foreTrackRect));
        gfx::Rect opaqueRect(contentsOpaque() ? quadRect : gfx::Rect());
        gfx::RectF uvRect(toUVRect(foreTrackRect, boundsRect));
        const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
        scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
        quad->SetNew(sharedQuadState, quadRect, opaqueRect, m_foreTrackResourceId, premultipledAlpha, uvRect.origin(), uvRect.bottom_right(), opacity, flipped);
        quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
    }

    // Order matters here: since the back track texture is being drawn to the entire contents rect, we must append it after the thumb and
    // fore track quads. The back track texture contains (and displays) the buttons.
    if (!contentBoundsRect.IsEmpty()) {
        gfx::Rect quadRect(contentBoundsRect);
        gfx::Rect opaqueRect(contentsOpaque() ? quadRect : gfx::Rect());
        const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
        scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
        quad->SetNew(sharedQuadState, quadRect, opaqueRect, m_backTrackResourceId, premultipledAlpha, uvTopLeft, uvBottomRight, opacity, flipped);
        quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
    }
}

void ScrollbarLayerImpl::didLoseOutputSurface()
{
    m_backTrackResourceId = 0;
    m_foreTrackResourceId = 0;
    m_thumbResourceId = 0;
}

bool ScrollbarLayerImpl::Scrollbar::isOverlay() const
{
    return m_owner->m_isOverlayScrollbar;
}

int ScrollbarLayerImpl::Scrollbar::value() const
{
    return m_owner->currentPos();
}

WebKit::WebPoint ScrollbarLayerImpl::Scrollbar::location() const
{
    return WebKit::WebPoint();
}

WebKit::WebSize ScrollbarLayerImpl::Scrollbar::size() const
{
    return WebKit::WebSize(m_owner->bounds().width(), m_owner->bounds().height());
}

bool ScrollbarLayerImpl::Scrollbar::enabled() const
{
    return m_owner->m_enabled;
}

int ScrollbarLayerImpl::Scrollbar::maximum() const
{
    return m_owner->maximum();
}

int ScrollbarLayerImpl::Scrollbar::totalSize() const
{
    return m_owner->totalSize();
}

bool ScrollbarLayerImpl::Scrollbar::isScrollViewScrollbar() const
{
    return m_owner->m_isScrollViewScrollbar;
}

bool ScrollbarLayerImpl::Scrollbar::isScrollableAreaActive() const
{
    return m_owner->m_isScrollableAreaActive;
}

void ScrollbarLayerImpl::Scrollbar::getTickmarks(WebKit::WebVector<WebRect>& tickmarks) const
{
    tickmarks = m_owner->m_tickmarks;
}

WebScrollbar::ScrollbarControlSize ScrollbarLayerImpl::Scrollbar::controlSize() const
{
    return m_owner->m_controlSize;
}

WebScrollbar::ScrollbarPart ScrollbarLayerImpl::Scrollbar::pressedPart() const
{
    return m_owner->m_pressedPart;
}

WebScrollbar::ScrollbarPart ScrollbarLayerImpl::Scrollbar::hoveredPart() const
{
    return m_owner->m_hoveredPart;
}

WebScrollbar::ScrollbarOverlayStyle ScrollbarLayerImpl::Scrollbar::scrollbarOverlayStyle() const
{
    return m_owner->m_scrollbarOverlayStyle;
}

WebScrollbar::Orientation ScrollbarLayerImpl::Scrollbar::orientation() const
{
    return m_owner->m_orientation;
}

bool ScrollbarLayerImpl::Scrollbar::isCustomScrollbar() const
{
    return m_owner->m_isCustomScrollbar;
}

const char* ScrollbarLayerImpl::layerTypeAsString() const
{
    return "ScrollbarLayer";
}

}  // namespace cc
