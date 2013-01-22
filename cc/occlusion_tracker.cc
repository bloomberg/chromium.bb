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

namespace cc {

template<typename LayerType, typename RenderSurfaceType>
OcclusionTrackerBase<LayerType, RenderSurfaceType>::OcclusionTrackerBase(gfx::Rect screenSpaceClipRect, bool recordMetricsForFrame)
    : m_screenSpaceClipRect(screenSpaceClipRect)
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
    // TODO(danakj): This should be done when entering the contributing surface, but in a way that the surface's own occlusion won't occlude itself.
    else if (layerIterator.representsContributingRenderSurface)
        leaveToRenderTarget(renderTarget);
}

template<typename RenderSurfaceType>
static gfx::Rect screenSpaceClipRectInTargetSurface(const RenderSurfaceType* targetSurface, gfx::Rect screenSpaceClipRect)
{
    gfx::Transform inverseScreenSpaceTransform(gfx::Transform::kSkipInitialization);
    if (!targetSurface->screenSpaceTransform().GetInverse(&inverseScreenSpaceTransform))
        return targetSurface->contentRect();

    return gfx::ToEnclosingRect(MathUtil::projectClippedRect(inverseScreenSpaceTransform, screenSpaceClipRect));
}

template<typename RenderSurfaceType>
static Region transformSurfaceOpaqueRegion(const Region& region, bool haveClipRect, gfx::Rect clipRectInNewTarget, const gfx::Transform& transform)
{
    if (region.IsEmpty())
        return Region();

    // Verify that rects within the |surface| will remain rects in its target surface after applying |transform|. If this is true, then
    // apply |transform| to each rect within |region| in order to transform the entire Region.

    bool clipped;
    gfx::QuadF transformedBoundsQuad = MathUtil::mapQuad(transform, gfx::QuadF(region.bounds()), clipped);
    // FIXME: Find a rect interior to each transformed quad.
    if (clipped || !transformedBoundsQuad.IsRectilinear())
        return Region();

    // TODO(danakj): If the Region is too complex, degrade gracefully here by skipping rects in it.
    Region transformedRegion;
    for (Region::Iterator rects(region); rects.has_rect(); rects.next()) {
        gfx::Rect transformedRect = gfx::ToEnclosedRect(MathUtil::mapQuad(transform, gfx::QuadF(rects.rect()), clipped).BoundingBox());
        DCHECK(!clipped); // We only map if the transform preserves axis alignment.
        if (haveClipRect)
            transformedRect.Intersect(clipRectInNewTarget);
        transformedRegion.Union(transformedRect);
    }
    return transformedRegion;
}

static inline bool layerOpacityKnown(const Layer* layer) { return !layer->drawOpacityIsAnimating(); }
static inline bool layerOpacityKnown(const LayerImpl*) { return true; }
static inline bool layerTransformsToTargetKnown(const Layer* layer) { return !layer->drawTransformIsAnimating(); }
static inline bool layerTransformsToTargetKnown(const LayerImpl*) { return true; }

