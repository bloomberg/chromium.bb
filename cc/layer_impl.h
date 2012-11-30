// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_IMPL_H_
#define CC_LAYER_IMPL_H_

#include <public/WebFilterOperations.h>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/input_handler.h"
#include "cc/layer_animation_controller.h"
#include "cc/region.h"
#include "cc/render_pass.h"
#include "cc/render_surface_impl.h"
#include "cc/resource_provider.h"
#include "cc/scoped_ptr_vector.h"
#include "cc/shared_quad_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {

class LayerSorter;
class LayerTreeHostImpl;
class QuadSink;
class Renderer;
class ScrollbarAnimationController;
class ScrollbarLayerImpl;
class Layer;

struct AppendQuadsData;

class CC_EXPORT LayerImpl : public LayerAnimationControllerClient {
public:
    typedef ScopedPtrVector<LayerImpl> LayerList;

    static scoped_ptr<LayerImpl> create(int id)
    {
        return make_scoped_ptr(new LayerImpl(id));
    }

    virtual ~LayerImpl();

    // LayerAnimationControllerClient implementation.
    virtual int id() const OVERRIDE;
    virtual void setOpacityFromAnimation(float) OVERRIDE;
    virtual float opacity() const OVERRIDE;
    virtual void setTransformFromAnimation(const gfx::Transform&) OVERRIDE;
    virtual const gfx::Transform& transform() const OVERRIDE;

    // Tree structure.
    LayerImpl* parent() { return m_parent; }
    const LayerImpl* parent() const { return m_parent; }
    const LayerList& children() const { return m_children; }
    void addChild(scoped_ptr<LayerImpl>);
    void removeFromParent();
    void removeAllChildren();

    void setMaskLayer(scoped_ptr<LayerImpl>);
    LayerImpl* maskLayer() { return m_maskLayer.get(); }
    const LayerImpl* maskLayer() const { return m_maskLayer.get(); }

    void setReplicaLayer(scoped_ptr<LayerImpl>);
    LayerImpl* replicaLayer() { return m_replicaLayer.get(); }
    const LayerImpl* replicaLayer() const { return m_replicaLayer.get(); }

    bool hasMask() const { return m_maskLayer; }
    bool hasReplica() const { return m_replicaLayer; }
    bool replicaHasMask() const { return m_replicaLayer && (m_maskLayer || m_replicaLayer->m_maskLayer); }

    LayerTreeHostImpl* layerTreeHostImpl() const { return m_layerTreeHostImpl; }
    void setLayerTreeHostImpl(LayerTreeHostImpl* hostImpl) { m_layerTreeHostImpl = hostImpl; }

    scoped_ptr<SharedQuadState> createSharedQuadState() const;
    // willDraw must be called before appendQuads. If willDraw is called,
    // didDraw is guaranteed to be called before another willDraw or before
    // the layer is destroyed. To enforce this, any class that overrides
    // willDraw/didDraw must call the base class version.
    virtual void willDraw(ResourceProvider*);
    virtual void appendQuads(QuadSink&, AppendQuadsData&) { }
    virtual void didDraw(ResourceProvider*);

    virtual ResourceProvider::ResourceId contentsResourceId() const;

    virtual bool hasContributingDelegatedRenderPasses() const;
    virtual RenderPass::Id firstContributingRenderPassId() const;
    virtual RenderPass::Id nextContributingRenderPassId(RenderPass::Id) const;

    // Returns true if this layer has content to draw.
    void setDrawsContent(bool);
    bool drawsContent() const { return m_drawsContent; }

    bool forceRenderSurface() const { return m_forceRenderSurface; }
    void setForceRenderSurface(bool force) { m_forceRenderSurface = force; }

    // Returns 0 if none of the layer's descendants has content to draw,
    // 1 if exactly one descendant has content to draw, or a number >1
    // (but necessary the exact number of descendants) otherwise.
    virtual int descendantsDrawContent();

