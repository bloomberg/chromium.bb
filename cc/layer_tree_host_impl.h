// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_HOST_IMPL_H_
#define CC_LAYER_TREE_HOST_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/animation_events.h"
#include "cc/animation_registrar.h"
#include "cc/cc_export.h"
#include "cc/input_handler.h"
#include "cc/output_surface_client.h"
#include "cc/render_pass.h"
#include "cc/render_pass_sink.h"
#include "cc/renderer.h"
#include "cc/tile_manager.h"
#include "cc/top_controls_manager_client.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/rect.h"

namespace cc {

class CompletionEvent;
class CompositorFrameMetadata;
class DebugRectHistory;
class FrameRateCounter;
class LayerImpl;
class LayerTreeHostImplTimeSourceAdapter;
class LayerTreeImpl;
class PageScaleAnimation;
class PaintTimeCounter;
class MemoryHistory;
class RenderPassDrawQuad;
class ResourceProvider;
class TopControlsManager;
struct RendererCapabilities;
struct RenderingStats;

// LayerTreeHost->Proxy callback interface.
class LayerTreeHostImplClient {
public:
    virtual void didLoseOutputSurfaceOnImplThread() = 0;
    virtual void onSwapBuffersCompleteOnImplThread() = 0;
    virtual void onVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval) = 0;
    virtual void onCanDrawStateChanged(bool canDraw) = 0;
    virtual void onHasPendingTreeStateChanged(bool hasPendingTree) = 0;
    virtual void setNeedsRedrawOnImplThread() = 0;
    virtual void didUploadVisibleHighResolutionTileOnImplThread() = 0;
    virtual void setNeedsCommitOnImplThread() = 0;
    virtual void setNeedsManageTilesOnImplThread() = 0;
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime) = 0;
    // Returns true if resources were deleted by this call.
    virtual bool reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) = 0;
    virtual void reduceWastedContentsTextureMemoryOnImplThread() = 0;
    virtual void sendManagedMemoryStats() = 0;
    virtual bool isInsideDraw() = 0;
    virtual void renewTreePriority() = 0;
};