static inline bool surfaceOpacityKnown(const RenderSurface* surface) { return !surface->drawOpacityIsAnimating(); }
static inline bool surfaceOpacityKnown(const RenderSurfaceImpl*) { return true; }
static inline bool surfaceTransformsToTargetKnown(const RenderSurface* surface) { return !surface->targetSurfaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToTargetKnown(const RenderSurfaceImpl*) { return true; }
static inline bool surfaceTransformsToScreenKnown(const RenderSurface* surface) { return !surface->screenSpaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToScreenKnown(const RenderSurfaceImpl*) { return true; }

static inline bool layerIsInUnsorted3dRenderingContext(const Layer* layer) { return layer->parent() && layer->parent()->preserves3D(); }
static inline bool layerIsInUnsorted3dRenderingContext(const LayerImpl*) { return false; }

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::enterRenderTarget(const LayerType* newTarget)
{
    if (!m_stack.empty() && m_stack.back().target == newTarget)
        return;

    const LayerType* oldTarget = m_stack.empty() ? 0 : m_stack.back().target;
    const RenderSurfaceType* oldAncestorThatMovesPixels = !oldTarget ? 0 : oldTarget->renderSurface()->nearestAncestorThatMovesPixels();
    const RenderSurfaceType* newAncestorThatMovesPixels = newTarget->renderSurface()->nearestAncestorThatMovesPixels();

    m_stack.push_back(StackObject(newTarget));

    // We copy the screen occlusion into the new RenderSurface subtree, but we never copy in the
    // occlusion from inside the target, since we are looking at a new RenderSurface target.

    // If we are entering a subtree that is going to move pixels around, then the occlusion we've computed
    // so far won't apply to the pixels we're drawing here in the same way. We discard the occlusion thus
    // far to be safe, and ensure we don't cull any pixels that are moved such that they become visible.
    bool enteringSubtreeThatMovesPixels = newAncestorThatMovesPixels && newAncestorThatMovesPixels != oldAncestorThatMovesPixels;

    bool haveTransformFromScreenToNewTarget = false;
    gfx::Transform inverseNewTargetScreenSpaceTransform(gfx::Transform::kSkipInitialization); // Note carefully, not used if screen space transform is uninvertible.
    if (surfaceTransformsToScreenKnown(newTarget->renderSurface()))
        haveTransformFromScreenToNewTarget = newTarget->renderSurface()->screenSpaceTransform().GetInverse(&inverseNewTargetScreenSpaceTransform);

    bool enteringRootTarget = newTarget->parent() == NULL;

    bool copyOutsideOcclusionForward = m_stack.size() > 1 && !enteringSubtreeThatMovesPixels && haveTransformFromScreenToNewTarget && !enteringRootTarget;
    if (!copyOutsideOcclusionForward)
        return;

    int lastIndex = m_stack.size() - 1;
    gfx::Transform oldTargetToNewTargetTransform(inverseNewTargetScreenSpaceTransform, oldTarget->renderSurface()->screenSpaceTransform());
    m_stack[lastIndex].occlusionFromOutsideTarget = transformSurfaceOpaqueRegion<RenderSurfaceType>(
            m_stack[lastIndex - 1].occlusionFromOutsideTarget,
            false,
            gfx::Rect(),
            oldTargetToNewTargetTransform);
    m_stack[lastIndex].occlusionFromOutsideTarget.Union(transformSurfaceOpaqueRegion<RenderSurfaceType>(
            m_stack[lastIndex - 1].occlusionFromInsideTarget,
            false,
            gfx::Rect(),
            oldTargetToNewTargetTransform));
}

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
        m_stack.back().occlusionFromOutsideTarget.Clear();
        m_stack.back().occlusionFromInsideTarget.Clear();
    } else if (!surfaceTransformsToTargetKnown(surface)) {
        m_stack.back().occlusionFromInsideTarget.Clear();
        m_stack.back().occlusionFromOutsideTarget.Clear();
    }
}

