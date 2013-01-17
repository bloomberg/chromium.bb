// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_layer.h"

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "cc/caching_bitmap_content_layer_updater.h"
#include "cc/layer_painter.h"
#include "cc/layer_tree_host.h"
#include "cc/prioritized_resource.h"
#include "cc/resource_update_queue.h"
#include "cc/scrollbar_layer_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_ptr<LayerImpl> ScrollbarLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return ScrollbarLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

scoped_refptr<ScrollbarLayer> ScrollbarLayer::create(
    scoped_ptr<WebKit::WebScrollbar> scrollbar,
    scoped_ptr<ScrollbarThemePainter> painter,
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry,
    int scrollLayerId)
{
    return make_scoped_refptr(new ScrollbarLayer(scrollbar.Pass(), painter.Pass(), geometry.Pass(), scrollLayerId));
}

ScrollbarLayer::ScrollbarLayer(
    scoped_ptr<WebKit::WebScrollbar> scrollbar,
    scoped_ptr<ScrollbarThemePainter> painter,
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry,
    int scrollLayerId)
    : m_scrollbar(scrollbar.Pass())
    , m_painter(painter.Pass())
    , m_geometry(geometry.Pass())
    , m_scrollLayerId(scrollLayerId)
    , m_textureFormat(GL_INVALID_ENUM)
{
    if (!m_scrollbar->isOverlay())
        setShouldScrollOnMainThread(true);
}

ScrollbarLayer::~ScrollbarLayer()
{
}

void ScrollbarLayer::setScrollLayerId(int id)
{
    if (id == m_scrollLayerId)
        return;

    m_scrollLayerId = id;
    setNeedsFullTreeSync();
}

WebKit::WebScrollbar::Orientation ScrollbarLayer::orientation() const
{
    return m_scrollbar->orientation();
}

int ScrollbarLayer::maxTextureSize() {
    DCHECK(layerTreeHost());
    return layerTreeHost()->rendererCapabilities().maxTextureSize;
}

float ScrollbarLayer::clampScaleToMaxTextureSize(float scale) {
    // If the scaled contentBounds() is bigger than the max texture size of the
    // device, we need to clamp it by rescaling, since contentBounds() is used
    // below to set the texture size.
    gfx::Size scaledBounds = computeContentBoundsForScale(scale, scale);
    if (scaledBounds.width() > maxTextureSize() || scaledBounds.height() > maxTextureSize()) {
         if (scaledBounds.width() > scaledBounds.height())
             return (maxTextureSize() - 1) / static_cast<float>(bounds().width());
         else
             return (maxTextureSize() - 1) / static_cast<float>(bounds().height());
   }
    return scale;
}

void ScrollbarLayer::calculateContentsScale(
  float idealContentsScale,
  float* contentsScaleX,
  float* contentsScaleY,
  gfx::Size* contentBounds)
{
    ContentsScalingLayer::calculateContentsScale(
        clampScaleToMaxTextureSize(idealContentsScale),
        contentsScaleX,
        contentsScaleY,
        contentBounds);
    DCHECK_LE(contentBounds->width(), maxTextureSize());
    DCHECK_LE(contentBounds->height(), maxTextureSize());
}

void ScrollbarLayer::pushPropertiesTo(LayerImpl* layer)
{
    ContentsScalingLayer::pushPropertiesTo(layer);

    ScrollbarLayerImpl* scrollbarLayer = static_cast<ScrollbarLayerImpl*>(layer);

    if (!scrollbarLayer->scrollbarGeometry())
        scrollbarLayer->setScrollbarGeometry(ScrollbarGeometryFixedThumb::create(make_scoped_ptr(m_geometry->clone())));

    scrollbarLayer->setScrollbarData(m_scrollbar.get());

    if (m_backTrack && m_backTrack->texture()->haveBackingTexture())
        scrollbarLayer->setBackTrackResourceId(m_backTrack->texture()->resourceId());
    else
        scrollbarLayer->setBackTrackResourceId(0);

    if (m_foreTrack && m_foreTrack->texture()->haveBackingTexture())
        scrollbarLayer->setForeTrackResourceId(m_foreTrack->texture()->resourceId());
    else
        scrollbarLayer->setForeTrackResourceId(0);

    if (m_thumb && m_thumb->texture()->haveBackingTexture())
        scrollbarLayer->setThumbResourceId(m_thumb->texture()->resourceId());
    else
        scrollbarLayer->setThumbResourceId(0);
}

ScrollbarLayer* ScrollbarLayer::toScrollbarLayer()
{
    return this;
}

class ScrollbarBackgroundPainter : public LayerPainter {
public:
    static scoped_ptr<ScrollbarBackgroundPainter> create(WebKit::WebScrollbar* scrollbar, ScrollbarThemePainter *painter, WebKit::WebScrollbarThemeGeometry* geometry, WebKit::WebScrollbar::ScrollbarPart trackPart)
    {
        return make_scoped_ptr(new ScrollbarBackgroundPainter(scrollbar, painter, geometry, trackPart));
    }

