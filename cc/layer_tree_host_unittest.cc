// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "base/synchronization/lock.h"
#include "cc/content_layer.h"
#include "cc/content_layer_client.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/output_surface.h"
#include "cc/picture_layer.h"
#include "cc/prioritized_resource.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_scrollbar_layer.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/resource_update_queue.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/timing_function.h"
#include "skia/ext/refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {
namespace {

class LayerTreeHostTest : public ThreadedTest { };

// Shortlived layerTreeHosts shouldn't die.
class LayerTreeHostTestShortlived1 : public LayerTreeHostTest {
public:
    LayerTreeHostTestShortlived1() { }

    virtual void beginTest() OVERRIDE
    {
        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

// Shortlived layerTreeHosts shouldn't die with a commit in flight.
class LayerTreeHostTestShortlived2 : public LayerTreeHostTest {
public:
    LayerTreeHostTestShortlived2() { }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestShortlived2)

// Shortlived layerTreeHosts shouldn't die with a redraw in flight.
class LayerTreeHostTestShortlived3 : public LayerTreeHostTest {
public:
    LayerTreeHostTestShortlived3() { }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsRedrawToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestShortlived3)

// Test interleaving of redraws and commits
class LayerTreeHostTestCommitingWithContinuousRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestCommitingWithContinuousRedraw()
        : m_numCompleteCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCompleteCommits++;
        if (m_numCompleteCommits == 2)
            endTest();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDraws == 1)
          postSetNeedsCommitToMainThread();
        m_numDraws++;
        postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCompleteCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestCommitingWithContinuousRedraw, runMultiThread)
{
    runTest(true);
}

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class LayerTreeHostTestSetNeedsCommit1 : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsCommit1()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        if (!impl->activeTree()->source_frame_number())
            endTest();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_GE(1, m_numCommits);
        EXPECT_GE(1, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetNeedsCommit1, DISABLED_runMultiThread)
{
    runTest(true);
}

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class LayerTreeHostTestSetNeedsCommit2 : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsCommit2()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (impl->activeTree()->source_frame_number() == 0)
            postSetNeedsCommitToMainThread();
        else if (impl->activeTree()->source_frame_number() == 1)
            endTest();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_numCommits);
        EXPECT_GE(2, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetNeedsCommit2, runMultiThread)
{
    runTest(true);
}

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class LayerTreeHostTestSetNeedsRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(0, impl->activeTree()->source_frame_number());
        if (!m_numDraws)
            postSetNeedsRedrawToMainThread(); // Redraw again to verify that the second redraw doesn't commit.
        else
            endTest();
        m_numDraws++;
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        EXPECT_EQ(0, m_numDraws);
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_GE(2, m_numDraws);
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetNeedsRedraw, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestNoExtraCommitFromInvalidate : public LayerTreeHostTest {
public:
    LayerTreeHostTestNoExtraCommitFromInvalidate()
        : m_rootLayer(ContentLayer::create(&m_client))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_rootLayer->setAutomaticallyComputeRasterScale(false);
        m_rootLayer->setIsDrawable(true);
        m_rootLayer->setBounds(gfx::Size(1, 1));
        m_layerTreeHost->setRootLayer(m_rootLayer);
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        switch (m_layerTreeHost->commitNumber()) {
        case 1:
            // Changing the content bounds will cause a single commit!
            m_rootLayer->setRasterScale(4.0f);
            break;
        default:
            // No extra commits.
            EXPECT_EQ(2, m_layerTreeHost->commitNumber());
            endTest();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<ContentLayer> m_rootLayer;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestNoExtraCommitFromInvalidate)

class LayerTreeHostTestCompositeAndReadback : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadback()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 2) {
            // This is inside the readback. We should get another commit after it.
        } else if (m_numCommits == 3) {
            endTest();
        } else {
          NOTREACHED();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestCompositeAndReadback, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            m_layerTreeHost->setNeedsCommit();
        } else if (m_numCommits == 2) {
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 3) {
            // This is inside the readback. We should get another commit after it.
        } else if (m_numCommits == 4) {
            endTest();
        } else {
          NOTREACHED();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws, runMultiThread)
{
    runTest(true);
}

// If the layerTreeHost says it can't draw, then we should not try to draw.
class LayerTreeHostTestCanDrawBlocksDrawing : public LayerTreeHostTest {
public:
    LayerTreeHostTestCanDrawBlocksDrawing()
        : m_numCommits(0)
        , m_done(false)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (m_done)
            return;
        // Only the initial draw should bring us here.
        EXPECT_TRUE(impl->canDraw());
        EXPECT_EQ(0, impl->activeTree()->source_frame_number());
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (m_done)
            return;
        if (m_numCommits >= 1) {
            // After the first commit, we should not be able to draw.
            EXPECT_FALSE(impl->canDraw());
        }
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            // Make the viewport empty so the host says it can't draw.
            m_layerTreeHost->setViewportSize(gfx::Size(0, 0), gfx::Size(0, 0));
        } else if (m_numCommits == 2) {
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 3) {
            // Let it draw so we go idle and end the test.
            m_layerTreeHost->setViewportSize(gfx::Size(1, 1), gfx::Size(1, 1));
            m_done = true;
            endTest();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
    bool m_done;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCanDrawBlocksDrawing)

// beginLayerWrite should prevent draws from executing until a commit occurs
class LayerTreeHostTestWriteLayersRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestWriteLayersRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAcquireLayerTextures();
        postSetNeedsRedrawToMainThread(); // should be inhibited without blocking
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        EXPECT_EQ(m_numDraws, m_numCommits);
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestWriteLayersRedraw, runMultiThread)
{
    runTest(true);
}

// Verify that when resuming visibility, requesting layer write permission
// will not deadlock the main thread even though there are not yet any
// scheduled redraws. This behavior is critical for reliably surviving tab
// switching. There are no failure conditions to this test, it just passes
// by not timing out.
class LayerTreeHostTestWriteLayersAfterVisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestWriteLayersAfterVisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 2)
            endTest();
        else if (m_numCommits < 2) {
            postSetVisibleToMainThread(false);
            postSetVisibleToMainThread(true);
            postAcquireLayerTextures();
            postSetNeedsCommitToMainThread();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestWriteLayersAfterVisible, runMultiThread)
{
    runTest(true);
}

// A compositeAndReadback while invisible should force a normal commit without assertion.
class LayerTreeHostTestCompositeAndReadbackWhileInvisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackWhileInvisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            m_layerTreeHost->setVisible(false);
            m_layerTreeHost->setNeedsCommit();
            m_layerTreeHost->setNeedsCommit();
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        } else
            endTest();

    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestCompositeAndReadbackWhileInvisible, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestAbortFrameWhenInvisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestAbortFrameWhenInvisible()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        // Request a commit (from the main thread), which will trigger the commit flow from the impl side.
        m_layerTreeHost->setNeedsCommit();
        // Then mark ourselves as not visible before processing any more messages on the main thread.
        m_layerTreeHost->setVisible(false);
        // If we make it without kicking a frame, we pass!
        endTestAfterDelay(1);
    }

    virtual void layout() OVERRIDE
    {
        ASSERT_FALSE(true);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
};

TEST_F(LayerTreeHostTestAbortFrameWhenInvisible, runMultiThread)
{
    runTest(true);
}

// This test verifies that properties on the layer tree host are commited to the impl side.
class LayerTreeHostTestCommit : public LayerTreeHostTest {
public:

    LayerTreeHostTestCommit() { }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(20, 20), gfx::Size(20, 20));
        m_layerTreeHost->setBackgroundColor(SK_ColorGRAY);
        m_layerTreeHost->setPageScaleFactorAndLimits(5, 5, 5);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(gfx::Size(20, 20), impl->layoutViewportSize());
        EXPECT_EQ(SK_ColorGRAY, impl->activeTree()->background_color());
        EXPECT_EQ(5, impl->pageScaleFactor());

        endTest();
    }

    virtual void afterTest() OVERRIDE { }
};

TEST_F(LayerTreeHostTestCommit, runTest)
{
    runTest(true);
}

// Verifies that startPageScaleAnimation events propagate correctly from LayerTreeHost to
// LayerTreeHostImpl in the MT compositor.
class LayerTreeHostTestStartPageScaleAnimation : public LayerTreeHostTest {
public:

    LayerTreeHostTestStartPageScaleAnimation()
        : m_animationRequested(false)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollOffset(gfx::Vector2d());
        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    void requestStartPageScaleAnimation()
    {
        layerTreeHost()->startPageScaleAnimation(gfx::Vector2d(), false, 1.25, base::TimeDelta());
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        impl->rootLayer()->setScrollable(true);
        impl->rootLayer()->setScrollOffset(gfx::Vector2d());
        impl->setPageScaleFactorAndLimits(impl->pageScaleFactor(), 0.5, 2);

        // We request animation only once.
        if (!m_animationRequested) {
            impl->proxy()->mainThread()->postTask(base::Bind(&LayerTreeHostTestStartPageScaleAnimation::requestStartPageScaleAnimation, base::Unretained(this)));
            m_animationRequested = true;
        }
    }

    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
        m_layerTreeHost->rootLayer()->setScrollOffset(offset + scrollDelta);
        m_layerTreeHost->setPageScaleFactorAndLimits(scale, 0.5, 2);
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        impl->processScrollDeltas();
        // We get one commit before the first draw, and the animation doesn't happen until the second draw.
        if (impl->activeTree()->source_frame_number() == 1) {
            EXPECT_EQ(1.25, impl->pageScaleFactor());
            endTest();
        } else
            postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    bool m_animationRequested;
};

TEST_F(LayerTreeHostTestStartPageScaleAnimation, runTest)
{
    runTest(true);
}

class LayerTreeHostTestSetVisible : public LayerTreeHostTest {
public:

    LayerTreeHostTestSetVisible()
        : m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
        postSetVisibleToMainThread(false);
        postSetNeedsRedrawToMainThread(); // This is suppressed while we're invisible.
        postSetVisibleToMainThread(true); // Triggers the redraw.
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_TRUE(impl->visible());
        ++m_numDraws;
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numDraws);
    }

private:
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetVisible, runMultiThread)
{
    runTest(true);
}

class TestOpacityChangeLayerDelegate : public ContentLayerClient {
public:
    TestOpacityChangeLayerDelegate()
        : m_testLayer(0)
    {
    }

    void setTestLayer(Layer* testLayer)
    {
        m_testLayer = testLayer;
    }