template<typename LayerType>
static void reduceOcclusionBelowSurface(LayerType* contributingLayer, const gfx::Rect& surfaceRect, const gfx::Transform& surfaceTransform, LayerType* renderTarget, Region& occlusionFromInsideTarget)
{
    if (surfaceRect.IsEmpty())
        return;

    gfx::Rect affectedAreaInTarget = gfx::ToEnclosingRect(MathUtil::mapClippedRect(surfaceTransform, gfx::RectF(surfaceRect)));
    if (contributingLayer->renderSurface()->isClipped())
        affectedAreaInTarget.Intersect(contributingLayer->renderSurface()->clipRect());
    if (affectedAreaInTarget.IsEmpty())
        return;

    int outsetTop, outsetRight, outsetBottom, outsetLeft;
    contributingLayer->backgroundFilters().getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

    // The filter can move pixels from outside of the clip, so allow affectedArea to expand outside the clip.
    affectedAreaInTarget.Inset(-outsetLeft, -outsetTop, -outsetRight, -outsetBottom);

    gfx::Rect filterOutsetsInTarget(-outsetLeft, -outsetTop, outsetLeft + outsetRight, outsetTop + outsetBottom);

    Region affectedOcclusion = IntersectRegions(occlusionFromInsideTarget, affectedAreaInTarget);
    Region::Iterator affectedOcclusionRects(affectedOcclusion);

    occlusionFromInsideTarget.Subtract(affectedAreaInTarget);
    for (; affectedOcclusionRects.has_rect(); affectedOcclusionRects.next()) {
        gfx::Rect occlusionRect = affectedOcclusionRects.rect();

        // Shrink the rect by expanding the non-opaque pixels outside the rect.

        // The left outset of the filters moves pixels on the right side of
        // the occlusionRect into it, shrinking its right edge.
        int shrinkLeft = occlusionRect.x() == affectedAreaInTarget.x() ? 0 : outsetRight;
        int shrinkTop = occlusionRect.y() == affectedAreaInTarget.y() ? 0 : outsetBottom;
        int shrinkRight = occlusionRect.right() == affectedAreaInTarget.right() ? 0 : outsetLeft;
        int shrinkBottom = occlusionRect.bottom() == affectedAreaInTarget.bottom() ? 0 : outsetTop;

        occlusionRect.Inset(shrinkLeft, shrinkTop, shrinkRight, shrinkBottom);

        occlusionFromInsideTarget.Union(occlusionRect);
    }
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

    Region oldOcclusionFromInsideTargetInNewTarget = transformSurfaceOpaqueRegion<RenderSurfaceType>(m_stack[lastIndex].occlusionFromInsideTarget, oldSurface->isClipped(), oldSurface->clipRect(), oldSurface->drawTransform());
    if (oldTarget->hasReplica() && !oldTarget->replicaHasMask())
        oldOcclusionFromInsideTargetInNewTarget.Union(transformSurfaceOpaqueRegion<RenderSurfaceType>(m_stack[lastIndex].occlusionFromInsideTarget, oldSurface->isClipped(), oldSurface->clipRect(), oldSurface->replicaDrawTransform()));

    Region oldOcclusionFromOutsideTargetInNewTarget = transformSurfaceOpaqueRegion<RenderSurfaceType>(m_stack[lastIndex].occlusionFromOutsideTarget, false, gfx::Rect(), oldSurface->drawTransform());

    gfx::Rect unoccludedSurfaceRect;
    gfx::Rect unoccludedReplicaRect;
    if (oldTarget->backgroundFilters().hasFilterThatMovesPixels()) {
        unoccludedSurfaceRect = unoccludedContributingSurfaceContentRect(oldTarget, false, oldSurface->contentRect());
        if (oldTarget->hasReplica())
            unoccludedReplicaRect = unoccludedContributingSurfaceContentRect(oldTarget, true, oldSurface->contentRect());
    }

    if (surfaceWillBeAtTopAfterPop) {
        // Merge the top of the stack down.
        m_stack[lastIndex - 1].occlusionFromInsideTarget.Union(oldOcclusionFromInsideTargetInNewTarget);
        // TODO(danakj): Strictly this should subtract the inside target occlusion before union.
        if (newTarget->parent())
            m_stack[lastIndex - 1].occlusionFromOutsideTarget.Union(oldOcclusionFromOutsideTargetInNewTarget);
        m_stack.pop_back();
    } else {
        // Replace the top of the stack with the new pushed surface.
        m_stack.back().target = newTarget;
        m_stack.back().occlusionFromInsideTarget = oldOcclusionFromInsideTargetInNewTarget;
        if (newTarget->parent())
            m_stack.back().occlusionFromOutsideTarget = oldOcclusionFromOutsideTargetInNewTarget;
        else
            m_stack.back().occlusionFromOutsideTarget.Clear();
    }

    if (!oldTarget->backgroundFilters().hasFilterThatMovesPixels())
        return;

    reduceOcclusionBelowSurface(oldTarget, unoccludedSurfaceRect, oldSurface->drawTransform(), newTarget, m_stack.back().occlusionFromInsideTarget);
    reduceOcclusionBelowSurface(oldTarget, unoccludedSurfaceRect, oldSurface->drawTransform(), newTarget, m_stack.back().occlusionFromOutsideTarget);

    if (!oldTarget->hasReplica())
        return;
    reduceOcclusionBelowSurface(oldTarget, unoccludedReplicaRect, oldSurface->replicaDrawTransform(), newTarget, m_stack.back().occlusionFromInsideTarget);
    reduceOcclusionBelowSurface(oldTarget, unoccludedReplicaRect, oldSurface->replicaDrawTransform(), newTarget, m_stack.back().occlusionFromOutsideTarget);
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

    if (!layerTransformsToTargetKnown(layer))
        return;

    Region opaqueContents = layer->visibleContentOpaqueRegion();
    if (opaqueContents.IsEmpty())
        return;

    DCHECK(layer->visibleContentRect().Contains(opaqueContents.bounds()));

    bool clipped;
    gfx::QuadF visibleTransformedQuad = MathUtil::mapQuad(layer->drawTransform(), gfx::QuadF(opaqueContents.bounds()), clipped);
    // FIXME: Find a rect interior to each transformed quad.
    if (clipped || !visibleTransformedQuad.IsRectilinear())
        return;

    gfx::Rect clipRectInTarget = gfx::IntersectRects(
        layer->renderTarget()->renderSurface()->contentRect(),
        screenSpaceClipRectInTargetSurface(layer->renderTarget()->renderSurface(), m_screenSpaceClipRect));

    for (Region::Iterator opaqueContentRects(opaqueContents); opaqueContentRects.has_rect(); opaqueContentRects.next()) {
        gfx::Rect transformedRect = gfx::ToEnclosedRect(MathUtil::mapQuad(layer->drawTransform(), gfx::QuadF(opaqueContentRects.rect()), clipped).BoundingBox());
        DCHECK(!clipped); // We only map if the transform preserves axis alignment.
        transformedRect.Intersect(clipRectInTarget);
        if (transformedRect.width() < m_minimumTrackingSize.width() && transformedRect.height() < m_minimumTrackingSize.height())
            continue;
        m_stack.back().occlusionFromInsideTarget.Union(transformedRect);

        if (!m_occludingScreenSpaceRects)
            continue;

        // Save the occluding area in screen space for debug visualization.
        gfx::QuadF screenSpaceQuad = MathUtil::mapQuad(layer->renderTarget()->renderSurface()->screenSpaceTransform(), gfx::QuadF(transformedRect), clipped);
        // TODO(danakj): Store the quad in the debug info instead of the bounding box.
        gfx::Rect screenSpaceRect = gfx::ToEnclosedRect(screenSpaceQuad.BoundingBox());
        m_occludingScreenSpaceRects->push_back(screenSpaceRect);
    }

    if (!m_nonOccludingScreenSpaceRects)
        return;

    Region nonOpaqueContents = SubtractRegions(gfx::Rect(layer->contentBounds()), opaqueContents);
    for (Region::Iterator nonOpaqueContentRects(nonOpaqueContents); nonOpaqueContentRects.has_rect(); nonOpaqueContentRects.next()) {
        // We've already checked for clipping in the mapQuad call above, these calls should not clip anything further.
        gfx::Rect transformedRect = gfx::ToEnclosedRect(MathUtil::mapClippedRect(layer->drawTransform(), gfx::RectF(nonOpaqueContentRects.rect())));
        transformedRect.Intersect(clipRectInTarget);
        if (transformedRect.IsEmpty())
            continue;

        gfx::QuadF screenSpaceQuad = MathUtil::mapQuad(layer->renderTarget()->renderSurface()->screenSpaceTransform(), gfx::QuadF(transformedRect), clipped);
        // TODO(danakj): Store the quad in the debug info instead of the bounding box.
        gfx::Rect screenSpaceRect = gfx::ToEnclosedRect(screenSpaceQuad.BoundingBox());
        m_nonOccludingScreenSpaceRects->push_back(screenSpaceRect);
    }
}

