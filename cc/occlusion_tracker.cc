// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/occlusion_tracker.h"

#include <algorithm>

#include "cc/layer.h"
#include "cc/layer_impl.h"
#include "cc/math_util.h"
#include "cc/overdraw_metrics.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"

using namespace std;
using WebKit::WebTransformationMatrix;

namespace cc {

template<typename LayerType, typename RenderSurfaceType>
OcclusionTrackerBase<LayerType, RenderSurfaceType>::OcclusionTrackerBase(gfx::Rect rootTargetRect, bool recordMetricsForFrame)
    : m_rootTargetRect(rootTargetRect)
    , m_overdrawMetrics(OverdrawMetrics::create(recordMetricsForFrame))
    , m_occludingScreenSpaceRects(0)
    , m_nonOccludingScreenSpaceRects(0)
{
}

template<typename LayerType, typename RenderSurfaceType>
OcclusionTrackerBase<LayerType, RenderSurfaceType>::~OcclusionTrackerBase()
{
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::enterLayer(const LayerIteratorPosition<LayerType>& layerIterator)
{
    LayerType* renderTarget = layerIterator.targetRenderSurfaceLayer;

    if (layerIterator.representsItself)
        enterRenderTarget(renderTarget);
    else if (layerIterator.representsTargetRenderSurface)
        finishedRenderTarget(renderTarget);
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::leaveLayer(const LayerIteratorPosition<LayerType>& layerIterator)
{
    LayerType* renderTarget = layerIterator.targetRenderSurfaceLayer;

    if (layerIterator.representsItself)
        markOccludedBehindLayer(layerIterator.currentLayer);
    else if (layerIterator.representsContributingRenderSurface)
        leaveToRenderTarget(renderTarget);
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::enterRenderTarget(const LayerType* newTarget)
{
    if (!m_stack.empty() && m_stack.back().target == newTarget)
        return;

    const LayerType* oldTarget = m_stack.empty() ? 0 : m_stack.back().target;
    const RenderSurfaceType* oldAncestorThatMovesPixels = !oldTarget ? 0 : oldTarget->renderSurface()->nearestAncestorThatMovesPixels();
    const RenderSurfaceType* newAncestorThatMovesPixels = newTarget->renderSurface()->nearestAncestorThatMovesPixels();

    m_stack.push_back(StackObject(newTarget));

    // We copy the screen occlusion into the new RenderSurfaceImpl subtree, but we never copy in the
    // target occlusion, since we are looking at a new RenderSurfaceImpl target.

    // If we are entering a subtree that is going to move pixels around, then the occlusion we've computed
    // so far won't apply to the pixels we're drawing here in the same way. We discard the occlusion thus
    // far to be safe, and ensure we don't cull any pixels that are moved such that they become visible.
    bool enteringSubtreeThatMovesPixels = newAncestorThatMovesPixels && newAncestorThatMovesPixels != oldAncestorThatMovesPixels;

    bool copyScreenOcclusionForward = m_stack.size() > 1 && !enteringSubtreeThatMovesPixels;
    if (copyScreenOcclusionForward) {
        int lastIndex = m_stack.size() - 1;
        m_stack[lastIndex].occlusionInScreen = m_stack[lastIndex - 1].occlusionInScreen;
    }
}

static inline bool layerOpacityKnown(const Layer* layer) { return !layer->drawOpacityIsAnimating(); }
static inline bool layerOpacityKnown(const LayerImpl*) { return true; }
static inline bool layerTransformsToTargetKnown(const Layer* layer) { return !layer->drawTransformIsAnimating(); }
static inline bool layerTransformsToTargetKnown(const LayerImpl*) { return true; }
static inline bool layerTransformsToScreenKnown(const Layer* layer) { return !layer->screenSpaceTransformIsAnimating(); }
static inline bool layerTransformsToScreenKnown(const LayerImpl*) { return true; }

static inline bool surfaceOpacityKnown(const RenderSurface* surface) { return !surface->drawOpacityIsAnimating(); }
static inline bool surfaceOpacityKnown(const RenderSurfaceImpl*) { return true; }
static inline bool surfaceTransformsToTargetKnown(const RenderSurface* surface) { return !surface->targetSurfaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToTargetKnown(const RenderSurfaceImpl*) { return true; }
static inline bool surfaceTransformsToScreenKnown(const RenderSurface* surface) { return !surface->screenSpaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToScreenKnown(const RenderSurfaceImpl*) { return true; }

static inline bool layerIsInUnsorted3dRenderingContext(const Layer* layer) { return layer->parent() && layer->parent()->preserves3D(); }
static inline bool layerIsInUnsorted3dRenderingContext(const LayerImpl*) { return false; }

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::finishedRenderTarget(const LayerType* finishedTarget)
{
    // Make sure we know about the target surface.
    enterRenderTarget(finishedTarget);

    RenderSurfaceType* surface = finishedTarget->renderSurface();

    // If the occlusion within the surface can not be applied to things outside of the surface's subtree, then clear the occlusion here so it won't be used.
    // TODO(senorblanco):  Make this smarter for SkImageFilter case:  once
    // SkImageFilters can report affectsOpacity(), call that.
    if (finishedTarget->maskLayer() || !surfaceOpacityKnown(surface) || surface->drawOpacity() < 1 || finishedTarget->filters().hasFilterThatAffectsOpacity() || finishedTarget->filter()) {
        m_stack.back().occlusionInScreen.Clear();
        m_stack.back().occlusionInTarget.Clear();
    } else {
        if (!surfaceTransformsToTargetKnown(surface))
            m_stack.back().occlusionInTarget.Clear();
        if (!surfaceTransformsToScreenKnown(surface))
            m_stack.back().occlusionInScreen.Clear();
    }
}

template<typename RenderSurfaceType>
static inline Region transformSurfaceOpaqueRegion(const RenderSurfaceType* surface, const Region& region, const WebTransformationMatrix& transform)
{
    // Verify that rects within the |surface| will remain rects in its target surface after applying |transform|. If this is true, then
    // apply |transform| to each rect within |region| in order to transform the entire Region.

    bool clipped;
    gfx::QuadF transformedBoundsQuad = MathUtil::mapQuad(transform, gfx::QuadF(region.bounds()), clipped);
    // FIXME: Find a rect interior to each transformed quad.
    if (clipped || !transformedBoundsQuad.IsRectilinear())
        return Region();

    Region transformedRegion;

    for (Region::Iterator rects(region); rects.has_rect(); rects.next()) {
        // We've already checked for clipping in the mapQuad call above, these calls should not clip anything further.
        gfx::Rect transformedRect = gfx::ToEnclosedRect(MathUtil::mapClippedRect(transform, gfx::RectF(rects.rect())));
        if (!surface->clipRect().IsEmpty())
            transformedRect.Intersect(surface->clipRect());
        transformedRegion.Union(transformedRect);
    }
    return transformedRegion;
}

static inline void reduceOcclusion(const gfx::Rect& affectedArea, const gfx::Rect& expandedPixel, Region& occlusion)
{
    if (affectedArea.IsEmpty())
        return;

    Region affectedOcclusion = IntersectRegions(occlusion, affectedArea);
    Region::Iterator affectedOcclusionRects(affectedOcclusion);

    occlusion.Subtract(affectedArea);
    for (; affectedOcclusionRects.has_rect(); affectedOcclusionRects.next()) {
        gfx::Rect occlusionRect = affectedOcclusionRects.rect();

        // Shrink the rect by expanding the non-opaque pixels outside the rect.

        // The expandedPixel is the Rect for a single pixel after being
        // expanded by filters on the layer. The original pixel would be
        // Rect(0, 0, 1, 1), and the expanded pixel is the rect, relative
        // to this original rect, that the original pixel can influence after
        // being filtered.
        // To convert the expandedPixel Rect back to filter outsets:
        // x = -leftOutset
        // width = leftOutset + rightOutset
        // right = x + width = -leftOutset + leftOutset + rightOutset = rightOutset

        // The leftOutset of the filters moves pixels on the right side of
        // the occlusionRect into it, shrinking its right edge.
        int shrinkLeft = occlusionRect.x() == affectedArea.x() ? 0 : expandedPixel.right();
        int shrinkTop = occlusionRect.y() == affectedArea.y() ? 0 : expandedPixel.bottom();
        int shrinkRight = occlusionRect.right() == affectedArea.right() ? 0 : -expandedPixel.x();
        int shrinkBottom = occlusionRect.bottom() == affectedArea.bottom() ? 0 : -expandedPixel.y();

        occlusionRect.Inset(shrinkLeft, shrinkTop, shrinkRight, shrinkBottom);

        occlusion.Union(occlusionRect);
    }
}

template<typename LayerType>
static void reduceOcclusionBelowSurface(LayerType* contributingLayer, const gfx::Rect& surfaceRect, const WebTransformationMatrix& surfaceTransform, LayerType* renderTarget, Region& occlusionInTarget, Region& occlusionInScreen)
{
    if (surfaceRect.IsEmpty())
        return;

    gfx::Rect boundsInTarget = gfx::ToEnclosingRect(MathUtil::mapClippedRect(surfaceTransform, gfx::RectF(surfaceRect)));
    if (!contributingLayer->renderSurface()->clipRect().IsEmpty())
        boundsInTarget.Intersect(contributingLayer->renderSurface()->clipRect());

    int outsetTop, outsetRight, outsetBottom, outsetLeft;
    contributingLayer->backgroundFilters().getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

    // The filter can move pixels from outside of the clip, so allow affectedArea to expand outside the clip.
    boundsInTarget.Inset(-outsetLeft, -outsetTop, -outsetRight, -outsetBottom);

    gfx::Rect boundsInScreen = gfx::ToEnclosingRect(MathUtil::mapClippedRect(renderTarget->renderSurface()->screenSpaceTransform(), gfx::RectF(boundsInTarget)));

    gfx::Rect filterOutsetsInTarget(-outsetLeft, -outsetTop, outsetLeft + outsetRight, outsetTop + outsetBottom);
    gfx::Rect filterOutsetsInScreen = gfx::ToEnclosingRect(MathUtil::mapClippedRect(renderTarget->renderSurface()->screenSpaceTransform(), gfx::RectF(filterOutsetsInTarget)));

    reduceOcclusion(boundsInTarget, filterOutsetsInTarget, occlusionInTarget);
    reduceOcclusion(boundsInScreen, filterOutsetsInScreen, occlusionInScreen);
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::leaveToRenderTarget(const LayerType* newTarget)
{
    int lastIndex = m_stack.size() - 1;
    bool surfaceWillBeAtTopAfterPop = m_stack.size() > 1 && m_stack[lastIndex - 1].target == newTarget;

    // We merge the screen occlusion from the current RenderSurfaceImpl subtree out to its parent target RenderSurfaceImpl.
    // The target occlusion can be merged out as well but needs to be transformed to the new target.

    const LayerType* oldTarget = m_stack[lastIndex].target;
    const RenderSurfaceType* oldSurface = oldTarget->renderSurface();
    Region oldTargetOcclusionInNewTarget = transformSurfaceOpaqueRegion<RenderSurfaceType>(oldSurface, m_stack[lastIndex].occlusionInTarget, oldSurface->drawTransform());
    if (oldTarget->hasReplica() && !oldTarget->replicaHasMask())
        oldTargetOcclusionInNewTarget.Union(transformSurfaceOpaqueRegion<RenderSurfaceType>(oldSurface, m_stack[lastIndex].occlusionInTarget, oldSurface->replicaDrawTransform()));

    gfx::Rect unoccludedSurfaceRect;
    gfx::Rect unoccludedReplicaRect;
    if (oldTarget->backgroundFilters().hasFilterThatMovesPixels()) {
        unoccludedSurfaceRect = unoccludedContributingSurfaceContentRect(oldTarget, false, oldSurface->contentRect());
        if (oldTarget->hasReplica())
            unoccludedReplicaRect = unoccludedContributingSurfaceContentRect(oldTarget, true, oldSurface->contentRect());
    }

    if (surfaceWillBeAtTopAfterPop) {
        // Merge the top of the stack down.
        m_stack[lastIndex - 1].occlusionInScreen.Union(m_stack[lastIndex].occlusionInScreen);
        m_stack[lastIndex - 1].occlusionInTarget.Union(oldTargetOcclusionInNewTarget);
        m_stack.pop_back();
    } else {
        // Replace the top of the stack with the new pushed surface. Copy the occluded screen region to the top.
        m_stack.back().target = newTarget;
        m_stack.back().occlusionInTarget = oldTargetOcclusionInNewTarget;
    }

    if (oldTarget->backgroundFilters().hasFilterThatMovesPixels()) {
        reduceOcclusionBelowSurface(oldTarget, unoccludedSurfaceRect, oldSurface->drawTransform(), newTarget, m_stack.back().occlusionInTarget, m_stack.back().occlusionInScreen);
        if (oldTarget->hasReplica())
            reduceOcclusionBelowSurface(oldTarget, unoccludedReplicaRect, oldSurface->replicaDrawTransform(), newTarget, m_stack.back().occlusionInTarget, m_stack.back().occlusionInScreen);
    }
}

// FIXME: Remove usePaintTracking when paint tracking is on for paint culling.
template<typename LayerType>
static inline void addOcclusionBehindLayer(Region& region, const LayerType* layer, const WebTransformationMatrix& transform, const Region& opaqueContents, const gfx::Rect& clipRectInTarget, const gfx::Size& minimumTrackingSize, std::vector<gfx::Rect>* occludingScreenSpaceRects, std::vector<gfx::Rect>* nonOccludingScreenSpaceRects)
{
    DCHECK(layer->visibleContentRect().Contains(opaqueContents.bounds()));

    bool clipped;
    gfx::QuadF visibleTransformedQuad = MathUtil::mapQuad(transform, gfx::QuadF(layer->visibleContentRect()), clipped);
    // FIXME: Find a rect interior to each transformed quad.
    if (clipped || !visibleTransformedQuad.IsRectilinear())
        return;

    for (Region::Iterator opaqueContentRects(opaqueContents); opaqueContentRects.has_rect(); opaqueContentRects.next()) {
        // We've already checked for clipping in the mapQuad call above, these calls should not clip anything further.
        gfx::Rect transformedRect = gfx::ToEnclosedRect(MathUtil::mapClippedRect(transform, gfx::RectF(opaqueContentRects.rect())));
        transformedRect.Intersect(clipRectInTarget);
        if (transformedRect.width() >= minimumTrackingSize.width() || transformedRect.height() >= minimumTrackingSize.height()) {
            if (occludingScreenSpaceRects)
                occludingScreenSpaceRects->push_back(transformedRect);
            region.Union(transformedRect);
        }
    }

    if (nonOccludingScreenSpaceRects) {
        Region nonOpaqueContents = SubtractRegions(gfx::Rect(layer->contentBounds()), opaqueContents);
        for (Region::Iterator nonOpaqueContentRects(nonOpaqueContents); nonOpaqueContentRects.has_rect(); nonOpaqueContentRects.next()) {
            // We've already checked for clipping in the mapQuad call above, these calls should not clip anything further.
            gfx::Rect transformedRect = gfx::ToEnclosedRect(MathUtil::mapClippedRect(transform, gfx::RectF(nonOpaqueContentRects.rect())));
            transformedRect.Intersect(clipRectInTarget);
            if (transformedRect.IsEmpty())
                continue;
            nonOccludingScreenSpaceRects->push_back(transformedRect);
        }
    }
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::markOccludedBehindLayer(const LayerType* layer)
{
    DCHECK(!m_stack.empty());
    DCHECK(layer->renderTarget() == m_stack.back().target);
    if (m_stack.empty())
        return;

    if (!layerOpacityKnown(layer) || layer->drawOpacity() < 1)
        return;

    if (layerIsInUnsorted3dRenderingContext(layer))
        return;

    Region opaqueContents = layer->visibleContentOpaqueRegion();
    if (opaqueContents.IsEmpty())
        return;

    gfx::Rect clipRectInTarget = layerClipRectInTarget(layer);
    if (layerTransformsToTargetKnown(layer))
        addOcclusionBehindLayer<LayerType>(m_stack.back().occlusionInTarget, layer, layer->drawTransform(), opaqueContents, clipRectInTarget, m_minimumTrackingSize, 0, 0);

    // We must clip the occlusion within the layer's clipRectInTarget within screen space as well. If the clip rect can't be moved to screen space and
    // remain rectilinear, then we don't add any occlusion in screen space.

    if (layerTransformsToScreenKnown(layer)) {
        WebTransformationMatrix targetToScreenTransform = m_stack.back().target->renderSurface()->screenSpaceTransform();
        bool clipped;
        gfx::QuadF clipQuadInScreen = MathUtil::mapQuad(targetToScreenTransform, gfx::QuadF(clipRectInTarget), clipped);
        // FIXME: Find a rect interior to the transformed clip quad.
        if (clipped || !clipQuadInScreen.IsRectilinear())
            return;
        gfx::Rect clipRectInScreen = gfx::IntersectRects(m_rootTargetRect, gfx::ToEnclosedRect(clipQuadInScreen.BoundingBox()));
        addOcclusionBehindLayer<LayerType>(m_stack.back().occlusionInScreen, layer, layer->screenSpaceTransform(), opaqueContents, clipRectInScreen, m_minimumTrackingSize, m_occludingScreenSpaceRects, m_nonOccludingScreenSpaceRects);
    }
}

static inline bool testContentRectOccluded(const gfx::Rect& contentRect, const WebTransformationMatrix& contentSpaceTransform, const gfx::Rect& clipRectInTarget, const Region& occlusion)
{
    gfx::RectF transformedRect = MathUtil::mapClippedRect(contentSpaceTransform, gfx::RectF(contentRect));
    // Take the gfx::ToEnclosingRect, as we want to include partial pixels in the test.
    gfx::Rect targetRect = gfx::IntersectRects(gfx::ToEnclosingRect(transformedRect), clipRectInTarget);
    return occlusion.Contains(targetRect);
}

template<typename LayerType, typename RenderSurfaceType>
bool OcclusionTrackerBase<LayerType, RenderSurfaceType>::occluded(const LayerType* renderTarget, const gfx::Rect& contentRect, const WebKit::WebTransformationMatrix& drawTransform, bool implDrawTransformIsUnknown, const gfx::Rect& clippedRectInTarget, bool* hasOcclusionFromOutsideTargetSurface) const
{
    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = false;

    DCHECK(!m_stack.empty());
    if (m_stack.empty())
        return false;
    if (contentRect.IsEmpty())
        return true;

    DCHECK(renderTarget == m_stack.back().target);

    if (!implDrawTransformIsUnknown && testContentRectOccluded(contentRect, drawTransform, clippedRectInTarget, m_stack.back().occlusionInTarget))
        return true;

    // renderTarget can be NULL in some tests.
    bool transformToScreenKnown = renderTarget && !implDrawTransformIsUnknown && layerTransformsToScreenKnown(renderTarget);
    if (transformToScreenKnown && testContentRectOccluded(contentRect, renderTarget->renderSurface()->screenSpaceTransform() * drawTransform, m_rootTargetRect, m_stack.back().occlusionInScreen)) {
        if (hasOcclusionFromOutsideTargetSurface)
            *hasOcclusionFromOutsideTargetSurface = true;
        return true;
    }

    return false;
}

// Determines what portion of rect, if any, is unoccluded (not occluded by region). If
// the resulting unoccluded region is not rectangular, we return a rect containing it.
static inline gfx::Rect rectSubtractRegion(const gfx::Rect& rect, const Region& region)
{
    if (region.IsEmpty())
        return rect;

    Region rectRegion(rect);
    rectRegion.Subtract(region);
    return rectRegion.bounds();
}

static inline gfx::Rect computeUnoccludedContentRect(const gfx::Rect& contentRect, const WebTransformationMatrix& contentSpaceTransform, const gfx::Rect& clipRectInTarget, const Region& occlusion)
{
    if (!contentSpaceTransform.isInvertible())
        return contentRect;

    // Take the ToEnclosingRect at each step, as we want to contain any unoccluded partial pixels in the resulting Rect.
    gfx::RectF transformedRect = MathUtil::mapClippedRect(contentSpaceTransform, gfx::RectF(contentRect));
    gfx::Rect shrunkRect = rectSubtractRegion(gfx::IntersectRects(gfx::ToEnclosingRect(transformedRect), clipRectInTarget), occlusion);
    gfx::Rect unoccludedRect = gfx::ToEnclosingRect(MathUtil::projectClippedRect(contentSpaceTransform.inverse(), gfx::RectF(shrunkRect)));
    // The rect back in content space is a bounding box and may extend outside of the original contentRect, so clamp it to the contentRectBounds.
    return gfx::IntersectRects(unoccludedRect, contentRect);
}

template<typename LayerType, typename RenderSurfaceType>
gfx::Rect OcclusionTrackerBase<LayerType, RenderSurfaceType>::unoccludedContentRect(const LayerType* renderTarget, const gfx::Rect& contentRect, const WebKit::WebTransformationMatrix& drawTransform, bool implDrawTransformIsUnknown, const gfx::Rect& clippedRectInTarget, bool* hasOcclusionFromOutsideTargetSurface) const
{
    DCHECK(!m_stack.empty());
    if (m_stack.empty())
        return contentRect;
    if (contentRect.IsEmpty())
        return contentRect;

    DCHECK(renderTarget->renderTarget() == renderTarget);
    DCHECK(renderTarget->renderSurface());
    DCHECK(renderTarget == m_stack.back().target);

    // We want to return a rect that contains all the visible parts of |contentRect| in both screen space and in the target surface.
    // So we find the visible parts of |contentRect| in each space, and take the intersection.

    gfx::Rect unoccludedInScreen = contentRect;
    if (layerTransformsToScreenKnown(renderTarget) && !implDrawTransformIsUnknown)
        unoccludedInScreen = computeUnoccludedContentRect(contentRect, renderTarget->renderSurface()->screenSpaceTransform() * drawTransform, m_rootTargetRect, m_stack.back().occlusionInScreen);

    gfx::Rect unoccludedInTarget = contentRect;
    if (!implDrawTransformIsUnknown)
        unoccludedInTarget = computeUnoccludedContentRect(contentRect, drawTransform, clippedRectInTarget, m_stack.back().occlusionInTarget);

    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = (gfx::IntersectRects(unoccludedInScreen, unoccludedInTarget) != unoccludedInTarget);

    return gfx::IntersectRects(unoccludedInScreen, unoccludedInTarget);
}

template<typename LayerType, typename RenderSurfaceType>
gfx::Rect OcclusionTrackerBase<LayerType, RenderSurfaceType>::unoccludedContributingSurfaceContentRect(const LayerType* layer, bool forReplica, const gfx::Rect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const
{
    DCHECK(!m_stack.empty());
    // The layer is a contributing renderTarget so it should have a surface.
    DCHECK(layer->renderSurface());
    // The layer is a contributing renderTarget so its target should be itself.
    DCHECK(layer->renderTarget() == layer);
    // The layer should not be the root, else what is is contributing to?
    DCHECK(layer->parent());
    // This should be called while the layer is still considered the current target in the occlusion tracker.
    DCHECK(layer == m_stack.back().target);

    if (contentRect.IsEmpty())
        return contentRect;

    RenderSurfaceType* surface = layer->renderSurface();

    gfx::Rect surfaceClipRect = surface->clipRect();
    if (surfaceClipRect.IsEmpty()) {
        const LayerType* contributingSurfaceRenderTarget = layer->parent()->renderTarget();
        surfaceClipRect = gfx::IntersectRects(contributingSurfaceRenderTarget->renderSurface()->contentRect(), gfx::ToEnclosingRect(surface->drawableContentRect()));
    }

    // A contributing surface doesn't get occluded by things inside its own surface, so only things outside the surface can occlude it. That occlusion is
    // found just below the top of the stack (if it exists).
    bool hasOcclusion = m_stack.size() > 1;

    const WebTransformationMatrix& transformToScreen = forReplica ? surface->replicaScreenSpaceTransform() : surface->screenSpaceTransform();
    const WebTransformationMatrix& transformToTarget = forReplica ? surface->replicaDrawTransform() : surface->drawTransform();

    gfx::Rect unoccludedInScreen = contentRect;
    if (surfaceTransformsToScreenKnown(surface)) {
        if (hasOcclusion) {
            const StackObject& secondLast = m_stack[m_stack.size() - 2];
            unoccludedInScreen = computeUnoccludedContentRect(contentRect, transformToScreen, m_rootTargetRect, secondLast.occlusionInScreen);
        } else
            unoccludedInScreen = computeUnoccludedContentRect(contentRect, transformToScreen, m_rootTargetRect, Region());
    }

    gfx::Rect unoccludedInTarget = contentRect;
    if (surfaceTransformsToTargetKnown(surface)) {
        if (hasOcclusion) {
            const StackObject& secondLast = m_stack[m_stack.size() - 2];
            unoccludedInTarget = computeUnoccludedContentRect(contentRect, transformToTarget, surfaceClipRect, secondLast.occlusionInTarget);
        } else
            unoccludedInTarget = computeUnoccludedContentRect(contentRect, transformToTarget, surfaceClipRect, Region());
    }

    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = (gfx::IntersectRects(unoccludedInScreen, unoccludedInTarget) != unoccludedInTarget);

    return gfx::IntersectRects(unoccludedInScreen, unoccludedInTarget);
}

template<typename LayerType, typename RenderSurfaceType>
gfx::Rect OcclusionTrackerBase<LayerType, RenderSurfaceType>::layerClipRectInTarget(const LayerType* layer) const
{
    // FIXME: we could remove this helper function, but unit tests currently override this
    //        function, and they need to be verified/adjusted before this can be removed.
    return layer->drawableContentRect();
}

// Instantiate (and export) templates here for the linker.
template class OcclusionTrackerBase<Layer, RenderSurface>;
template class OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>;

}  // namespace cc
