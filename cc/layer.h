// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_H_
#define CC_LAYER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/cc_export.h"
#include "cc/draw_properties.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_animation_event_observer.h"
#include "cc/layer_animation_value_observer.h"
#include "cc/occlusion_tracker.h"
#include "cc/region.h"
#include "cc/render_surface.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace WebKit {
class WebAnimationDelegate;
class WebLayerScrollClient;
}

namespace cc {

class Animation;
struct AnimationEvent;
class LayerAnimationDelegate;
class LayerImpl;
class LayerTreeHost;
class LayerTreeImpl;
class PriorityCalculator;
class ResourceUpdateQueue;
class ScrollbarLayer;
struct AnimationEvent;
struct RenderingStats;

// Base class for composited layers. Special layer types are derived from
// this class.
class CC_EXPORT Layer : public base::RefCounted<Layer>,
                        public LayerAnimationValueObserver {
public:
    typedef std::vector<scoped_refptr<Layer> > LayerList;

    static scoped_refptr<Layer> create();

    int id() const;

    Layer* rootLayer();
    Layer* parent() { return m_parent; }
    const Layer* parent() const { return m_parent; }
    void addChild(scoped_refptr<Layer>);
    void insertChild(scoped_refptr<Layer>, size_t index);
    void replaceChild(Layer* reference, scoped_refptr<Layer> newLayer);
    void removeFromParent();
    void removeAllChildren();
    void setChildren(const LayerList&);

    const LayerList& children() const { return m_children; }

    void setAnchorPoint(const gfx::PointF&);
    gfx::PointF anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float);
    float anchorPointZ() const { return m_anchorPointZ; }

    virtual void setBackgroundColor(SkColor);
    SkColor backgroundColor() const { return m_backgroundColor; }

    // A layer's bounds are in logical, non-page-scaled pixels (however, the
    // root layer's bounds are in physical pixels).
    void setBounds(const gfx::Size&);
    // TODO(enne): remove this function: http://crbug.com/166023
    virtual void didUpdateBounds();
    const gfx::Size& bounds() const { return m_bounds; }

    void setMasksToBounds(bool);
    bool masksToBounds() const { return m_masksToBounds; }

    void setMaskLayer(Layer*);
    Layer* maskLayer() { return m_maskLayer.get(); }
    const Layer* maskLayer() const { return m_maskLayer.get(); }

    virtual void setNeedsDisplayRect(const gfx::RectF& dirtyRect);
    void setNeedsDisplay() { setNeedsDisplayRect(gfx::RectF(gfx::PointF(), bounds())); }
    virtual bool needsDisplay() const;

    void setOpacity(float);
    float opacity() const;
    bool opacityIsAnimating() const;

    void setFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& filters() const { return m_filters; }

    void setFilter(const skia::RefPtr<SkImageFilter>& filter);
    skia::RefPtr<SkImageFilter> filter() const { return m_filter; }

    // Background filters are filters applied to what is behind this layer, when they are viewed through non-opaque
    // regions in this layer. They are used through the WebLayer interface, and are not exposed to HTML.
    void setBackgroundFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& backgroundFilters() const { return m_backgroundFilters; }

    virtual void setContentsOpaque(bool);
    bool contentsOpaque() const { return m_contentsOpaque; }

    void setPosition(const gfx::PointF&);
    gfx::PointF position() const { return m_position; }

    void setIsContainerForFixedPositionLayers(bool);
    bool isContainerForFixedPositionLayers() const { return m_isContainerForFixedPositionLayers; }

    void setFixedToContainerLayer(bool);
    bool fixedToContainerLayer() const { return m_fixedToContainerLayer; }

    void setSublayerTransform(const gfx::Transform&);
    const gfx::Transform& sublayerTransform() const { return m_sublayerTransform; }

    void setTransform(const gfx::Transform&);
    const gfx::Transform& transform() const;
    bool transformIsAnimating() const;