    virtual void paintContents(SkCanvas*, const gfx::Rect&, gfx::RectF&) OVERRIDE
    {
        // Set layer opacity to 0.
        if (m_testLayer)
            m_testLayer->setOpacity(0);
    }

private:
    Layer* m_testLayer;
};

class ContentLayerWithUpdateTracking : public ContentLayer {
public:
    static scoped_refptr<ContentLayerWithUpdateTracking> create(ContentLayerClient* client) { return make_scoped_refptr(new ContentLayerWithUpdateTracking(client)); }

    int paintContentsCount() { return m_paintContentsCount; }
    void resetPaintContentsCount() { m_paintContentsCount = 0; }

    virtual void update(ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats) OVERRIDE
    {
        ContentLayer::update(queue, occlusion, stats);
        m_paintContentsCount++;
    }

private:
    explicit ContentLayerWithUpdateTracking(ContentLayerClient* client)
        : ContentLayer(client)
        , m_paintContentsCount(0)
    {
        setAnchorPoint(gfx::PointF(0, 0));
        setBounds(gfx::Size(10, 10));
        setIsDrawable(true);
    }
    virtual ~ContentLayerWithUpdateTracking()
    {
    }

    int m_paintContentsCount;
};

// Layer opacity change during paint should not prevent compositor resources from being updated during commit.
class LayerTreeHostTestOpacityChange : public LayerTreeHostTest {
public:
    LayerTreeHostTestOpacityChange()
        : m_testOpacityChangeDelegate()
        , m_updateCheckLayer(ContentLayerWithUpdateTracking::create(&m_testOpacityChangeDelegate))
    {
        m_testOpacityChangeDelegate.setTestLayer(m_updateCheckLayer.get());
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        m_layerTreeHost->rootLayer()->addChild(m_updateCheckLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // update() should have been called once.
        EXPECT_EQ(1, m_updateCheckLayer->paintContentsCount());

        // clear m_updateCheckLayer so LayerTreeHost dies.
        m_updateCheckLayer = NULL;
    }

private:
    TestOpacityChangeLayerDelegate m_testOpacityChangeDelegate;
    scoped_refptr<ContentLayerWithUpdateTracking> m_updateCheckLayer;
};

TEST_F(LayerTreeHostTestOpacityChange, runMultiThread)
{
    runTest(true);
}

class NoScaleContentLayer : public ContentLayer {
public:
    static scoped_refptr<NoScaleContentLayer> create(ContentLayerClient* client) { return make_scoped_refptr(new NoScaleContentLayer(client)); }

    virtual void calculateContentsScale(
        float idealContentsScale,
        float* contentsScaleX,
        float* contentsScaleY,
        gfx::Size* contentBounds) OVERRIDE
    {
        Layer::calculateContentsScale(
             idealContentsScale,
             contentsScaleX,
             contentsScaleY,
             contentBounds);
    }

private:
    explicit NoScaleContentLayer(ContentLayerClient* client)
        : ContentLayer(client) { }
    virtual ~NoScaleContentLayer() { }
};

class LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers : public LayerTreeHostTest {
public:

    LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
        : m_rootLayer(NoScaleContentLayer::create(&m_client))
        , m_childLayer(ContentLayer::create(&m_client))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(40, 40), gfx::Size(60, 60));
        m_layerTreeHost->setDeviceScaleFactor(1.5);
        EXPECT_EQ(gfx::Size(40, 40), m_layerTreeHost->layoutViewportSize());
        EXPECT_EQ(gfx::Size(60, 60), m_layerTreeHost->deviceViewportSize());

        m_rootLayer->addChild(m_childLayer);

        m_rootLayer->setIsDrawable(true);
        m_rootLayer->setBounds(gfx::Size(30, 30));
        m_rootLayer->setAnchorPoint(gfx::PointF(0, 0));

        m_childLayer->setIsDrawable(true);
        m_childLayer->setPosition(gfx::Point(2, 2));
        m_childLayer->setBounds(gfx::Size(10, 10));
        m_childLayer->setAnchorPoint(gfx::PointF(0, 0));

        m_layerTreeHost->setRootLayer(m_rootLayer);

        ASSERT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        ResourceUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        // Get access to protected methods.
        MockLayerTreeHostImpl* mockImpl = static_cast<MockLayerTreeHostImpl*>(impl);

        // Should only do one commit.
        EXPECT_EQ(0, impl->activeTree()->source_frame_number());
        // Device scale factor should come over to impl.
        EXPECT_NEAR(impl->deviceScaleFactor(), 1.5, 0.00001);

        // Both layers are on impl.
        ASSERT_EQ(1u, impl->rootLayer()->children().size());

        // Device viewport is scaled.
        EXPECT_EQ(gfx::Size(40, 40), impl->layoutViewportSize());
        EXPECT_EQ(gfx::Size(60, 60), impl->deviceViewportSize());

        LayerImpl* root = impl->rootLayer();
        LayerImpl* child = impl->rootLayer()->children()[0];

        // Positions remain in layout pixels.
        EXPECT_EQ(gfx::Point(0, 0), root->position());
        EXPECT_EQ(gfx::Point(2, 2), child->position());

        // Compute all the layer transforms for the frame.
        LayerTreeHostImpl::FrameData frameData;
        mockImpl->prepareToDraw(frameData);
        mockImpl->didDrawAllLayers(frameData);

        const MockLayerTreeHostImpl::LayerList& renderSurfaceLayerList =
          *frameData.renderSurfaceLayerList;