    virtual void paint(SkCanvas* canvas, gfx::Rect contentRect, gfx::RectF&) OVERRIDE
    {
        // The following is a simplification of ScrollbarThemeComposite::paint.
        m_painter->PaintScrollbarBackground(canvas, contentRect);

        if (m_geometry->hasButtons(m_scrollbar)) {
            gfx::Rect backButtonStartPaintRect = m_geometry->backButtonStartRect(m_scrollbar);
            m_painter->PaintBackButtonStart(canvas, backButtonStartPaintRect);

            gfx::Rect backButtonEndPaintRect = m_geometry->backButtonEndRect(m_scrollbar);
            m_painter->PaintBackButtonEnd(canvas, backButtonEndPaintRect);

            gfx::Rect forwardButtonStartPaintRect = m_geometry->forwardButtonStartRect(m_scrollbar);
            m_painter->PaintForwardButtonStart(canvas, forwardButtonStartPaintRect);

            gfx::Rect forwardButtonEndPaintRect = m_geometry->forwardButtonEndRect(m_scrollbar);
            m_painter->PaintForwardButtonEnd(canvas, forwardButtonEndPaintRect);
        }

        gfx::Rect trackPaintRect = m_geometry->trackRect(m_scrollbar);
        m_painter->PaintTrackBackground(canvas, trackPaintRect);

        bool thumbPresent = m_geometry->hasThumb(m_scrollbar);
        if (thumbPresent) {
            if (m_trackPart == WebKit::WebScrollbar::ForwardTrackPart)
                m_painter->PaintForwardTrackPart(canvas, trackPaintRect);
            else
                m_painter->PaintBackTrackPart(canvas, trackPaintRect);
        }

        m_painter->PaintTickmarks(canvas, trackPaintRect);
    }
private:
    ScrollbarBackgroundPainter(WebKit::WebScrollbar* scrollbar, ScrollbarThemePainter *painter, WebKit::WebScrollbarThemeGeometry* geometry, WebKit::WebScrollbar::ScrollbarPart trackPart)
        : m_scrollbar(scrollbar)
        , m_painter(painter)
        , m_geometry(geometry)
        , m_trackPart(trackPart)
    {
    }

    WebKit::WebScrollbar* m_scrollbar;
    ScrollbarThemePainter* m_painter;
    WebKit::WebScrollbarThemeGeometry* m_geometry;
    WebKit::WebScrollbar::ScrollbarPart m_trackPart;

    DISALLOW_COPY_AND_ASSIGN(ScrollbarBackgroundPainter);
};

class ScrollbarThumbPainter : public LayerPainter {
public:
    static scoped_ptr<ScrollbarThumbPainter> create(WebKit::WebScrollbar* scrollbar, ScrollbarThemePainter* painter, WebKit::WebScrollbarThemeGeometry* geometry)
    {
        return make_scoped_ptr(new ScrollbarThumbPainter(scrollbar, painter, geometry));
    }

    virtual void paint(SkCanvas* canvas, gfx::Rect contentRect, gfx::RectF& opaque) OVERRIDE
    {
        // Consider the thumb to be at the origin when painting.
        gfx::Rect thumbRect = m_geometry->thumbRect(m_scrollbar);
        m_painter->PaintThumb(canvas, gfx::Rect(thumbRect.size()));
    }

private:
    ScrollbarThumbPainter(WebKit::WebScrollbar* scrollbar, ScrollbarThemePainter* painter, WebKit::WebScrollbarThemeGeometry* geometry)
        : m_scrollbar(scrollbar)
        , m_painter(painter)
        , m_geometry(geometry)
    {
    }

    WebKit::WebScrollbar* m_scrollbar;
    ScrollbarThemePainter* m_painter;
    WebKit::WebScrollbarThemeGeometry* m_geometry;

    DISALLOW_COPY_AND_ASSIGN(ScrollbarThumbPainter);
};

void ScrollbarLayer::setLayerTreeHost(LayerTreeHost* host)
{
    if (!host || host != layerTreeHost()) {
        m_backTrackUpdater = NULL;
        m_backTrack.reset();
        m_thumbUpdater = NULL;
        m_thumb.reset();
    }

    ContentsScalingLayer::setLayerTreeHost(host);
}

void ScrollbarLayer::createUpdaterIfNeeded()
{
    m_textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;

    if (!m_backTrackUpdater)
        m_backTrackUpdater = CachingBitmapContentLayerUpdater::Create(ScrollbarBackgroundPainter::create(m_scrollbar.get(), m_painter.get(), m_geometry.get(), WebKit::WebScrollbar::BackTrackPart).PassAs<LayerPainter>());
    if (!m_backTrack)
        m_backTrack = m_backTrackUpdater->createResource(layerTreeHost()->contentsTextureManager());

    // Only create two-part track if we think the two parts could be different in appearance.
    if (m_scrollbar->isCustomScrollbar()) {
        if (!m_foreTrackUpdater)
            m_foreTrackUpdater = CachingBitmapContentLayerUpdater::Create(ScrollbarBackgroundPainter::create(m_scrollbar.get(), m_painter.get(), m_geometry.get(), WebKit::WebScrollbar::ForwardTrackPart).PassAs<LayerPainter>());
        if (!m_foreTrack)
            m_foreTrack = m_foreTrackUpdater->createResource(layerTreeHost()->contentsTextureManager());
    }

    if (!m_thumbUpdater)
        m_thumbUpdater = CachingBitmapContentLayerUpdater::Create(ScrollbarThumbPainter::create(m_scrollbar.get(), m_painter.get(), m_geometry.get()).PassAs<LayerPainter>());
    if (!m_thumb)
        m_thumb = m_thumbUpdater->createResource(layerTreeHost()->contentsTextureManager());
}