    DrawProperties<Layer, RenderSurface>& drawProperties() { return m_drawProperties; }
    const DrawProperties<Layer, RenderSurface>& drawProperties() const { return m_drawProperties; }

    // The following are shortcut accessors to get various information from m_drawProperties
    const gfx::Transform& drawTransform() const { return m_drawProperties.target_space_transform; }
    const gfx::Transform& screenSpaceTransform() const { return m_drawProperties.screen_space_transform; }
    float drawOpacity() const { return m_drawProperties.opacity; }
    bool drawOpacityIsAnimating() const { return m_drawProperties.opacity_is_animating; }
    bool drawTransformIsAnimating() const { return m_drawProperties.target_space_transform_is_animating; }
    bool screenSpaceTransformIsAnimating() const { return m_drawProperties.screen_space_transform_is_animating; }
    bool screenSpaceOpacityIsAnimating() const { return m_drawProperties.screen_space_opacity_is_animating; }
    bool canUseLCDText() const { return m_drawProperties.can_use_lcd_text; }
    bool isClipped() const { return m_drawProperties.is_clipped; }
    const gfx::Rect& clipRect() const { return m_drawProperties.clip_rect; }
    const gfx::Rect& drawableContentRect() const { return m_drawProperties.drawable_content_rect; }
    const gfx::Rect& visibleContentRect() const { return m_drawProperties.visible_content_rect; }
    Layer* renderTarget() { DCHECK(!m_drawProperties.render_target || m_drawProperties.render_target->renderSurface()); return m_drawProperties.render_target; }
    const Layer* renderTarget() const { DCHECK(!m_drawProperties.render_target || m_drawProperties.render_target->renderSurface()); return m_drawProperties.render_target; }
    RenderSurface* renderSurface() const { return m_drawProperties.render_surface.get(); }

    void setScrollOffset(gfx::Vector2d);
    gfx::Vector2d scrollOffset() const { return m_scrollOffset; }

    void setMaxScrollOffset(gfx::Vector2d);
    gfx::Vector2d maxScrollOffset() const { return m_maxScrollOffset; }

    void setScrollable(bool);
    bool scrollable() const { return m_scrollable; }

    void setShouldScrollOnMainThread(bool);
    bool shouldScrollOnMainThread() const { return m_shouldScrollOnMainThread; }

    void setHaveWheelEventHandlers(bool);
    bool haveWheelEventHandlers() const { return m_haveWheelEventHandlers; }

    void setNonFastScrollableRegion(const Region&);
    const Region& nonFastScrollableRegion() const { return m_nonFastScrollableRegion; }

    void setTouchEventHandlerRegion(const Region&);
    const Region& touchEventHandlerRegion() const { return m_touchEventHandlerRegion; }

    void setLayerScrollClient(WebKit::WebLayerScrollClient* layerScrollClient) { m_layerScrollClient = layerScrollClient; }

    void setDrawCheckerboardForMissingTiles(bool);
    bool drawCheckerboardForMissingTiles() const { return m_drawCheckerboardForMissingTiles; }

    bool forceRenderSurface() const { return m_forceRenderSurface; }
    void setForceRenderSurface(bool);

    gfx::Vector2d scrollDelta() const { return gfx::Vector2d(); }

    void setImplTransform(const gfx::Transform&);
    const gfx::Transform& implTransform() const { return m_implTransform; }

    void setDoubleSided(bool);
    bool doubleSided() const { return m_doubleSided; }

    void setPreserves3D(bool preserve3D) { m_preserves3D = preserve3D; }
    bool preserves3D() const { return m_preserves3D; }

    void setUseParentBackfaceVisibility(bool useParentBackfaceVisibility) { m_useParentBackfaceVisibility = useParentBackfaceVisibility; }
    bool useParentBackfaceVisibility() const { return m_useParentBackfaceVisibility; }

    virtual void setLayerTreeHost(LayerTreeHost*);

    bool hasDelegatedContent() const { return false; }
    bool hasContributingDelegatedRenderPasses() const { return false; }

    void setIsDrawable(bool);