        // Both layers should be drawing into the root render surface.
        ASSERT_EQ(1u, renderSurfaceLayerList.size());
        ASSERT_EQ(root->renderSurface(), renderSurfaceLayerList[0]->renderSurface());
        ASSERT_EQ(2u, root->renderSurface()->layerList().size());

        // The root render surface is the size of the viewport.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 60, 60), root->renderSurface()->contentRect());

        // The content bounds of the child should be scaled.
        gfx::Size childBoundsScaled = gfx::ToCeiledSize(gfx::ScaleSize(child->bounds(), 1.5));
        EXPECT_EQ(childBoundsScaled, child->contentBounds());

        gfx::Transform scaleTransform;
        scaleTransform.Scale(impl->deviceScaleFactor(), impl->deviceScaleFactor());

        // The root layer is scaled by 2x.
        gfx::Transform rootScreenSpaceTransform = scaleTransform;
        gfx::Transform rootDrawTransform = scaleTransform;

        EXPECT_EQ(rootDrawTransform, root->drawTransform());
        EXPECT_EQ(rootScreenSpaceTransform, root->screenSpaceTransform());

        // The child is at position 2,2, which is transformed to 3,3 after the scale
        gfx::Transform childScreenSpaceTransform;
        childScreenSpaceTransform.Translate(3, 3);
        gfx::Transform childDrawTransform = childScreenSpaceTransform;

        EXPECT_TRANSFORMATION_MATRIX_EQ(childDrawTransform, child->drawTransform());
        EXPECT_TRANSFORMATION_MATRIX_EQ(childScreenSpaceTransform, child->screenSpaceTransform());

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        m_rootLayer = NULL;
        m_childLayer = NULL;
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<NoScaleContentLayer> m_rootLayer;
    scoped_refptr<ContentLayer> m_childLayer;
};

TEST_F(LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers, runMultiThread)
{
    runTest(true);
}

// Verify atomicity of commits and reuse of textures.
class LayerTreeHostTestAtomicCommit : public LayerTreeHostTest {
public:
    LayerTreeHostTestAtomicCommit()
    {
        // Make sure partial texture updates are turned off.
        m_settings.maxPartialTextureUpdates = 0;
        // Linear fade animator prevents scrollbars from drawing immediately.
        m_settings.useLinearFadeScrollbarAnimator = false;
    }

