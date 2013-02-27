// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_HOST_H_
#define CC_LAYER_TREE_HOST_H_

#include <limits>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "cc/animation_events.h"
#include "cc/cc_export.h"
#include "cc/layer_tree_host_client.h"
#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_settings.h"
#include "cc/occlusion_tracker.h"
#include "cc/output_surface.h"
#include "cc/proxy.h"
#include "cc/rate_limiter.h"
#include "cc/rendering_stats.h"
#include "cc/scoped_ptr_vector.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/rect.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<WebKit::WebGraphicsContext3D*> {
  size_t operator()(WebKit::WebGraphicsContext3D* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

namespace cc {

class AnimationRegistrar;
class HeadsUpDisplayLayer;
class Layer;
class LayerTreeHostImpl;
class LayerTreeHostImplClient;
class PrioritizedResourceManager;
class PrioritizedResource;
class Region;
class ResourceProvider;
class ResourceUpdateQueue;
class TopControlsManager;
struct ScrollAndScaleSet;


// Provides information on an Impl's rendering capabilities back to the LayerTreeHost
struct CC_EXPORT RendererCapabilities {
    RendererCapabilities();
    ~RendererCapabilities();

    unsigned bestTextureFormat;
    bool usingPartialSwap;
    bool usingAcceleratedPainting;
    bool usingSetVisibility;
    bool usingSwapCompleteCallback;
    bool usingGpuMemoryManager;
    bool usingDiscardBackbuffer;
    bool usingEglImage;
    bool allowPartialTextureUpdates;
    bool usingOffscreenContext3d;
    int maxTextureSize;
    bool avoidPow2Textures;
};

class CC_EXPORT LayerTreeHost : public RateLimiterClient {
public:
    static scoped_ptr<LayerTreeHost> create(LayerTreeHostClient*, const LayerTreeSettings&, scoped_ptr<Thread> implThread);
    virtual ~LayerTreeHost();

    void setSurfaceReady();

    // Returns true if any LayerTreeHost is alive.
    static bool anyLayerTreeHostInstanceExists();

    void setNeedsFilterContext() { m_needsFilterContext = true; }
    bool needsOffscreenContext() const { return m_needsFilterContext || settings().acceleratePainting; }

    // LayerTreeHost interface to Proxy.
    void willBeginFrame() { m_client->willBeginFrame(); }
    void didBeginFrame();
    void updateAnimations(base::TimeTicks monotonicFrameBeginTime);
    void didStopFlinging();
    void layout();
    void beginCommitOnImplThread(LayerTreeHostImpl*);
    void finishCommitOnImplThread(LayerTreeHostImpl*);
    void willCommit();
    void commitComplete();
    scoped_ptr<OutputSurface> createOutputSurface();
    scoped_ptr<InputHandler> createInputHandler();
    virtual scoped_ptr<LayerTreeHostImpl> createLayerTreeHostImpl(LayerTreeHostImplClient*);
    void didLoseOutputSurface();
    enum RecreateResult {
        RecreateSucceeded,
        RecreateFailedButTryAgain,
        RecreateFailedAndGaveUp,
    };
    RecreateResult recreateOutputSurface();
    void didCommitAndDrawFrame() { m_client->didCommitAndDrawFrame(); }
    void didCompleteSwapBuffers() { m_client->didCompleteSwapBuffers(); }
    void deleteContentsTexturesOnImplThread(ResourceProvider*);
    virtual void acquireLayerTextures();
    // Returns false if we should abort this frame due to initialization failure.
    bool initializeRendererIfNeeded();
    void updateLayers(ResourceUpdateQueue&, size_t contentsMemoryLimitBytes);

    LayerTreeHostClient* client() { return m_client; }

    void composite();

    // Only used when compositing on the main thread.
    void scheduleComposite();

    // Composites and attempts to read back the result into the provided
    // buffer. If it wasn't possible, e.g. due to context lost, will return
    // false.
    bool compositeAndReadback(void *pixels, const gfx::Rect&);

    void finishAllRendering();

    void setDeferCommits(bool deferCommits);

    // Test only hook
    virtual void didDeferCommit();

    int commitNumber() const { return m_commitNumber; }

    void renderingStats(RenderingStats*) const;

    const RendererCapabilities& rendererCapabilities() const;

    void setNeedsAnimate();
    // virtual for testing
    virtual void setNeedsCommit();
    virtual void setNeedsFullTreeSync();
    void setNeedsRedraw();
    bool commitRequested() const;

    void setAnimationEvents(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime);

    Layer* rootLayer() { return m_rootLayer.get(); }
    const Layer* rootLayer() const { return m_rootLayer.get(); }
    void setRootLayer(scoped_refptr<Layer>);

    const LayerTreeSettings& settings() const { return m_settings; }

    void setDebugState(const LayerTreeDebugState& debugState);
    const LayerTreeDebugState& debugState() const { return m_debugState; }

    void setViewportSize(const gfx::Size& layoutViewportSize, const gfx::Size& deviceViewportSize);

    const gfx::Size& layoutViewportSize() const { return m_layoutViewportSize; }
    const gfx::Size& deviceViewportSize() const { return m_deviceViewportSize; }

    void setPageScaleFactorAndLimits(float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor);

    void setBackgroundColor(SkColor color) { m_backgroundColor = color; }

    void setHasTransparentBackground(bool transparent) { m_hasTransparentBackground = transparent; }

    PrioritizedResourceManager* contentsTextureManager() const;

    bool visible() const { return m_visible; }
    void setVisible(bool);

    void startPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration);

    void applyScrollAndScale(const ScrollAndScaleSet&);
    void setImplTransform(const gfx::Transform&);

    void startRateLimiter(WebKit::WebGraphicsContext3D*);
    void stopRateLimiter(WebKit::WebGraphicsContext3D*);

    // RateLimitClient implementation
    virtual void rateLimit() OVERRIDE;

    bool bufferedUpdates();
    bool requestPartialTextureUpdate();

    void setDeviceScaleFactor(float);
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

    void enableHidingTopControls(bool enable);

    HeadsUpDisplayLayer* hudLayer() const { return m_hudLayer.get(); }

    Proxy* proxy() const { return m_proxy.get(); }

    AnimationRegistrar* animationRegistrar() const { return m_animationRegistrar.get(); }

    skia::RefPtr<SkPicture> capturePicture();

    bool blocksPendingCommit() const;

    // Obtains a thorough dump of the LayerTreeHost as a value.
    scoped_ptr<base::Value> asValue() const;

protected:
    LayerTreeHost(LayerTreeHostClient*, const LayerTreeSettings&);
    bool initialize(scoped_ptr<Thread> implThread);
    bool initializeForTesting(scoped_ptr<Proxy> proxyForTesting);

private:
    typedef std::vector<scoped_refptr<Layer> > LayerList;

    bool initializeProxy(scoped_ptr<Proxy> proxy);
    void initializeRenderer();

    void update(Layer*, ResourceUpdateQueue&, const OcclusionTracker*);
    bool paintLayerContents(const LayerList&, ResourceUpdateQueue&);
    bool paintMasksForRenderSurface(Layer*, ResourceUpdateQueue&);

    void updateLayers(Layer*, ResourceUpdateQueue&);
    void updateHudLayer();
    void triggerPrepaint();

    void prioritizeTextures(const LayerList&, OverdrawMetrics&); 
    void setPrioritiesForSurfaces(size_t surfaceMemoryBytes);
    void setPrioritiesForLayers(const LayerList&);
    size_t calculateMemoryForRenderSurfaces(const LayerList& updateList);

    void animateLayers(base::TimeTicks monotonicTime);
    bool animateLayersRecursive(Layer* current, base::TimeTicks time);
    void setAnimationEventsRecursive(const AnimationEventsVector&, Layer*, base::Time wallClockTime);

    bool m_animating;
    bool m_needsFullTreeSync;
    bool m_needsFilterContext;

    base::CancelableClosure m_prepaintCallback;

    LayerTreeHostClient* m_client;
    scoped_ptr<Proxy> m_proxy;

    int m_commitNumber;
    RenderingStats m_renderingStats;

    bool m_rendererInitialized;
    bool m_outputSurfaceLost;
    int m_numFailedRecreateAttempts;

    scoped_refptr<Layer> m_rootLayer;
    scoped_refptr<HeadsUpDisplayLayer> m_hudLayer;

    scoped_ptr<PrioritizedResourceManager> m_contentsTextureManager;
    scoped_ptr<PrioritizedResource> m_surfaceMemoryPlaceholder;

    base::WeakPtr<TopControlsManager> m_topControlsManagerWeakPtr;

    LayerTreeSettings m_settings;
    LayerTreeDebugState m_debugState;

    gfx::Size m_layoutViewportSize;
    gfx::Size m_deviceViewportSize;
    float m_deviceScaleFactor;

    bool m_visible;

    typedef base::hash_map<WebKit::WebGraphicsContext3D*, scoped_refptr<RateLimiter> > RateLimiterMap;
    RateLimiterMap m_rateLimiters;

    float m_pageScaleFactor;
    float m_minPageScaleFactor, m_maxPageScaleFactor;
    gfx::Transform m_implTransform;
    bool m_triggerIdleUpdates;

    SkColor m_backgroundColor;
    bool m_hasTransparentBackground;

    typedef ScopedPtrVector<PrioritizedResource> TextureList;
    size_t m_partialTextureUpdateRequests;

    scoped_ptr<AnimationRegistrar> m_animationRegistrar;

    DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_LAYER_TREE_HOST_H_
