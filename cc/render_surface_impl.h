// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderSurface_h
#define CCRenderSurface_h

#include "FloatRect.h"
#include "IntRect.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/render_pass.h"
#include "cc/shared_quad_state.h"
#include <public/WebTransformationMatrix.h>

namespace cc {

class DamageTracker;
class DelegatedRendererLayerImpl;
class QuadSink;
class RenderPassSink;
class LayerImpl;

struct AppendQuadsData;

class RenderSurfaceImpl {
public:
    explicit RenderSurfaceImpl(LayerImpl*);
    virtual ~RenderSurfaceImpl();

    std::string name() const;
    void dumpSurface(std::string*, int indent) const;

    FloatPoint contentRectCenter() const { return FloatRect(m_contentRect).center(); }

    // Returns the rect that encloses the RenderSurfaceImpl including any reflection.
    FloatRect drawableContentRect() const;

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }

    void setNearestAncestorThatMovesPixels(RenderSurfaceImpl* surface) { m_nearestAncestorThatMovesPixels = surface; }
    const RenderSurfaceImpl* nearestAncestorThatMovesPixels() const { return m_nearestAncestorThatMovesPixels; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    void setDrawTransform(const WebKit::WebTransformationMatrix& drawTransform) { m_drawTransform = drawTransform; }
    const WebKit::WebTransformationMatrix& drawTransform() const { return m_drawTransform; }

    void setScreenSpaceTransform(const WebKit::WebTransformationMatrix& screenSpaceTransform) { m_screenSpaceTransform = screenSpaceTransform; }
    const WebKit::WebTransformationMatrix& screenSpaceTransform() const { return m_screenSpaceTransform; }

    void setReplicaDrawTransform(const WebKit::WebTransformationMatrix& replicaDrawTransform) { m_replicaDrawTransform = replicaDrawTransform; }
    const WebKit::WebTransformationMatrix& replicaDrawTransform() const { return m_replicaDrawTransform; }

    void setReplicaScreenSpaceTransform(const WebKit::WebTransformationMatrix& replicaScreenSpaceTransform) { m_replicaScreenSpaceTransform = replicaScreenSpaceTransform; }
    const WebKit::WebTransformationMatrix& replicaScreenSpaceTransform() const { return m_replicaScreenSpaceTransform; }

    bool targetSurfaceTransformsAreAnimating() const { return m_targetSurfaceTransformsAreAnimating; }
    void setTargetSurfaceTransformsAreAnimating(bool animating) { m_targetSurfaceTransformsAreAnimating = animating; }
    bool screenSpaceTransformsAreAnimating() const { return m_screenSpaceTransformsAreAnimating; }
    void setScreenSpaceTransformsAreAnimating(bool animating) { m_screenSpaceTransformsAreAnimating = animating; }

    void setClipRect(const IntRect&);
    const IntRect& clipRect() const { return m_clipRect; }

    bool contentsChanged() const;

    void setContentRect(const IntRect&);
    const IntRect& contentRect() const { return m_contentRect; }

    std::vector<LayerImpl*>& layerList() { return m_layerList; }
    void addContributingDelegatedRenderPassLayer(LayerImpl*);
    void clearLayerLists();

    int owningLayerId() const;

    void resetPropertyChangedFlag() { m_surfacePropertyChanged = false; }
    bool surfacePropertyChanged() const;
    bool surfacePropertyChangedOnlyFromDescendant() const;

    DamageTracker* damageTracker() const { return m_damageTracker.get(); }

    RenderPass::Id renderPassId();

    void appendRenderPasses(RenderPassSink&);
    void appendQuads(QuadSink&, AppendQuadsData&, bool forReplica, RenderPass::Id renderPassId);

private:
    LayerImpl* m_owningLayer;

    // Uses this surface's space.
    IntRect m_contentRect;
    bool m_surfacePropertyChanged;

    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;
    WebKit::WebTransformationMatrix m_drawTransform;
    WebKit::WebTransformationMatrix m_screenSpaceTransform;
    WebKit::WebTransformationMatrix m_replicaDrawTransform;
    WebKit::WebTransformationMatrix m_replicaScreenSpaceTransform;
    bool m_targetSurfaceTransformsAreAnimating;
    bool m_screenSpaceTransformsAreAnimating;

    // Uses the space of the surface's target surface.
    IntRect m_clipRect;

    std::vector<LayerImpl*> m_layerList;
    std::vector<DelegatedRendererLayerImpl*> m_contributingDelegatedRenderPassLayerList;

    // The nearest ancestor target surface that will contain the contents of this surface, and that is going
    // to move pixels within the surface (such as with a blur). This can point to itself.
    RenderSurfaceImpl* m_nearestAncestorThatMovesPixels;

    scoped_ptr<DamageTracker> m_damageTracker;

    // For LayerIteratorActions
    int m_targetRenderSurfaceLayerIndexHistory;
    int m_currentLayerIndexHistory;

    friend struct LayerIteratorActions;

    DISALLOW_COPY_AND_ASSIGN(RenderSurfaceImpl);
};

}
#endif