    virtual void setupTree() OVERRIDE
    {
        m_layer = FakeContentLayer::Create(&m_client);
        m_layer->setBounds(gfx::Size(10, 20));

        bool paint_scrollbar = true;
        bool has_thumb = false;
        m_scrollbar = FakeScrollbarLayer::Create(
            paint_scrollbar, has_thumb, m_layer->id());
        m_scrollbar->setPosition(gfx::Point(0, 10));
        m_scrollbar->setBounds(gfx::Size(10, 10));

        m_layer->addChild(m_scrollbar);

        m_layerTreeHost->setRootLayer(m_layer);
        LayerTreeHostTest::setupTree();
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        ASSERT_EQ(0u, m_layerTreeHost->settings().maxPartialTextureUpdates);

        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        switch (impl->activeTree()->source_frame_number()) {
        case 0:
            // Number of textures should be one for each layer
            ASSERT_EQ(2, context->NumTextures());
            // Number of textures used for commit should be one for each layer.
            EXPECT_EQ(2, context->NumUsedTextures());
            // Verify that used texture is correct.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 1:
            // Number of textures should be doubled as the first textures
            // are used by impl thread and cannot by used for update.
            ASSERT_EQ(4, context->NumTextures());
            // Number of textures used for commit should still be one for each layer.
            EXPECT_EQ(2, context->NumUsedTextures());
            // First textures should not have been used.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
            // New textures should have been used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 2:
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        // Number of textures used for draw should always be one for each layer.
        EXPECT_EQ(2, context->NumUsedTextures());
        context->ResetUsedTextures();
    }

    virtual void layout() OVERRIDE
    {
        m_layer->setNeedsDisplay();
        m_scrollbar->setNeedsDisplay();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<FakeContentLayer> m_layer;
    scoped_refptr<FakeScrollbarLayer> m_scrollbar;
};

TEST_F(LayerTreeHostTestAtomicCommit, runMultiThread)
{
    runTest(true);
}

static void setLayerPropertiesForTesting(Layer* layer, Layer* parent, const gfx::Transform& transform, const gfx::PointF& anchor, const gfx::PointF& position, const gfx::Size& bounds, bool opaque)
{
    layer->removeAllChildren();
    if (parent)
        parent->addChild(layer);
    layer->setTransform(transform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setContentsOpaque(opaque);
}

class LayerTreeHostTestAtomicCommitWithPartialUpdate : public LayerTreeHostTest {
public:
    LayerTreeHostTestAtomicCommitWithPartialUpdate()
        : m_numCommits(0)
    {
        // Allow one partial texture update.
        m_settings.maxPartialTextureUpdates = 1;
        // Linear fade animator prevents scrollbars from drawing immediately.
        m_settings.useLinearFadeScrollbarAnimator = false;
    }

    virtual void setupTree() OVERRIDE
    {
        m_parent = FakeContentLayer::Create(&m_client);
        m_parent->setBounds(gfx::Size(10, 20));

        m_child = FakeContentLayer::Create(&m_client);
        m_child->setPosition(gfx::Point(0, 10));
        m_child->setBounds(gfx::Size(3, 10));

        bool paint_scrollbar = true;
        bool has_thumb = false;
        m_scrollbarWithPaints = FakeScrollbarLayer::Create(
            paint_scrollbar, has_thumb, m_parent->id());
        m_scrollbarWithPaints->setPosition(gfx::Point(3, 10));
        m_scrollbarWithPaints->setBounds(gfx::Size(3, 10));

        paint_scrollbar = false;
        m_scrollbarWithoutPaints = FakeScrollbarLayer::Create(
            paint_scrollbar, has_thumb, m_parent->id());
        m_scrollbarWithoutPaints->setPosition(gfx::Point(6, 10));
        m_scrollbarWithoutPaints->setBounds(gfx::Size(3, 10));

        m_parent->addChild(m_child);
        m_parent->addChild(m_scrollbarWithPaints);
        m_parent->addChild(m_scrollbarWithoutPaints);

        m_layerTreeHost->setRootLayer(m_parent);
        LayerTreeHostTest::setupTree();
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        ASSERT_EQ(1u, m_layerTreeHost->settings().maxPartialTextureUpdates);

        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        switch (impl->activeTree()->source_frame_number()) {
        case 0:
            // Number of textures should be one for each layer.
            ASSERT_EQ(4, context->NumTextures());
            // Number of textures used for commit should be one for each layer.
            EXPECT_EQ(4, context->NumUsedTextures());
            // Verify that used textures are correct.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 1:
            // Number of textures should be two for each content layer and one
            // for each scrollbar, since they always do a partial update.
            ASSERT_EQ(6, context->NumTextures());
            // Number of textures used for commit should be one for each content
            // layer, and one for the scrollbar layer that paints.
            EXPECT_EQ(3, context->NumUsedTextures());

            // First content textures should not have been used.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
            // The non-painting scrollbar's texture wasn't updated.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(2)));
            // The painting scrollbar's partial update texture was used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
            // New textures should have been used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(4)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(5)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 2:
            // Number of textures should be two for each content layer and one
            // for each scrollbar, since they always do a partial update.
            ASSERT_EQ(6, context->NumTextures());
            // Number of textures used for commit should be one for each content
            // layer, and one for the scrollbar layer that paints.
            EXPECT_EQ(3, context->NumUsedTextures());

            // The non-painting scrollbar's texture wasn't updated.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(2)));
            // The painting scrollbar does a partial update.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
            // One content layer does a partial update also.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(4)));
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(5)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 3:
            // No textures should be used for commit.
            EXPECT_EQ(0, context->NumUsedTextures());

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 4:
            // Number of textures used for commit should be two. One for the
            // content layer, and one for the painting scrollbar. The
            // non-painting scrollbar doesn't update its texture.
            EXPECT_EQ(2, context->NumUsedTextures());

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 5:
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        // Number of textures used for drawing should one per layer except for
        // frame 3 where the viewport only contains one layer.
        if (impl->activeTree()->source_frame_number() == 3)
            EXPECT_EQ(1, context->NumUsedTextures());
        else
            EXPECT_EQ(4, context->NumUsedTextures());

        context->ResetUsedTextures();
    }

    virtual void layout() OVERRIDE
    {
        switch (m_numCommits++) {
        case 0:
        case 1:
            m_parent->setNeedsDisplay();
            m_child->setNeedsDisplay();
            m_scrollbarWithPaints->setNeedsDisplay();
            m_scrollbarWithoutPaints->setNeedsDisplay();
            break;
        case 2:
            // Damage part of layers.
            m_parent->setNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            m_child->setNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            m_scrollbarWithPaints->setNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            m_scrollbarWithoutPaints->setNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            break;
        case 3:
            m_child->setNeedsDisplay();
            m_scrollbarWithPaints->setNeedsDisplay();
            m_scrollbarWithoutPaints->setNeedsDisplay();
            m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
            break;
        case 4:
            m_layerTreeHost->setViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));
            break;
        case 5:
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<FakeContentLayer> m_parent;
    scoped_refptr<FakeContentLayer> m_child;
    scoped_refptr<FakeScrollbarLayer> m_scrollbarWithPaints;
    scoped_refptr<FakeScrollbarLayer> m_scrollbarWithoutPaints;
    int m_numCommits;
};

TEST_F(LayerTreeHostTestAtomicCommitWithPartialUpdate, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestFinishAllRendering : public LayerTreeHostTest {
public:
    LayerTreeHostTestFinishAllRendering()
        : m_once(false)
        , m_drawCount(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setNeedsRedraw();
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        if (m_once)
            return;
        m_once = true;
        m_layerTreeHost->setNeedsRedraw();
        m_layerTreeHost->acquireLayerTextures();
        {
            base::AutoLock lock(m_lock);
            m_drawCount = 0;
        }
        m_layerTreeHost->finishAllRendering();
        {
            base::AutoLock lock(m_lock);
            EXPECT_EQ(0, m_drawCount);
        }
        endTest();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        base::AutoLock lock(m_lock);
        ++m_drawCount;
    }

    virtual void afterTest() OVERRIDE
    {
    }
private:

    bool m_once;
    base::Lock m_lock;
    int m_drawCount;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestFinishAllRendering)

class LayerTreeHostTestCompositeAndReadbackCleanup : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackCleanup() { }

    virtual void beginTest() OVERRIDE
    {
        Layer* rootLayer = m_layerTreeHost->rootLayer();

        char pixels[4];
        m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        EXPECT_FALSE(rootLayer->renderSurface());

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadbackCleanup)

class LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit : public LayerTreeHostTest {
public:
    LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit()
        : m_rootLayer(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_surfaceLayer1(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_replicaLayer1(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_surfaceLayer2(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_replicaLayer2(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

        m_rootLayer->setBounds(gfx::Size(100, 100));
        m_surfaceLayer1->setBounds(gfx::Size(100, 100));
        m_surfaceLayer1->setForceRenderSurface(true);
        m_surfaceLayer1->setOpacity(0.5);
        m_surfaceLayer2->setBounds(gfx::Size(100, 100));
        m_surfaceLayer2->setForceRenderSurface(true);
        m_surfaceLayer2->setOpacity(0.5);

        m_surfaceLayer1->setReplicaLayer(m_replicaLayer1.get());
        m_surfaceLayer2->setReplicaLayer(m_replicaLayer2.get());

        m_rootLayer->addChild(m_surfaceLayer1);
        m_surfaceLayer1->addChild(m_surfaceLayer2);
        m_layerTreeHost->setRootLayer(m_rootLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* hostImpl) OVERRIDE
    {
        Renderer* renderer = hostImpl->renderer();
        RenderPass::Id surface1RenderPassId = hostImpl->rootLayer()->children()[0]->renderSurface()->renderPassId();
        RenderPass::Id surface2RenderPassId = hostImpl->rootLayer()->children()[0]->children()[0]->renderSurface()->renderPassId();

        switch (hostImpl->activeTree()->source_frame_number()) {
        case 0:
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            // Reduce the memory limit to only fit the root layer and one render surface. This
            // prevents any contents drawing into surfaces from being allocated.
            hostImpl->setManagedMemoryPolicy(ManagedMemoryPolicy(100 * 100 * 4 * 2));
            break;
        case 1:
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            endTest();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_rootLayer->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer1->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer2->paintContentsCount());

        // Clear layer references so LayerTreeHost dies.
        m_rootLayer = NULL;
        m_surfaceLayer1 = NULL;
        m_replicaLayer1 = NULL;
        m_surfaceLayer2 = NULL;
        m_replicaLayer2 = NULL;
    }

private:
    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<ContentLayerWithUpdateTracking> m_rootLayer;
    scoped_refptr<ContentLayerWithUpdateTracking> m_surfaceLayer1;
    scoped_refptr<ContentLayerWithUpdateTracking> m_replicaLayer1;
    scoped_refptr<ContentLayerWithUpdateTracking> m_surfaceLayer2;
    scoped_refptr<ContentLayerWithUpdateTracking> m_replicaLayer2;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit)

class EvictionTestLayer : public Layer {
public:
    static scoped_refptr<EvictionTestLayer> create() { return make_scoped_refptr(new EvictionTestLayer()); }

    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE { return true; }

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;

    bool haveBackingTexture() const { return m_texture.get() ? m_texture->haveBackingTexture() : false; }

private:
    EvictionTestLayer() : Layer() { }
    virtual ~EvictionTestLayer() { }

    void createTextureIfNeeded()
    {
        if (m_texture.get())
            return;
        m_texture = PrioritizedResource::create(layerTreeHost()->contentsTextureManager());
        m_texture->setDimensions(gfx::Size(10, 10), GL_RGBA);
        m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    }

    scoped_ptr<PrioritizedResource> m_texture;
    SkBitmap m_bitmap;
};

class EvictionTestLayerImpl : public LayerImpl {
public:
    static scoped_ptr<EvictionTestLayerImpl> create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new EvictionTestLayerImpl(treeImpl, id));
    }
    virtual ~EvictionTestLayerImpl() { }

    virtual void appendQuads(QuadSink& quadSink, AppendQuadsData&) OVERRIDE
    {
        ASSERT_TRUE(m_hasTexture);
        ASSERT_NE(0u, layerTreeImpl()->resource_provider()->numResources());
    }

    void setHasTexture(bool hasTexture) { m_hasTexture = hasTexture; }

private:
    EvictionTestLayerImpl(LayerTreeImpl* treeImpl, int id)
        : LayerImpl(treeImpl, id)
        , m_hasTexture(false) { }

    bool m_hasTexture;
};

void EvictionTestLayer::setTexturePriorities(const PriorityCalculator&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;
    m_texture->setRequestPriority(PriorityCalculator::uiPriority(true));
}

void EvictionTestLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker*, RenderingStats&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;

    gfx::Rect fullRect(0, 0, 10, 10);
    ResourceUpdate upload = ResourceUpdate::Create(
        m_texture.get(), &m_bitmap, fullRect, fullRect, gfx::Vector2d());
    queue.appendFullUpload(upload);
}

scoped_ptr<LayerImpl> EvictionTestLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return EvictionTestLayerImpl::create(treeImpl, m_layerId).PassAs<LayerImpl>();
}

void EvictionTestLayer::pushPropertiesTo(LayerImpl* layerImpl)
{
    Layer::pushPropertiesTo(layerImpl);

    EvictionTestLayerImpl* testLayerImpl = static_cast<EvictionTestLayerImpl*>(layerImpl);
    testLayerImpl->setHasTexture(m_texture->haveBackingTexture());
}

class LayerTreeHostTestEvictTextures : public LayerTreeHostTest {
public:
    LayerTreeHostTestEvictTextures()
        : m_layer(EvictionTestLayer::create())
        , m_implForEvictTextures(0)
        , m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));

        gfx::Transform identityMatrix;
        setLayerPropertiesForTesting(m_layer.get(), 0, identityMatrix, gfx::PointF(0, 0), gfx::PointF(0, 0), gfx::Size(10, 20), true);

        postSetNeedsCommitToMainThread();
    }

    void postEvictTextures()
    {
        DCHECK(implThread());
        implThread()->postTask(base::Bind(&LayerTreeHostTestEvictTextures::evictTexturesOnImplThread,
                               base::Unretained(this)));
    }

    void evictTexturesOnImplThread()
    {
        DCHECK(m_implForEvictTextures);
        m_implForEvictTextures->enforceManagedMemoryPolicy(ManagedMemoryPolicy(0));
    }

    // Commit 1: Just commit and draw normally, then post an eviction at the end
    // that will trigger a commit.
    // Commit 2: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 3: Triggered by the setNeedsCommit. In layout(), post an eviction
    // task, which will be handled before the commit. Don't set needsCommit, it
    // should have been posted. A frame should not be drawn (note,
    // didCommitAndDrawFrame may be called anyway).
    // Commit 4: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 5: Triggered by the setNeedsCommit, post an eviction task in
    // layout(), a frame should not be drawn but a commit will be posted.
    // Commit 6: Triggered by the eviction, post an eviction task in
    // layout(), which will be a noop, letting the commit (which recreates the
    // textures) go through and draw a frame, then end the test.
    //
    // Commits 1+2 test the eviction recovery path where eviction happens outside
    // of the beginFrame/commit pair.
    // Commits 3+4 test the eviction recovery path where eviction happens inside
    // the beginFrame/commit pair.
    // Commits 5+6 test the path where an eviction happens during the eviction
    // recovery path.
    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        switch (m_numCommits) {
        case 1:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        case 2:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 3:
            break;
        case 4:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 5:
            break;
        case 6:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_implForEvictTextures = impl;
    }

    virtual void layout() OVERRIDE
    {
        ++m_numCommits;
        switch (m_numCommits) {
        case 1:
        case 2:
            break;
        case 3:
            postEvictTextures();
            break;
        case 4:
            // We couldn't check in didCommitAndDrawFrame on commit 3, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            break;
        case 5:
            postEvictTextures();
            break;
        case 6:
            // We couldn't check in didCommitAndDrawFrame on commit 5, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<EvictionTestLayer> m_layer;
    LayerTreeHostImpl* m_implForEvictTextures;
    int m_numCommits;
};

TEST_F(LayerTreeHostTestEvictTextures, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestContinuousCommit : public LayerTreeHostTest {
public:
    LayerTreeHostTestContinuousCommit()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(gfx::Size(10, 10));

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        if (m_numDrawLayers == 2)
            return;
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);
    }

private:
    int m_numCommitComplete;
    int m_numDrawLayers;
};

TEST_F(LayerTreeHostTestContinuousCommit, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestContinuousInvalidate : public LayerTreeHostTest {
public:
    LayerTreeHostTestContinuousInvalidate()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(gfx::Size(10, 10));

        m_contentLayer = ContentLayer::create(&m_fakeDelegate);
        m_contentLayer->setBounds(gfx::Size(10, 10));
        m_contentLayer->setPosition(gfx::PointF(0, 0));
        m_contentLayer->setAnchorPoint(gfx::PointF(0, 0));
        m_contentLayer->setIsDrawable(true);
        m_layerTreeHost->rootLayer()->addChild(m_contentLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        if (m_numDrawLayers == 2)
            return;
        m_contentLayer->setNeedsDisplay();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);

        // Clear layer references so LayerTreeHost dies.
        m_contentLayer = NULL;
    }

private:
    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<Layer> m_contentLayer;
    int m_numCommitComplete;
    int m_numDrawLayers;
};

TEST_F(LayerTreeHostTestContinuousInvalidate, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestAdjustPointForZoom : public LayerTreeHostTest {
public:
    LayerTreeHostTestAdjustPointForZoom()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        gfx::Transform m;
        m.Translate(250, 360);
        m.Scale(2, 2);

        gfx::Point point(400, 550);
        gfx::Point transformedPoint;

        // Unit transform, no change expected.
        m_layerTreeHost->setImplTransform(gfx::Transform());
        transformedPoint = gfx::ToRoundedPoint(m_layerTreeHost->adjustEventPointForPinchZoom(point));
        EXPECT_EQ(point.x(), transformedPoint.x());
        EXPECT_EQ(point.y(), transformedPoint.y());

        m_layerTreeHost->setImplTransform(m);

        // Apply m^(-1): 75 = (400 - 250) / 2; 95 = (550 - 360) / 2.
        transformedPoint = gfx::ToRoundedPoint(m_layerTreeHost->adjustEventPointForPinchZoom(point));
        EXPECT_EQ(75, transformedPoint.x());
        EXPECT_EQ(95, transformedPoint.y());
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

TEST_F(LayerTreeHostTestAdjustPointForZoom, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestDeferCommits : public LayerTreeHostTest {
public:
    LayerTreeHostTestDeferCommits()
        : m_numCommitsDeferred(0)
        , m_numCompleteCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didDeferCommit() OVERRIDE
    {
        m_numCommitsDeferred++;
        m_layerTreeHost->setDeferCommits(false);
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCompleteCommits++;
        switch (m_numCompleteCommits) {
        case 1:
            EXPECT_EQ(0, m_numCommitsDeferred);
            m_layerTreeHost->setDeferCommits(true);
            postSetNeedsCommitToMainThread();
            break;
        case 2:
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numCommitsDeferred);
        EXPECT_EQ(2, m_numCompleteCommits);
    }

private:
    int m_numCommitsDeferred;
    int m_numCompleteCommits;
};

TEST_F(LayerTreeHostTestDeferCommits, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostWithProxy : public LayerTreeHost {
public:
    LayerTreeHostWithProxy(FakeLayerImplTreeHostClient* client, const LayerTreeSettings& settings, scoped_ptr<Proxy> proxy)
            : LayerTreeHost(client, settings)
    {
        EXPECT_TRUE(initializeForTesting(proxy.Pass()));
    }

private:
    FakeLayerImplTreeHostClient m_client;
};

TEST(LayerTreeHostTest, LimitPartialUpdates)
{
    // When partial updates are not allowed, max updates should be 0.
    {
        FakeLayerImplTreeHostClient client;

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->rendererCapabilities().allowPartialTextureUpdates = false;
        proxy->setMaxPartialTextureUpdates(5);

        LayerTreeSettings settings;
        settings.maxPartialTextureUpdates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.initializeRendererIfNeeded());

        EXPECT_EQ(0u, host.settings().maxPartialTextureUpdates);
    }

    // When partial updates are allowed, max updates should be limited by the proxy.
    {
        FakeLayerImplTreeHostClient client;

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->rendererCapabilities().allowPartialTextureUpdates = true;
        proxy->setMaxPartialTextureUpdates(5);

        LayerTreeSettings settings;
        settings.maxPartialTextureUpdates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.initializeRendererIfNeeded());

        EXPECT_EQ(5u, host.settings().maxPartialTextureUpdates);
    }

    // When partial updates are allowed, max updates should also be limited by the settings.
    {
        FakeLayerImplTreeHostClient client;

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->rendererCapabilities().allowPartialTextureUpdates = true;
        proxy->setMaxPartialTextureUpdates(20);

        LayerTreeSettings settings;
        settings.maxPartialTextureUpdates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.initializeRendererIfNeeded());

        EXPECT_EQ(10u, host.settings().maxPartialTextureUpdates);
    }
}

TEST(LayerTreeHostTest, PartialUpdatesWithGLRenderer)
{
    bool useSoftwareRendering = false;
    bool useDelegatingRenderer = false;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(4u, host->settings().maxPartialTextureUpdates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithSoftwareRenderer)
{
    bool useSoftwareRendering = true;
    bool useDelegatingRenderer = false;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(4u, host->settings().maxPartialTextureUpdates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndGLContent)
{
    bool useSoftwareRendering = false;
    bool useDelegatingRenderer = true;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(0u, host->settings().maxPartialTextureUpdates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndSoftwareContent)
{
    bool useSoftwareRendering = true;
    bool useDelegatingRenderer = true;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(0u, host->settings().maxPartialTextureUpdates);
}

class LayerTreeHostTestCapturePicture : public LayerTreeHostTest {
public:
    LayerTreeHostTestCapturePicture()
        : m_bounds(gfx::Size(100, 100))
        , m_layer(PictureLayer::create(&m_contentClient))
    {
        m_settings.implSidePainting = true;
    }

    class FillRectContentLayerClient : public ContentLayerClient {
    public:
        virtual void paintContents(SkCanvas* canvas, const gfx::Rect& clip, gfx::RectF& opaque) OVERRIDE
        {
            SkPaint paint;
            paint.setColor(SK_ColorGREEN);

            SkRect rect = SkRect::MakeWH(canvas->getDeviceSize().width(), canvas->getDeviceSize().height());
            opaque = gfx::RectF(rect.width(), rect.height());
            canvas->drawRect(rect, paint);
        }
    };

    virtual void beginTest() OVERRIDE
    {
        m_layer->setIsDrawable(true);
        m_layer->setBounds(m_bounds);
        m_layerTreeHost->setViewportSize(m_bounds, m_bounds);
        m_layerTreeHost->setRootLayer(m_layer);

        EXPECT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_picture = m_layerTreeHost->capturePicture();
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(m_bounds, gfx::Size(m_picture->width(), m_picture->height()));

        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, m_bounds.width(), m_bounds.height());
        bitmap.allocPixels();
        bitmap.eraseARGB(0, 0, 0, 0);
        SkCanvas canvas(bitmap);

        m_picture->draw(&canvas);

        bitmap.lockPixels();
        SkColor* pixels = reinterpret_cast<SkColor*>(bitmap.getPixels());
        EXPECT_EQ(SK_ColorGREEN, pixels[0]);
        bitmap.unlockPixels();
    }

private:
    gfx::Size m_bounds;
    FillRectContentLayerClient m_contentClient;
    scoped_refptr<PictureLayer> m_layer;
    skia::RefPtr<SkPicture> m_picture;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCapturePicture);

}  // namespace
}  // namespace cc