void ScrollbarLayer::updatePart(CachingBitmapContentLayerUpdater* painter, LayerUpdater::Resource* resource, const gfx::Rect& rect, ResourceUpdateQueue& queue, RenderingStats& stats)
{
    // Skip painting and uploading if there are no invalidations and
    // we already have valid texture data.
    if (resource->texture()->haveBackingTexture() &&
        resource->texture()->size() == rect.size() &&
        !isDirty())
        return;

    // We should always have enough memory for UI.
    DCHECK(resource->texture()->canAcquireBackingTexture());
    if (!resource->texture()->canAcquireBackingTexture())
        return;

    // Paint and upload the entire part.
    gfx::Rect paintedOpaqueRect;
    painter->prepareToUpdate(rect, rect.size(), contentsScaleX(), contentsScaleY(), paintedOpaqueRect, stats);
    if (!painter->pixelsDidChange() && resource->texture()->haveBackingTexture()) {
        TRACE_EVENT_INSTANT0("cc","ScrollbarLayer::updatePart no texture upload needed");
        return;
    }

    bool partialUpdatesAllowed = layerTreeHost()->settings().maxPartialTextureUpdates > 0;
    if (!partialUpdatesAllowed)
        resource->texture()->returnBackingTexture();

    gfx::Vector2d destOffset(0, 0);
    resource->update(queue, rect, destOffset, partialUpdatesAllowed, stats);
}

gfx::Rect ScrollbarLayer::scrollbarLayerRectToContentRect(const gfx::Rect& layerRect) const
{
    // Don't intersect with the bounds as in layerRectToContentRect() because
    // layerRect here might be in coordinates of the containing layer.
    gfx::RectF contentRect = gfx::ScaleRect(layerRect, contentsScaleX(), contentsScaleY());
    return gfx::ToEnclosingRect(contentRect);
}

void ScrollbarLayer::setTexturePriorities(const PriorityCalculator&)
{
    if (contentBounds().IsEmpty())
        return;
    DCHECK_LE(contentBounds().width(), maxTextureSize());
    DCHECK_LE(contentBounds().height(), maxTextureSize());

    createUpdaterIfNeeded();

    bool drawsToRoot = !renderTarget()->parent();
    if (m_backTrack) {
        m_backTrack->texture()->setDimensions(contentBounds(), m_textureFormat);
        m_backTrack->texture()->setRequestPriority(PriorityCalculator::uiPriority(drawsToRoot));
    }
    if (m_foreTrack) {
        m_foreTrack->texture()->setDimensions(contentBounds(), m_textureFormat);
        m_foreTrack->texture()->setRequestPriority(PriorityCalculator::uiPriority(drawsToRoot));
    }
    if (m_thumb) {
        gfx::Size thumbSize = scrollbarLayerRectToContentRect(m_geometry->thumbRect(m_scrollbar.get())).size();
        m_thumb->texture()->setDimensions(thumbSize, m_textureFormat);
        m_thumb->texture()->setRequestPriority(PriorityCalculator::uiPriority(drawsToRoot));
    }
}

void ScrollbarLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats)
{
    ContentsScalingLayer::update(queue, occlusion, stats);

    m_dirtyRect.Union(m_updateRect);
    if (contentBounds().IsEmpty())
        return;
    if (visibleContentRect().IsEmpty())
        return;
    if (!isDirty())
        return;

    createUpdaterIfNeeded();

    gfx::Rect contentRect = scrollbarLayerRectToContentRect(gfx::Rect(m_scrollbar->location(), bounds()));
    updatePart(m_backTrackUpdater.get(), m_backTrack.get(), contentRect, queue, stats);
    if (m_foreTrack && m_foreTrackUpdater)
        updatePart(m_foreTrackUpdater.get(), m_foreTrack.get(), contentRect, queue, stats);

    // Consider the thumb to be at the origin when painting.
    gfx::Rect thumbRect = m_geometry->thumbRect(m_scrollbar.get());
    gfx::Rect originThumbRect = scrollbarLayerRectToContentRect(gfx::Rect(thumbRect.size()));
    if (!originThumbRect.IsEmpty())
        updatePart(m_thumbUpdater.get(), m_thumb.get(), originThumbRect, queue, stats);

    m_dirtyRect = gfx::RectF();
}

}  // namespace cc