    void setAnchorPoint(const gfx::PointF&);
    const gfx::PointF& anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float);
    float anchorPointZ() const { return m_anchorPointZ; }

    void setBackgroundColor(SkColor);
    SkColor backgroundColor() const { return m_backgroundColor; }

    void setFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& filters() const { return m_filters; }

    void setBackgroundFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& backgroundFilters() const { return m_backgroundFilters; }

    void setFilter(SkImageFilter*);
    SkImageFilter* filter() const { return m_filter; }

    void setMasksToBounds(bool);
    bool masksToBounds() const { return m_masksToBounds; }

    void setContentsOpaque(bool);
    bool contentsOpaque() const { return m_contentsOpaque; }

    void setOpacity(float);
    bool opacityIsAnimating() const;

    void setPosition(const gfx::PointF&);
    const gfx::PointF& position() const { return m_position; }

    void setIsContainerForFixedPositionLayers(bool isContainerForFixedPositionLayers) { m_isContainerForFixedPositionLayers = isContainerForFixedPositionLayers; }
    bool isContainerForFixedPositionLayers() const { return m_isContainerForFixedPositionLayers; }

    void setFixedToContainerLayer(bool fixedToContainerLayer = true) { m_fixedToContainerLayer = fixedToContainerLayer;}
    bool fixedToContainerLayer() const { return m_fixedToContainerLayer; }

    void setPreserves3D(bool);
    bool preserves3D() const { return m_preserves3D; }

    void setUseParentBackfaceVisibility(bool useParentBackfaceVisibility) { m_useParentBackfaceVisibility = useParentBackfaceVisibility; }
    bool useParentBackfaceVisibility() const { return m_useParentBackfaceVisibility; }

    void setUseLCDText(bool useLCDText) { m_useLCDText = useLCDText; }
    bool useLCDText() const { return m_useLCDText; }

    void setSublayerTransform(const gfx::Transform&);
    const gfx::Transform& sublayerTransform() const { return m_sublayerTransform; }

    // Debug layer name.
    void setDebugName(const std::string& debugName) { m_debugName = debugName; }
    std::string debugName() const { return m_debugName; }

    bool showDebugBorders() const;

    RenderSurfaceImpl* renderSurface() const { return m_renderSurface.get(); }
    void createRenderSurface();
    void clearRenderSurface() { m_renderSurface.reset(); }

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    void setRenderTarget(LayerImpl* target) { m_renderTarget = target; }
    LayerImpl* renderTarget() { DCHECK(!m_renderTarget || m_renderTarget->renderSurface()); return m_renderTarget; }
    const LayerImpl* renderTarget() const { DCHECK(!m_renderTarget || m_renderTarget->renderSurface()); return m_renderTarget; }

    // The client should be responsible for setting bounds, contentBounds and
    // contentsScale to appropriate values. LayerImpl doesn't calculate any of
    // them from the other values.

    void setBounds(const gfx::Size&);
    const gfx::Size& bounds() const { return m_bounds; }

    // ContentBounds may be [0, 1) pixels larger than bounds * contentsScale.
    // Don't calculate scale from it. Use contentsScale instead for accuracy.
    void setContentBounds(const gfx::Size&);
    gfx::Size contentBounds() const { return m_contentBounds; }

    float contentsScaleX() const { return m_contentsScaleX; }
    float contentsScaleY() const { return m_contentsScaleY; }
    void setContentsScale(float contentsScaleX, float contentsScaleY);

    gfx::Vector2d scrollOffset() const { return m_scrollOffset; }
    void setScrollOffset(gfx::Vector2d);

    gfx::Vector2d maxScrollOffset() const {return m_maxScrollOffset; }
    void setMaxScrollOffset(gfx::Vector2d);

    const gfx::Vector2dF& scrollDelta() const { return m_scrollDelta; }
    void setScrollDelta(const gfx::Vector2dF&);

    const gfx::Transform& implTransform() const { return m_implTransform; }
    void setImplTransform(const gfx::Transform& transform);

    const gfx::Vector2d& sentScrollDelta() const { return m_sentScrollDelta; }
    void setSentScrollDelta(const gfx::Vector2d& sentScrollDelta) { m_sentScrollDelta = sentScrollDelta; }

    // Returns the delta of the scroll that was outside of the bounds of the initial scroll
    gfx::Vector2dF scrollBy(const gfx::Vector2dF& scroll);

    bool scrollable() const { return m_scrollable; }
    void setScrollable(bool scrollable) { m_scrollable = scrollable; }

    bool shouldScrollOnMainThread() const { return m_shouldScrollOnMainThread; }
    void setShouldScrollOnMainThread(bool shouldScrollOnMainThread) { m_shouldScrollOnMainThread = shouldScrollOnMainThread; }

    bool haveWheelEventHandlers() const { return m_haveWheelEventHandlers; }
    void setHaveWheelEventHandlers(bool haveWheelEventHandlers) { m_haveWheelEventHandlers = haveWheelEventHandlers; }

    const Region& nonFastScrollableRegion() const { return m_nonFastScrollableRegion; }
    void setNonFastScrollableRegion(const Region& region) { m_nonFastScrollableRegion = region; }

    const Region& touchEventHandlerRegion() const { return m_touchEventHandlerRegion; }
    void setTouchEventHandlerRegion(const Region& region) { m_touchEventHandlerRegion = region; }

    void setDrawCheckerboardForMissingTiles(bool checkerboard) { m_drawCheckerboardForMissingTiles = checkerboard; }
    bool drawCheckerboardForMissingTiles() const;

    InputHandlerClient::ScrollStatus tryScroll(const gfx::PointF& screenSpacePoint, InputHandlerClient::ScrollInputType) const;

    const gfx::Rect& visibleContentRect() const { return m_visibleContentRect; }
    void setVisibleContentRect(const gfx::Rect& visibleContentRect) { m_visibleContentRect = visibleContentRect; }

    bool doubleSided() const { return m_doubleSided; }
    void setDoubleSided(bool);

    void setTransform(const gfx::Transform&);
    bool transformIsAnimating() const;

    const gfx::Transform& drawTransform() const { return m_drawTransform; }
    void setDrawTransform(const gfx::Transform& matrix) { m_drawTransform = matrix; }
    const gfx::Transform& screenSpaceTransform() const { return m_screenSpaceTransform; }
    void setScreenSpaceTransform(const gfx::Transform& matrix) { m_screenSpaceTransform = matrix; }

    bool drawTransformIsAnimating() const { return m_drawTransformIsAnimating; }
    void setDrawTransformIsAnimating(bool animating) { m_drawTransformIsAnimating = animating; }
    bool screenSpaceTransformIsAnimating() const { return m_screenSpaceTransformIsAnimating; }
    void setScreenSpaceTransformIsAnimating(bool animating) { m_screenSpaceTransformIsAnimating = animating; }

    bool isClipped() const { return m_isClipped; }
    void setIsClipped(bool isClipped) { m_isClipped = isClipped; }

    const gfx::Rect& clipRect() const { return m_clipRect; }
    void setClipRect(const gfx::Rect& clipRect) { m_clipRect = clipRect; }

    const gfx::Rect& drawableContentRect() const { return m_drawableContentRect; }
    void setDrawableContentRect(const gfx::Rect& rect) { m_drawableContentRect = rect; }

    const gfx::RectF& updateRect() const { return m_updateRect; }
    void setUpdateRect(const gfx::RectF& updateRect) { m_updateRect = updateRect; }

    std::string layerTreeAsText() const;

    void setStackingOrderChanged(bool);

    bool layerPropertyChanged() const { return m_layerPropertyChanged || layerIsAlwaysDamaged(); }
    bool layerSurfacePropertyChanged() const;

    void resetAllChangeTrackingForSubtree();

    virtual bool layerIsAlwaysDamaged() const;

    LayerAnimationController* layerAnimationController() { return m_layerAnimationController.get(); }

    virtual Region visibleContentOpaqueRegion() const;

    virtual void didUpdateTransforms() { }

    // Indicates that the context previously used to render this layer
    // was lost and that a new one has been created. Won't be called
    // until the new context has been created successfully.
    virtual void didLoseContext();

    ScrollbarAnimationController* scrollbarAnimationController() const { return m_scrollbarAnimationController.get(); }

    void setHorizontalScrollbarLayer(ScrollbarLayerImpl*);
    ScrollbarLayerImpl* horizontalScrollbarLayer();
    const ScrollbarLayerImpl* horizontalScrollbarLayer() const;

    void setVerticalScrollbarLayer(ScrollbarLayerImpl*);
    ScrollbarLayerImpl* verticalScrollbarLayer();
    const ScrollbarLayerImpl* verticalScrollbarLayer() const;

    gfx::Rect layerRectToContentRect(const gfx::RectF& layerRect) const;

