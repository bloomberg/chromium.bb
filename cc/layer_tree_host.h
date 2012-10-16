// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerTreeHost_h
#define CCLayerTreeHost_h

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "CCAnimationEvents.h"
#include "CCGraphicsContext.h"
#include "CCLayerTreeHostClient.h"
#include "CCLayerTreeHostCommon.h"
#include "CCOcclusionTracker.h"
#include "CCPrioritizedTextureManager.h"
#include "CCProxy.h"
#include "CCRenderingStats.h"
#include "IntRect.h"
#include "RateLimiter.h"
#include "scoped_ptr_vector.h"
#include "third_party/skia/include/core/SkColor.h"
#include <limits>

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

class CCFontAtlas;
class CCLayerChromium;
class CCLayerTreeHostImpl;
class CCLayerTreeHostImplClient;
class CCPrioritizedTextureManager;
class CCTextureUpdateQueue;
class HeadsUpDisplayLayerChromium;
class Region;
struct CCScrollAndScaleSet;

struct CCLayerTreeSettings {
    CCLayerTreeSettings();
    ~CCLayerTreeSettings();

    bool acceleratePainting;
    bool showFPSCounter;
    bool showPlatformLayerTree;
    bool showPaintRects;
    bool showPropertyChangedRects;
    bool showSurfaceDamageRects;
    bool showScreenSpaceRects;
    bool showReplicaScreenSpaceRects;
    bool showOccludingRects;
    bool renderVSyncEnabled;
    double refreshRate;
    size_t maxPartialTextureUpdates;
    IntSize defaultTileSize;
    IntSize maxUntiledLayerSize;
    IntSize minimumOcclusionTrackingSize;

    bool showDebugInfo() const { return showPlatformLayerTree || showFPSCounter || showDebugRects(); }
    bool showDebugRects() const { return showPaintRects || showPropertyChangedRects || showSurfaceDamageRects || showScreenSpaceRects || showReplicaScreenSpaceRects || showOccludingRects; }
};

// Provides information on an Impl's rendering capabilities back to the CCLayerTreeHost
struct RendererCapabilities {
    RendererCapabilities();
    ~RendererCapabilities();

    GC3Denum bestTextureFormat;
    bool contextHasCachedFrontBuffer;
    bool usingPartialSwap;
    bool usingAcceleratedPainting;
    bool usingSetVisibility;
    bool usingSwapCompleteCallback;
    bool usingGpuMemoryManager;
    bool usingDiscardFramebuffer;
    bool usingEglImage;
    int maxTextureSize;
};

class CCLayerTreeHost : public RateLimiterClient {
public:
    static scoped_ptr<CCLayerTreeHost> create(CCLayerTreeHostClient*, const CCLayerTreeSettings&);
    virtual ~CCLayerTreeHost();

    void setSurfaceReady();

    // Returns true if any CCLayerTreeHost is alive.
    static bool anyLayerTreeHostInstanceExists();

    static bool needsFilterContext() { return s_needsFilterContext; }
    static void setNeedsFilterContext(bool needsFilterContext) { s_needsFilterContext = needsFilterContext; }
    bool needsSharedContext() const { return needsFilterContext() || settings().acceleratePainting; }