// LayerTreeHostImpl owns the LayerImpl tree as well as associated rendering state
class CC_EXPORT LayerTreeHostImpl : public InputHandlerClient,
                                    public RendererClient,
                                    public TileManagerClient,
                                    public OutputSurfaceClient,
                                    public TopControlsManagerClient {
    typedef std::vector<LayerImpl*> LayerList;

public:
    static scoped_ptr<LayerTreeHostImpl> create(const LayerTreeSettings&, LayerTreeHostImplClient*, Proxy*);
    virtual ~LayerTreeHostImpl();

    // InputHandlerClient implementation
    virtual InputHandlerClient::ScrollStatus scrollBegin(gfx::Point, InputHandlerClient::ScrollInputType) OVERRIDE;
    virtual bool scrollBy(const gfx::Point&, const gfx::Vector2d&) OVERRIDE;
    virtual void scrollEnd() OVERRIDE;
    virtual void pinchGestureBegin() OVERRIDE;
    virtual void pinchGestureUpdate(float, gfx::Point) OVERRIDE;
    virtual void pinchGestureEnd() OVERRIDE;
    virtual void startPageScaleAnimation(gfx::Vector2d targetOffset, bool anchorPoint, float pageScale, base::TimeTicks startTime, base::TimeDelta duration) OVERRIDE;
    virtual void scheduleAnimation() OVERRIDE;
    virtual bool haveTouchEventHandlersAt(const gfx::Point&) OVERRIDE;

    // TopControlsManagerClient implementation.
    virtual void setActiveTreeNeedsUpdateDrawProperties() OVERRIDE;
    virtual void setNeedsRedraw() OVERRIDE;
    virtual bool haveRootScrollLayer() const OVERRIDE;
    virtual float rootScrollLayerTotalScrollY() const OVERRIDE;

    struct CC_EXPORT FrameData : public RenderPassSink {
        FrameData();
        ~FrameData();

        std::vector<gfx::Rect> occludingScreenSpaceRects;
        std::vector<gfx::Rect> nonOccludingScreenSpaceRects;
        RenderPassList renderPasses;
        RenderPassIdHashMap renderPassesById;
        const LayerList* renderSurfaceLayerList;
        LayerList willDrawLayers;
        bool containsIncompleteTile;

        // RenderPassSink implementation.
        virtual void appendRenderPass(scoped_ptr<RenderPass>) OVERRIDE;
    };

    // Virtual for testing.
    virtual void beginCommit();
    virtual void commitComplete();
    virtual void animate(base::TimeTicks monotonicTime, base::Time wallClockTime);

    void manageTiles();

    // Returns false if problems occured preparing the frame, and we should try
    // to avoid displaying the frame. If prepareToDraw is called,
    // didDrawAllLayers must also be called, regardless of whether drawLayers is
    // called between the two.
    virtual bool prepareToDraw(FrameData&);
    virtual void drawLayers(FrameData&);
    // Must be called if and only if prepareToDraw was called.
    void didDrawAllLayers(const FrameData&);

    // RendererClient implementation
    virtual const gfx::Size& deviceViewportSize() const OVERRIDE;
    virtual const LayerTreeSettings& settings() const OVERRIDE;
    virtual void didLoseOutputSurface() OVERRIDE;
    virtual void onSwapBuffersComplete() OVERRIDE;
    virtual void setFullRootLayerDamage() OVERRIDE;
    virtual void setManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE;
    virtual void enforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE;
    virtual bool hasImplThread() const OVERRIDE;
    virtual bool shouldClearRootRenderPass() const OVERRIDE;
    virtual CompositorFrameMetadata makeCompositorFrameMetadata() const OVERRIDE;

    // TileManagerClient implementation.
    virtual void ScheduleManageTiles() OVERRIDE;
    virtual void DidUploadVisibleHighResolutionTile() OVERRIDE;

    // OutputSurfaceClient implementation.
    virtual void OnVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE;
    virtual void OnSendFrameToParentCompositorAck(const CompositorFrameAck&) OVERRIDE;

    // Called from LayerTreeImpl.
    void OnCanDrawStateChangedForTree(LayerTreeImpl*);

    // Implementation
    bool canDraw();
    OutputSurface* outputSurface() const;

    std::string layerTreeAsText() const;
    std::string layerTreeAsJson() const;

    void finishAllRendering();
    int sourceAnimationFrameNumber() const;

    virtual bool initializeRenderer(scoped_ptr<OutputSurface>);
    bool isContextLost();
    TileManager* tileManager() { return m_tileManager.get(); }
    Renderer* renderer() { return m_renderer.get(); }
    const RendererCapabilities& rendererCapabilities() const;

    bool swapBuffers();

    void readback(void* pixels, const gfx::Rect&);

    LayerTreeImpl* activeTree() { return m_activeTree.get(); }
    const LayerTreeImpl* activeTree() const { return m_activeTree.get(); }
    LayerTreeImpl* pendingTree() { return m_pendingTree.get(); }
    const LayerTreeImpl* pendingTree() const { return m_pendingTree.get(); }
    const LayerTreeImpl* recycleTree() const { return m_recycleTree.get(); }
    void createPendingTree();
    void checkForCompletedTileUploads();
    virtual bool activatePendingTreeIfNeeded();

    // Shortcuts to layers on the active tree.
    LayerImpl* rootLayer() const;
    LayerImpl* rootScrollLayer() const;
    LayerImpl* currentlyScrollingLayer() const;

    bool visible() const { return m_visible; }
    void setVisible(bool);

    size_t memoryAllocationLimitBytes() const { return m_managedMemoryPolicy.bytesLimitWhenVisible; }

    void setViewportSize(const gfx::Size& layoutViewportSize, const gfx::Size& deviceViewportSize);
    const gfx::Size& layoutViewportSize() const { return m_layoutViewportSize; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float);

    scoped_ptr<ScrollAndScaleSet> processScrollDeltas();

    void startPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration);

    bool needsAnimateLayers() const { return !m_animationRegistrar->active_animation_controllers().empty(); }

    void renderingStats(RenderingStats*) const;

    void sendManagedMemoryStats(
       size_t memoryVisibleBytes,
       size_t memoryVisibleAndNearbyBytes,
       size_t memoryUseBytes);

    FrameRateCounter* fpsCounter() const { return m_fpsCounter.get(); }
    PaintTimeCounter* paintTimeCounter() const { return m_paintTimeCounter.get(); }
    MemoryHistory* memoryHistory() const { return m_memoryHistory.get(); }
    DebugRectHistory* debugRectHistory() const { return m_debugRectHistory.get(); }
    ResourceProvider* resourceProvider() const { return m_resourceProvider.get(); }
    TopControlsManager* topControlsManager() const { return m_topControlsManager.get(); }

    Proxy* proxy() const { return m_proxy; }

    AnimationRegistrar* animationRegistrar() const { return m_animationRegistrar.get(); }

    void setDebugState(const LayerTreeDebugState& debugState);
    const LayerTreeDebugState& debugState() const { return m_debugState; }

    void savePaintTime(const base::TimeDelta& totalPaintTime, int commitNumber);

    class CC_EXPORT CullRenderPassesWithCachedTextures {
    public:
        bool shouldRemoveRenderPass(const RenderPassDrawQuad&, const FrameData&) const;

        // Iterates from the root first, in order to remove the surfaces closest
        // to the root with cached textures, and all surfaces that draw into
        // them.
        size_t renderPassListBegin(const RenderPassList& list) const { return list.size() - 1; }
        size_t renderPassListEnd(const RenderPassList&) const { return 0 - 1; }
        size_t renderPassListNext(size_t it) const { return it - 1; }

        CullRenderPassesWithCachedTextures(Renderer& renderer) : m_renderer(renderer) { }
    private:
        Renderer& m_renderer;
    };

    class CC_EXPORT CullRenderPassesWithNoQuads {
    public:
        bool shouldRemoveRenderPass(const RenderPassDrawQuad&, const FrameData&) const;

        // Iterates in draw order, so that when a surface is removed, and its
        // target becomes empty, then its target can be removed also.
        size_t renderPassListBegin(const RenderPassList&) const { return 0; }
        size_t renderPassListEnd(const RenderPassList& list) const { return list.size(); }
        size_t renderPassListNext(size_t it) const { return it + 1; }
    };

    template<typename RenderPassCuller>
    static void removeRenderPasses(RenderPassCuller, FrameData&);

    skia::RefPtr<SkPicture> capturePicture();

    bool pinchGestureActive() const { return m_pinchGestureActive; }

    void setTreePriority(TreePriority priority);

    void beginNextFrame();
    base::TimeTicks currentFrameTime();

    scoped_ptr<base::Value> asValue() const;
    scoped_ptr<base::Value> activationStateAsValue() const;
    scoped_ptr<base::Value> frameStateAsValue() const;