protected:
    explicit LayerImpl(int);

    // Get the color and size of the layer's debug border.
    virtual void getDebugBorderProperties(SkColor*, float* width) const;

    void appendDebugBorderQuad(QuadSink&, const SharedQuadState*, AppendQuadsData&) const;

    virtual void dumpLayerProperties(std::string*, int indent) const;
    static std::string indentString(int indent);

private:
    void setParent(LayerImpl* parent) { m_parent = parent; }
    friend class TreeSynchronizer;
    void clearChildList(); // Warning: This does not preserve tree structure invariants and so is only exposed to the tree synchronizer.

    void noteLayerPropertyChangedForSubtree();

    // Note carefully this does not affect the current layer.
    void noteLayerPropertyChangedForDescendants();

    virtual const char* layerTypeAsString() const;

    void dumpLayer(std::string*, int indent) const;

    // Properties internal to LayerImpl
    LayerImpl* m_parent;
    LayerList m_children;
    // m_maskLayer can be temporarily stolen during tree sync, we need this ID to confirm newly assigned layer is still the previous one
    int m_maskLayerId;
    scoped_ptr<LayerImpl> m_maskLayer;
    int m_replicaLayerId; // ditto
    scoped_ptr<LayerImpl> m_replicaLayer;
    int m_layerId;
    LayerTreeHostImpl* m_layerTreeHostImpl;

    // Properties synchronized from the associated Layer.
    gfx::PointF m_anchorPoint;
    float m_anchorPointZ;
    gfx::Size m_bounds;
    gfx::Size m_contentBounds;
    float m_contentsScaleX;
    float m_contentsScaleY;
    gfx::Vector2d m_scrollOffset;
    bool m_scrollable;
    bool m_shouldScrollOnMainThread;
    bool m_haveWheelEventHandlers;
    Region m_nonFastScrollableRegion;
    Region m_touchEventHandlerRegion;
    SkColor m_backgroundColor;

    // Whether the "back" of this layer should draw.
    bool m_doubleSided;

    // Tracks if drawing-related properties have changed since last redraw.
    bool m_layerPropertyChanged;

    // Indicates that a property has changed on this layer that would not
    // affect the pixels on its target surface, but would require redrawing
    // but would require redrawing the targetSurface onto its ancestor targetSurface.
    // For layers that do not own a surface this flag acts as m_layerPropertyChanged.
    bool m_layerSurfacePropertyChanged;

    // Uses layer's content space.
    gfx::Rect m_visibleContentRect;
    bool m_masksToBounds;
    bool m_contentsOpaque;
    float m_opacity;
    gfx::PointF m_position;
    bool m_preserves3D;
    bool m_useParentBackfaceVisibility;
    bool m_drawCheckerboardForMissingTiles;
    gfx::Transform m_sublayerTransform;
    gfx::Transform m_transform;
    bool m_useLCDText;

    bool m_drawsContent;
    bool m_forceRenderSurface;

    // Set for the layer that other layers are fixed to.
    bool m_isContainerForFixedPositionLayers;
    // This is true if the layer should be fixed to the closest ancestor container.
    bool m_fixedToContainerLayer;

    gfx::Vector2dF m_scrollDelta;
    gfx::Vector2d m_sentScrollDelta;
    gfx::Vector2d m_maxScrollOffset;
    gfx::Transform m_implTransform;

    // The layer whose coordinate space this layer draws into. This can be
    // either the same layer (m_renderTarget == this) or an ancestor of this
    // layer.
    LayerImpl* m_renderTarget;

    // The global depth value of the center of the layer. This value is used
    // to sort layers from back to front.
    float m_drawDepth;
    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;

    // Debug layer name.
    std::string m_debugName;

    WebKit::WebFilterOperations m_filters;
    WebKit::WebFilterOperations m_backgroundFilters;
    SkImageFilter* m_filter;

    gfx::Transform m_drawTransform;
    gfx::Transform m_screenSpaceTransform;
    bool m_drawTransformIsAnimating;
    bool m_screenSpaceTransformIsAnimating;

#ifndef NDEBUG
    bool m_betweenWillDrawAndDidDraw;
#endif

    // Render surface associated with this layer. The layer and its descendants
    // will render to this surface.
    scoped_ptr<RenderSurfaceImpl> m_renderSurface;

    // Uses target surface's space.
    gfx::Rect m_drawableContentRect;
    gfx::Rect m_clipRect;

    // True if the layer is clipped by m_clipRect.
    bool m_isClipped;

    // Rect indicating what was repainted/updated during update.
    // Note that plugin layers bypass this and leave it empty.
    // Uses layer's content space.
    gfx::RectF m_updateRect;

    // Manages animations for this layer.
    scoped_ptr<LayerAnimationController> m_layerAnimationController;

    // Manages scrollbars for this layer
    scoped_ptr<ScrollbarAnimationController> m_scrollbarAnimationController;

    DISALLOW_COPY_AND_ASSIGN(LayerImpl);
};

void sortLayers(std::vector<LayerImpl*>::iterator first, std::vector<LayerImpl*>::iterator end, LayerSorter*);

}

#endif  // CC_LAYER_IMPL_H_
