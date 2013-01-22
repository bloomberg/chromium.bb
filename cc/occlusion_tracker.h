// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OCCLUSION_TRACKER_H_
#define CC_OCCLUSION_TRACKER_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/layer_iterator.h"
#include "cc/region.h"
#include "ui/gfx/rect.h"

namespace cc {
class OverdrawMetrics;
class LayerImpl;
class RenderSurfaceImpl;
class Layer;
class RenderSurface;

// This class is used to track occlusion of layers while traversing them in a front-to-back order. As each layer is visited, one of the
// methods in this class is called to notify it about the current target surface.
// Then, occlusion in the content space of the current layer may be queried, via methods such as occluded() and unoccludedContentRect().
// If the current layer owns a RenderSurfaceImpl, then occlusion on that RenderSurfaceImpl may also be queried via surfaceOccluded() and surfaceUnoccludedContentRect().
// Finally, once finished with the layer, occlusion behind the layer should be marked by calling markOccludedBehindLayer().
template<typename LayerType, typename RenderSurfaceType>
class CC_EXPORT OcclusionTrackerBase {
public:
    OcclusionTrackerBase(gfx::Rect screenSpaceClipRect, bool recordMetricsForFrame);
    ~OcclusionTrackerBase();

    // Called at the beginning of each step in the LayerIterator's front-to-back traversal.
    void enterLayer(const LayerIteratorPosition<LayerType>&);
    // Called at the end of each step in the LayerIterator's front-to-back traversal.
    void leaveLayer(const LayerIteratorPosition<LayerType>&);

    // Returns true if the given rect in content space for a layer is fully occluded in either screen space or the layer's target surface.  |renderTarget| is the contributing layer's render target, and |drawTransform|, |transformsToTargetKnown| and |clippedRectInTarget| are relative to that.
    bool occluded(const LayerType* renderTarget, gfx::Rect contentRect, const gfx::Transform& drawTransform, bool implDrawTransformIsUnknown, bool isClipped, gfx::Rect clipRectInTarget, bool* hasOcclusionFromOutsideTargetSurface = 0) const;
    // Gives an unoccluded sub-rect of |contentRect| in the content space of a layer. Used when considering occlusion for a layer that paints/draws something. |renderTarget| is the contributing layer's render target, and |drawTransform|, |transformsToTargetKnown| and |clippedRectInTarget| are relative to that.
    gfx::Rect unoccludedContentRect(const LayerType* renderTarget, gfx::Rect contentRect, const gfx::Transform& drawTransform, bool implDrawTransformIsUnknown, bool isClipped, gfx::Rect clipRectInTarget, bool* hasOcclusionFromOutsideTargetSurface = 0) const;

    // Gives an unoccluded sub-rect of |contentRect| in the content space of the renderTarget owned by the layer.
    // Used when considering occlusion for a contributing surface that is rendering into another target.
    gfx::Rect unoccludedContributingSurfaceContentRect(const LayerType*, bool forReplica, const gfx::Rect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const;

    // Report operations for recording overdraw metrics.
    OverdrawMetrics& overdrawMetrics() const { return *m_overdrawMetrics.get(); }

    // Gives the region of the screen that is not occluded by something opaque.
    Region computeVisibleRegionInScreen() const {
        DCHECK(!m_stack.back().target->parent());
        return SubtractRegions(m_screenSpaceClipRect, m_stack.back().occlusionFromInsideTarget);
    }

    void setMinimumTrackingSize(const gfx::Size& size) { m_minimumTrackingSize = size; }

    // The following is used for visualization purposes. 
    void setOccludingScreenSpaceRectsContainer(std::vector<gfx::Rect>* rects) { m_occludingScreenSpaceRects = rects; }
    void setNonOccludingScreenSpaceRectsContainer(std::vector<gfx::Rect>* rects) { m_nonOccludingScreenSpaceRects = rects; }

protected:
    struct StackObject {
        StackObject() : target(0) { }
        StackObject(const LayerType* target) : target(target) { }
        const LayerType* target;
        Region occlusionFromOutsideTarget;
        Region occlusionFromInsideTarget;
    };

    // The stack holds occluded regions for subtrees in the RenderSurfaceImpl-Layer tree, so that when we leave a subtree we may
    // apply a mask to it, but not to the parts outside the subtree.
    // - The first time we see a new subtree under a target, we add that target to the top of the stack. This can happen as a layer representing itself, or as a target surface.
    // - When we visit a target surface, we apply its mask to its subtree, which is at the top of the stack.
    // - When we visit a layer representing itself, we add its occlusion to the current subtree, which is at the top of the stack.
    // - When we visit a layer representing a contributing surface, the current target will never be the top of the stack since we just came from the contributing surface.
    // We merge the occlusion at the top of the stack with the new current subtree. This new target is pushed onto the stack if not already there.
    std::vector<StackObject> m_stack;

private:
    // Called when visiting a layer representing itself. If the target was not already current, then this indicates we have entered a new surface subtree.
    void enterRenderTarget(const LayerType* newTarget);

    // Called when visiting a layer representing a target surface. This indicates we have visited all the layers within the surface, and we may
    // perform any surface-wide operations.
    void finishedRenderTarget(const LayerType* finishedTarget);

    // Called when visiting a layer representing a contributing surface. This indicates that we are leaving our current surface, and
    // entering the new one. We then perform any operations required for merging results from the child subtree into its parent.
    void leaveToRenderTarget(const LayerType* newTarget);

    // Add the layer's occlusion to the tracked state.
    void markOccludedBehindLayer(const LayerType*);

    gfx::Rect m_screenSpaceClipRect;
    scoped_ptr<OverdrawMetrics> m_overdrawMetrics;
    gfx::Size m_minimumTrackingSize;

    // This is used for visualizing the occlusion tracking process.
    std::vector<gfx::Rect>* m_occludingScreenSpaceRects;
    std::vector<gfx::Rect>* m_nonOccludingScreenSpaceRects;

    DISALLOW_COPY_AND_ASSIGN(OcclusionTrackerBase);
};

typedef OcclusionTrackerBase<Layer, RenderSurface> OcclusionTracker;
typedef OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl> OcclusionTrackerImpl;
#if !defined(COMPILER_MSVC)
extern template class OcclusionTrackerBase<Layer, RenderSurface>;
extern template class OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>;
#endif

}  // namespace cc

#endif  // CC_OCCLUSION_TRACKER_H_