    void setReplicaLayer(Layer*);
    Layer* replicaLayer() { return m_replicaLayer.get(); }
    const Layer* replicaLayer() const { return m_replicaLayer.get(); }

    bool hasMask() const { return !!m_maskLayer; }
    bool hasReplica() const { return !!m_replicaLayer; }
    bool replicaHasMask() const { return m_replicaLayer && (m_maskLayer || m_replicaLayer->m_maskLayer); }

    // These methods typically need to be overwritten by derived classes.
    virtual bool drawsContent() const;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) { }
    virtual bool needMoreUpdates();
    virtual void setIsMask(bool) { }

    void setDebugName(const std::string&);

    virtual void pushPropertiesTo(LayerImpl*);

    void clearRenderSurface() { m_drawProperties.render_surface.reset(); }
    void createRenderSurface();

    // The contentsScale converts from logical, non-page-scaled pixels to target pixels.
    // The contentsScale is 1 for the root layer as it is already in physical pixels.
    // By default contentsScale is forced to be 1 except for subclasses of ContentsScalingLayer.
    float contentsScaleX() const { return m_drawProperties.contents_scale_x; }
    float contentsScaleY() const { return m_drawProperties.contents_scale_y; }
    gfx::Size contentBounds() const { return m_drawProperties.content_bounds; }
    virtual void calculateContentsScale(
        float idealContentsScale,
        float* contentsScaleX,
        float* contentsScaleY,
        gfx::Size* contentBounds);

    // The scale at which contents should be rastered, to match the scale at
    // which they will drawn to the screen. This scale is a component of the
    // contentsScale() but does not include page/device scale factors.
    float rasterScale() const { return m_rasterScale; }
    void setRasterScale(float scale);

    // When true, the rasterScale() will be set by the compositor. If false, it
    // will use whatever value is given to it by the embedder.
    bool automaticallyComputeRasterScale() { return m_automaticallyComputeRasterScale; }
    void setAutomaticallyComputeRasterScale(bool);

    void forceAutomaticRasterScaleToBeRecomputed();

    // When true, the layer's contents are not scaled by the current page scale factor.
    // setBoundsContainPageScale recursively sets the value on all child layers.
    void setBoundsContainPageScale(bool);
    bool boundsContainPageScale() const { return m_boundsContainPageScale; }

    LayerTreeHost* layerTreeHost() const { return m_layerTreeHost; }

    // Set the priority of all desired textures in this layer.
    virtual void setTexturePriorities(const PriorityCalculator&) { }

    bool addAnimation(scoped_ptr<Animation>);
    void pauseAnimation(int animationId, double timeOffset);
    void removeAnimation(int animationId);

    void suspendAnimations(double monotonicTime);
    void resumeAnimations(double monotonicTime);

    LayerAnimationController* layerAnimationController() { return m_layerAnimationController.get(); }
    void setLayerAnimationController(scoped_refptr<LayerAnimationController>);
    scoped_refptr<LayerAnimationController> releaseLayerAnimationController();

    void setLayerAnimationDelegate(WebKit::WebAnimationDelegate* layerAnimationDelegate) { m_layerAnimationDelegate = layerAnimationDelegate; }

    bool hasActiveAnimation() const;

    virtual void notifyAnimationStarted(const AnimationEvent&, double wallClockTime);
    virtual void notifyAnimationFinished(double wallClockTime);

    void addLayerAnimationEventObserver(LayerAnimationEventObserver* animationObserver);
    void removeLayerAnimationEventObserver(LayerAnimationEventObserver* animationObserver);

    virtual Region visibleContentOpaqueRegion() const;

    virtual ScrollbarLayer* toScrollbarLayer();

    gfx::Rect layerRectToContentRect(const gfx::RectF& layerRect) const;

    // In impl-side painting, this returns true if this layer type is not
    // compatible with the main thread running freely, such as a double-buffered
    // canvas that doesn't want to be triple-buffered across all three trees.
    virtual bool blocksPendingCommit() const;

    virtual bool canClipSelf() const;