    // CCLayerTreeHost interface to CCProxy.
    void willBeginFrame() { m_client->willBeginFrame(); }
    void didBeginFrame() { m_client->didBeginFrame(); }
    void updateAnimations(double monotonicFrameBeginTime);
    void layout();
    void beginCommitOnImplThread(CCLayerTreeHostImpl*);
    void finishCommitOnImplThread(CCLayerTreeHostImpl*);
    void willCommit();
    void commitComplete();
    scoped_ptr<CCGraphicsContext> createContext();
    scoped_ptr<CCInputHandler> createInputHandler();
    virtual scoped_ptr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient*);
    void didLoseContext();
    enum RecreateResult {
        RecreateSucceeded,
        RecreateFailedButTryAgain,
        RecreateFailedAndGaveUp,
    };
    RecreateResult recreateContext();
    void didCommitAndDrawFrame() { m_client->didCommitAndDrawFrame(); }
    void didCompleteSwapBuffers() { m_client->didCompleteSwapBuffers(); }
    void deleteContentsTexturesOnImplThread(CCResourceProvider*);
    virtual void acquireLayerTextures();
    // Returns false if we should abort this frame due to initialization failure.
    bool initializeRendererIfNeeded();
    void updateLayers(CCTextureUpdateQueue&, size_t contentsMemoryLimitBytes);

    CCLayerTreeHostClient* client() { return m_client; }

    // Only used when compositing on the main thread.
    void composite();
    void scheduleComposite();

    // Composites and attempts to read back the result into the provided
    // buffer. If it wasn't possible, e.g. due to context lost, will return
    // false.
    bool compositeAndReadback(void *pixels, const IntRect&);

    void finishAllRendering();

    int commitNumber() const { return m_commitNumber; }

    void renderingStats(CCRenderingStats*) const;

    const RendererCapabilities& rendererCapabilities() const;

    // Test only hook
    void loseContext(int numTimes);

    void setNeedsAnimate();
    // virtual for testing
    virtual void setNeedsCommit();
    void setNeedsRedraw();
    bool commitRequested() const;

    void setAnimationEvents(scoped_ptr<CCAnimationEventsVector>, double wallClockTime);
    virtual void didAddAnimation();

    LayerChromium* rootLayer() { return m_rootLayer.get(); }
    const LayerChromium* rootLayer() const { return m_rootLayer.get(); }
    void setRootLayer(scoped_refptr<LayerChromium>);

    const CCLayerTreeSettings& settings() const { return m_settings; }

    void setViewportSize(const IntSize& layoutViewportSize, const IntSize& deviceViewportSize);

    const IntSize& layoutViewportSize() const { return m_layoutViewportSize; }
    const IntSize& deviceViewportSize() const { return m_deviceViewportSize; }

    void setPageScaleFactorAndLimits(float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor);

    void setBackgroundColor(SkColor color) { m_backgroundColor = color; }

    void setHasTransparentBackground(bool transparent) { m_hasTransparentBackground = transparent; }

    CCPrioritizedTextureManager* contentsTextureManager() const;

    bool visible() const { return m_visible; }
    void setVisible(bool);

    void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double durationSec);

    void applyScrollAndScale(const CCScrollAndScaleSet&);
    void setImplTransform(const WebKit::WebTransformationMatrix&);

    void startRateLimiter(WebKit::WebGraphicsContext3D*);
    void stopRateLimiter(WebKit::WebGraphicsContext3D*);

    // RateLimitClient implementation
    virtual void rateLimit() OVERRIDE;

    bool bufferedUpdates();
    bool requestPartialTextureUpdate();
    void deleteTextureAfterCommit(scoped_ptr<CCPrioritizedTexture>);

    void setDeviceScaleFactor(float);
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

    void setFontAtlas(scoped_ptr<CCFontAtlas>);

    HeadsUpDisplayLayerChromium* hudLayer() const { return m_hudLayer.get(); }

protected:
    CCLayerTreeHost(CCLayerTreeHostClient*, const CCLayerTreeSettings&);
    bool initialize();

private:
    typedef std::vector<scoped_refptr<LayerChromium> > LayerList;

    void initializeRenderer();

    void update(LayerChromium*, CCTextureUpdateQueue&, const CCOcclusionTracker*);
    bool paintLayerContents(const LayerList&, CCTextureUpdateQueue&);
    bool paintMasksForRenderSurface(LayerChromium*, CCTextureUpdateQueue&);

    void updateLayers(LayerChromium*, CCTextureUpdateQueue&);

    void prioritizeTextures(const LayerList&, CCOverdrawMetrics&); 
    void setPrioritiesForSurfaces(size_t surfaceMemoryBytes);
    void setPrioritiesForLayers(const LayerList&);
    size_t calculateMemoryForRenderSurfaces(const LayerList& updateList);

    void animateLayers(double monotonicTime);
    bool animateLayersRecursive(LayerChromium* current, double monotonicTime);
    void setAnimationEventsRecursive(const CCAnimationEventsVector&, LayerChromium*, double wallClockTime);

    bool m_animating;
    bool m_needsAnimateLayers;

    CCLayerTreeHostClient* m_client;

    int m_commitNumber;
    CCRenderingStats m_renderingStats;

    scoped_ptr<CCProxy> m_proxy;
    bool m_rendererInitialized;
    bool m_contextLost;
    int m_numTimesRecreateShouldFail;
    int m_numFailedRecreateAttempts;

    scoped_refptr<LayerChromium> m_rootLayer;
    scoped_refptr<HeadsUpDisplayLayerChromium> m_hudLayer;
    scoped_ptr<CCFontAtlas> m_fontAtlas;

    scoped_ptr<CCPrioritizedTextureManager> m_contentsTextureManager;
    scoped_ptr<CCPrioritizedTexture> m_surfaceMemoryPlaceholder;

    CCLayerTreeSettings m_settings;

    IntSize m_layoutViewportSize;
    IntSize m_deviceViewportSize;
    float m_deviceScaleFactor;

    bool m_visible;

    typedef base::hash_map<WebKit::WebGraphicsContext3D*, scoped_refptr<RateLimiter> > RateLimiterMap;
    RateLimiterMap m_rateLimiters;

    float m_pageScaleFactor;
    float m_minPageScaleFactor, m_maxPageScaleFactor;
    WebKit::WebTransformationMatrix m_implTransform;
    bool m_triggerIdleUpdates;

    SkColor m_backgroundColor;
    bool m_hasTransparentBackground;

    typedef ScopedPtrVector<CCPrioritizedTexture> TextureList;
    TextureList m_deleteTextureAfterCommitList;
    size_t m_partialTextureUpdateRequests;

    static bool s_needsFilterContext;

    DISALLOW_COPY_AND_ASSIGN(CCLayerTreeHost);
};

}  // namespace cc

#endif