template<typename LayerType, typename RenderSurfaceType>
bool OcclusionTrackerBase<LayerType, RenderSurfaceType>::occluded(const LayerType* renderTarget, gfx::Rect contentRect, const gfx::Transform& drawTransform, bool implDrawTransformIsUnknown, bool isClipped, gfx::Rect clipRectInTarget, bool* hasOcclusionFromOutsideTargetSurface) const
{
    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = false;

    DCHECK(!m_stack.empty());
    if (m_stack.empty())
        return false;
    if (contentRect.IsEmpty())
        return true;
    if (implDrawTransformIsUnknown)
        return false;

    // For tests with no render target.
    if (!renderTarget)
        return false;

    DCHECK(renderTarget->renderTarget() == renderTarget);
    DCHECK(renderTarget->renderSurface());
    DCHECK(renderTarget == m_stack.back().target);

    gfx::Transform inverseDrawTransform(gfx::Transform::kSkipInitialization);
    if (!drawTransform.GetInverse(&inverseDrawTransform))
        return false;

    // Take the ToEnclosingRect at each step, as we want to contain any unoccluded partial pixels in the resulting Rect.
    Region unoccludedRegionInTargetSurface = gfx::ToEnclosingRect(MathUtil::mapClippedRect(drawTransform, gfx::RectF(contentRect)));
    // Layers can't clip across surfaces, so count this as internal occlusion.
    if (isClipped)
      unoccludedRegionInTargetSurface.Intersect(clipRectInTarget);
    unoccludedRegionInTargetSurface.Subtract(m_stack.back().occlusionFromInsideTarget);
    gfx::RectF unoccludedRectInTargetSurfaceWithoutOutsideOcclusion = unoccludedRegionInTargetSurface.bounds();
    unoccludedRegionInTargetSurface.Subtract(m_stack.back().occlusionFromOutsideTarget);

    // Treat other clipping as occlusion from outside the surface.
    // TODO(danakj): Clip to visibleContentRect?
    unoccludedRegionInTargetSurface.Intersect(renderTarget->renderSurface()->contentRect());
    unoccludedRegionInTargetSurface.Intersect(screenSpaceClipRectInTargetSurface(renderTarget->renderSurface(), m_screenSpaceClipRect));

    gfx::RectF unoccludedRectInTargetSurface = unoccludedRegionInTargetSurface.bounds();

    if (hasOcclusionFromOutsideTargetSurface) {
        // Check if the unoccluded rect shrank when applying outside occlusion.
        *hasOcclusionFromOutsideTargetSurface = !gfx::SubtractRects(unoccludedRectInTargetSurfaceWithoutOutsideOcclusion, unoccludedRectInTargetSurface).IsEmpty();
    }

    return unoccludedRectInTargetSurface.IsEmpty();
}