protected:
    friend class LayerImpl;
    friend class TreeSynchronizer;
    virtual ~Layer();

    Layer();

    void setNeedsCommit();
    void setNeedsFullTreeSync();

    // This flag is set when layer need repainting/updating.
    bool m_needsDisplay;

    // Tracks whether this layer may have changed stacking order with its siblings.
    bool m_stackingOrderChanged;

    // The update rect is the region of the compositor resource that was actually updated by the compositor.
    // For layers that may do updating outside the compositor's control (i.e. plugin layers), this information
    // is not available and the update rect will remain empty.
    // Note this rect is in layer space (not content space).
    gfx::RectF m_updateRect;

    scoped_refptr<Layer> m_maskLayer;

    // Constructs a LayerImpl of the correct runtime type for this Layer type.
    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl);
    int m_layerId;

    // When true, the layer is about to perform an update. Any commit requests
    // will be handled implcitly after the update completes.
    bool m_ignoreSetNeedsCommit;

private:
    friend class base::RefCounted<Layer>;

    void setParent(Layer*);
    bool hasAncestor(Layer*) const;
    bool descendantIsFixedToContainerLayer() const;

    size_t numChildren() const { return m_children.size(); }

    // Returns the index of the child or -1 if not found.
    int indexOfChild(const Layer*);

    // This should only be called from removeFromParent.
    void removeChild(Layer*);

    // LayerAnimationValueObserver implementation.
    virtual void OnOpacityAnimated(float) OVERRIDE;
    virtual void OnTransformAnimated(const gfx::Transform&) OVERRIDE;
    virtual bool IsActive() const OVERRIDE;

    LayerList m_children;
    Layer* m_parent;

    // Layer instances have a weak pointer to their LayerTreeHost.
    // This pointer value is nil when a Layer is not in a tree and is
    // updated via setLayerTreeHost() if a layer moves between trees.
    LayerTreeHost* m_layerTreeHost;

    ObserverList<LayerAnimationEventObserver> m_layerAnimationObservers;

    scoped_refptr<LayerAnimationController> m_layerAnimationController;

    // Layer properties.
    gfx::Size m_bounds;

    gfx::Vector2d m_scrollOffset;
    gfx::Vector2d m_maxScrollOffset;
    bool m_scrollable;
    bool m_shouldScrollOnMainThread;
    bool m_haveWheelEventHandlers;
    Region m_nonFastScrollableRegion;
    Region m_touchEventHandlerRegion;
    gfx::PointF m_position;
    gfx::PointF m_anchorPoint;
    SkColor m_backgroundColor;
    std::string m_debugName;
    float m_opacity;
    skia::RefPtr<SkImageFilter> m_filter;
    WebKit::WebFilterOperations m_filters;
    WebKit::WebFilterOperations m_backgroundFilters;
    float m_anchorPointZ;
    bool m_isContainerForFixedPositionLayers;
    bool m_fixedToContainerLayer;
    bool m_isDrawable;
    bool m_masksToBounds;
    bool m_contentsOpaque;
    bool m_doubleSided;
    bool m_preserves3D;
    bool m_useParentBackfaceVisibility;
    bool m_drawCheckerboardForMissingTiles;
    bool m_forceRenderSurface;

    gfx::Transform m_transform;
    gfx::Transform m_sublayerTransform;

    // Replica layer used for reflections.
    scoped_refptr<Layer> m_replicaLayer;

    // Transient properties.
    float m_rasterScale;
    bool m_automaticallyComputeRasterScale;
    bool m_boundsContainPageScale;

    gfx::Transform m_implTransform;

    WebKit::WebAnimationDelegate* m_layerAnimationDelegate;
    WebKit::WebLayerScrollClient* m_layerScrollClient;

    DrawProperties<Layer, RenderSurface> m_drawProperties;
};

void sortLayers(std::vector<scoped_refptr<Layer> >::iterator, std::vector<scoped_refptr<Layer> >::iterator, void*);

}  // namespace cc

#endif  // CC_LAYER_H_
