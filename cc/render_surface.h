// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_RENDER_SURFACE_H_
#define CC_RENDER_SURFACE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {

class Layer;

class CC_EXPORT RenderSurface {
public:
    explicit RenderSurface(Layer*);
    ~RenderSurface();

    // Returns the rect that encloses the RenderSurfaceImpl including any reflection.
    gfx::RectF drawableContentRect() const;

    const gfx::Rect& contentRect() const { return m_contentRect; }
    void setContentRect(const gfx::Rect& contentRect) { m_contentRect = contentRect; }

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float drawOpacity) { m_drawOpacity = drawOpacity; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    // This goes from content space with the origin in the center of the rect being transformed to the target space with the origin in the top left of the
    // rect being transformed. Position the rect so that the origin is in the center of it before applying this transform.
    const gfx::Transform& drawTransform() const { return m_drawTransform; }
    void setDrawTransform(const gfx::Transform& drawTransform) { m_drawTransform = drawTransform; }

    const gfx::Transform& screenSpaceTransform() const { return m_screenSpaceTransform; }
    void setScreenSpaceTransform(const gfx::Transform& screenSpaceTransform) { m_screenSpaceTransform = screenSpaceTransform; }

    const gfx::Transform& replicaDrawTransform() const { return m_replicaDrawTransform; }
    void setReplicaDrawTransform(const gfx::Transform& replicaDrawTransform) { m_replicaDrawTransform = replicaDrawTransform; }

    const gfx::Transform& replicaScreenSpaceTransform() const { return m_replicaScreenSpaceTransform; }
    void setReplicaScreenSpaceTransform(const gfx::Transform& replicaScreenSpaceTransform) { m_replicaScreenSpaceTransform = replicaScreenSpaceTransform; }

    bool targetSurfaceTransformsAreAnimating() const { return m_targetSurfaceTransformsAreAnimating; }
    void setTargetSurfaceTransformsAreAnimating(bool animating) { m_targetSurfaceTransformsAreAnimating = animating; }
    bool screenSpaceTransformsAreAnimating() const { return m_screenSpaceTransformsAreAnimating; }
    void setScreenSpaceTransformsAreAnimating(bool animating) { m_screenSpaceTransformsAreAnimating = animating; }

    bool isClipped() const { return m_isClipped; }
    void setIsClipped(bool isClipped) { m_isClipped = isClipped; }

    const gfx::Rect& clipRect() const { return m_clipRect; }
    void setClipRect(const gfx::Rect& clipRect) { m_clipRect = clipRect; }

    typedef std::vector<scoped_refptr<Layer> > LayerList;
    LayerList& layerList() { return m_layerList; }
    // A no-op since DelegatedRendererLayers on the main thread don't have any
    // RenderPasses so they can't contribute to a surface.
    void addContributingDelegatedRenderPassLayer(Layer*) { }
    void clearLayerLists() { m_layerList.clear(); }

    void setNearestAncestorThatMovesPixels(RenderSurface* surface) { m_nearestAncestorThatMovesPixels = surface; }
    const RenderSurface* nearestAncestorThatMovesPixels() const { return m_nearestAncestorThatMovesPixels; }

private:
    friend struct LayerIteratorActions;

    Layer* m_owningLayer;

    // Uses this surface's space.
    gfx::Rect m_contentRect;

    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;
    gfx::Transform m_drawTransform;
    gfx::Transform m_screenSpaceTransform;
    gfx::Transform m_replicaDrawTransform;
    gfx::Transform m_replicaScreenSpaceTransform;
    bool m_targetSurfaceTransformsAreAnimating;
    bool m_screenSpaceTransformsAreAnimating;

    bool m_isClipped;

    // Uses the space of the surface's target surface.
    gfx::Rect m_clipRect;

    LayerList m_layerList;

    // The nearest ancestor target surface that will contain the contents of this surface, and that is going
    // to move pixels within the surface (such as with a blur). This can point to itself.
    RenderSurface* m_nearestAncestorThatMovesPixels;

    // For LayerIteratorActions
    int m_targetRenderSurfaceLayerIndexHistory;
    int m_currentLayerIndexHistory;

    DISALLOW_COPY_AND_ASSIGN(RenderSurface);
};

}
#endif  // CC_RENDER_SURFACE_H_