template<typename LayerType, typename RenderSurfaceType>
gfx::Rect OcclusionTrackerBase<LayerType, RenderSurfaceType>::unoccludedContentRect(const LayerType* renderTarget, gfx::Rect contentRect, const gfx::Transform& drawTransform, bool implDrawTransformIsUnknown, bool isClipped, gfx::Rect clipRectInTarget, bool* hasOcclusionFromOutsideTargetSurface) const
{
    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = false;

    DCHECK(!m_stack.empty());
    if (m_stack.empty())
        return contentRect;
    if (contentRect.IsEmpty())
        return contentRect;
    if (implDrawTransformIsUnknown)
        return contentRect;

    // For tests with no render target.
    if (!renderTarget)
        return contentRect;

    DCHECK(renderTarget->renderTarget() == renderTarget);
    DCHECK(renderTarget->renderSurface());
    DCHECK(renderTarget == m_stack.back().target);

    gfx::Transform inverseDrawTransform(gfx::Transform::kSkipInitialization);
    if (!drawTransform.GetInverse(&inverseDrawTransform))
        return contentRect;

    // Take the ToEnclosingRect at each step, as we want to contain any unoccluded partial pixels in the resulting Rect.
    Region unoccludedRegionInTargetSurface = gfx::ToEnclosingRect(MathUtil::mapClippedRect(drawTransform, gfx::RectF(contentRect)));
    // Layers can't clip across surfaces, so count this as internal occlusion.
    if (isClipped)
      unoccludedRegionInTargetSurface.Intersect(clipRectInTarget);
    unoccludedRegionInTargetSurface.Subtract(m_stack.back().occlusionFromInsideTarget);
    gfx::RectF unoccludedRectInTargetSurfaceWithoutOutsideOcclusion = unoccludedRegionInTargetSurface.bounds();
    unoccludedRegionInTargetSurface.Subtract(m_stack.back().occlusionFromOutsideTarget);

    // Treat other clipping as occlusion from outside the surface.
    // TODO(danakj): Clip to visibleContentRect?
    unoccludedRegionInTargetSurface.Intersect(renderTarget->renderSurface()->contentRect());
    unoccludedRegionInTargetSurface.Intersect(screenSpaceClipRectInTargetSurface(renderTarget->renderSurface(), m_screenSpaceClipRect));

    gfx::RectF unoccludedRectInTargetSurface = unoccludedRegionInTargetSurface.bounds();
    gfx::Rect unoccludedRect = gfx::ToEnclosingRect(MathUtil::projectClippedRect(inverseDrawTransform, unoccludedRectInTargetSurface));
    unoccludedRect.Intersect(contentRect);

    if (hasOcclusionFromOutsideTargetSurface) {
        // Check if the unoccluded rect shrank when applying outside occlusion.
        *hasOcclusionFromOutsideTargetSurface = !gfx::SubtractRects(unoccludedRectInTargetSurfaceWithoutOutsideOcclusion, unoccludedRectInTargetSurface).IsEmpty();
    }

    return unoccludedRect;
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

    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = false;

    if (contentRect.IsEmpty())
        return contentRect;

    const RenderSurfaceType* surface = layer->renderSurface();
    const LayerType* contributingSurfaceRenderTarget = layer->parent()->renderTarget();

    if (!surfaceTransformsToTargetKnown(surface))
        return contentRect;

    gfx::Transform drawTransform = forReplica ? surface->replicaDrawTransform() : surface->drawTransform();
    gfx::Transform inverseDrawTransform(gfx::Transform::kSkipInitialization);
    if (!drawTransform.GetInverse(&inverseDrawTransform))
        return contentRect;

    // A contributing surface doesn't get occluded by things inside its own surface, so only things outside the surface can occlude it. That occlusion is
    // found just below the top of the stack (if it exists).
    bool hasOcclusion = m_stack.size() > 1;

    // Take the ToEnclosingRect at each step, as we want to contain any unoccluded partial pixels in the resulting Rect.
    Region unoccludedRegionInTargetSurface = gfx::ToEnclosingRect(MathUtil::mapClippedRect(drawTransform, gfx::RectF(contentRect)));
    // Layers can't clip across surfaces, so count this as internal occlusion.
    if (surface->isClipped())
        unoccludedRegionInTargetSurface.Intersect(surface->clipRect());
    if (hasOcclusion) {
        const StackObject& secondLast = m_stack[m_stack.size() - 2];
        unoccludedRegionInTargetSurface.Subtract(secondLast.occlusionFromInsideTarget);
    }
    gfx::RectF unoccludedRectInTargetSurfaceWithoutOutsideOcclusion = unoccludedRegionInTargetSurface.bounds();
    if (hasOcclusion) {
        const StackObject& secondLast = m_stack[m_stack.size() - 2];
        unoccludedRegionInTargetSurface.Subtract(secondLast.occlusionFromOutsideTarget);
    }

    // Treat other clipping as occlusion from outside the target surface.
    unoccludedRegionInTargetSurface.Intersect(contributingSurfaceRenderTarget->renderSurface()->contentRect());
    unoccludedRegionInTargetSurface.Intersect(screenSpaceClipRectInTargetSurface(contributingSurfaceRenderTarget->renderSurface(), m_screenSpaceClipRect));

    gfx::RectF unoccludedRectInTargetSurface = unoccludedRegionInTargetSurface.bounds();
    gfx::Rect unoccludedRect = gfx::ToEnclosingRect(MathUtil::projectClippedRect(inverseDrawTransform, unoccludedRectInTargetSurface));
    unoccludedRect.Intersect(contentRect);

    if (hasOcclusionFromOutsideTargetSurface) {
        // Check if the unoccluded rect shrank when applying outside occlusion.
        *hasOcclusionFromOutsideTargetSurface = !gfx::SubtractRects(unoccludedRectInTargetSurfaceWithoutOutsideOcclusion, unoccludedRectInTargetSurface).IsEmpty();
    }

    return unoccludedRect;
}

// Instantiate (and export) templates here for the linker.
template class OcclusionTrackerBase<Layer, RenderSurface>;
template class OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>;

}  // namespace cc