protected:
    LayerTreeHostImpl(const LayerTreeSettings&, LayerTreeHostImplClient*, Proxy*);
    void activatePendingTree();

    // Virtual for testing.
    virtual void animateLayers(base::TimeTicks monotonicTime, base::Time wallClockTime);
    virtual void updateAnimationState();

    // Virtual for testing.
    virtual base::TimeDelta lowFrequencyAnimationInterval() const;

    const AnimationRegistrar::AnimationControllerMap& activeAnimationControllers() const { return m_animationRegistrar->active_animation_controllers(); }

    LayerTreeHostImplClient* m_client;
    Proxy* m_proxy;

private:
    void animatePageScale(base::TimeTicks monotonicTime);
    void animateScrollbars(base::TimeTicks monotonicTime);

    void setPageScaleDelta(float);
    gfx::Vector2dF scrollLayerWithViewportSpaceDelta(LayerImpl* layerImpl, float scaleFromViewportToScreenSpace, gfx::PointF viewportPoint, gfx::Vector2dF viewportDelta);

    void updateMaxScrollOffset();
    void trackDamageForAllSurfaces(LayerImpl* rootDrawLayer, const LayerList& renderSurfaceLayerList);

    // Returns false if the frame should not be displayed. This function should
    // only be called from prepareToDraw, as didDrawAllLayers must be called
    // if this helper function is called.
    bool calculateRenderPasses(FrameData&);
    void animateLayersRecursive(LayerImpl*, base::TimeTicks monotonicTime, base::Time wallClockTime, AnimationEventsVector*, bool& didAnimate, bool& needsAnimateLayers);
    void setBackgroundTickingEnabled(bool);

    void sendDidLoseOutputSurfaceRecursive(LayerImpl*);
    void clearRenderSurfaces();
    bool ensureRenderSurfaceLayerList();
    void clearCurrentlyScrollingLayer();

    void animateScrollbarsRecursive(LayerImpl*, base::TimeTicks monotonicTime);

    void dumpRenderSurfaces(std::string*, int indent, const LayerImpl*) const;

    static LayerImpl* getNonCompositedContentLayerRecursive(LayerImpl* layer);

    scoped_ptr<OutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> m_resourceProvider;
    scoped_ptr<Renderer> m_renderer;
    scoped_ptr<TileManager> m_tileManager;

    // Tree currently being drawn.
    scoped_ptr<LayerTreeImpl> m_activeTree;

    // In impl-side painting mode, tree with possibly incomplete rasterized
    // content. May be promoted to active by activatePendingTreeIfNeeded().
    scoped_ptr<LayerTreeImpl> m_pendingTree;

    // In impl-side painting mode, inert tree with layers that can be recycled
    // by the next sync from the main thread.
    scoped_ptr<LayerTreeImpl> m_recycleTree;

    bool m_didLockScrollingLayer;
    bool m_shouldBubbleScrolls;
    bool m_wheelScrolling;
    LayerTreeSettings m_settings;
    LayerTreeDebugState m_debugState;
    gfx::Size m_layoutViewportSize;
    gfx::Size m_deviceViewportSize;
    float m_deviceScaleFactor;
    bool m_visible;
    ManagedMemoryPolicy m_managedMemoryPolicy;

    bool m_pinchGestureActive;
    gfx::Point m_previousPinchAnchor;

    // This is set by animateLayers() and used by updateAnimationState()
    // when sending animation events to the main thread.
    base::Time m_lastAnimationTime;

    scoped_ptr<TopControlsManager> m_topControlsManager;

    scoped_ptr<PageScaleAnimation> m_pageScaleAnimation;

    // This is used for ticking animations slowly when hidden.
    scoped_ptr<LayerTreeHostImplTimeSourceAdapter> m_timeSourceClientAdapter;

    scoped_ptr<FrameRateCounter> m_fpsCounter;
    scoped_ptr<PaintTimeCounter> m_paintTimeCounter;
    scoped_ptr<MemoryHistory> m_memoryHistory;
    scoped_ptr<DebugRectHistory> m_debugRectHistory;

    int64 m_numImplThreadScrolls;
    int64 m_numMainThreadScrolls;

    int64 m_cumulativeNumLayersDrawn;

    int64 m_cumulativeNumMissingTiles;

    size_t m_lastSentMemoryVisibleBytes;
    size_t m_lastSentMemoryVisibleAndNearbyBytes;
    size_t m_lastSentMemoryUseBytes;

    base::TimeTicks m_currentFrameTime;

    scoped_ptr<AnimationRegistrar> m_animationRegistrar;

    DISALLOW_COPY_AND_ASSIGN(LayerTreeHostImpl);
};

}  // namespace cc

#endif  // CC_LAYER_TREE_HOST_IMPL_H_
