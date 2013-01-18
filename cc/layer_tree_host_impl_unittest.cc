// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host_impl.h"

#include <cmath>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "cc/compositor_frame_metadata.h"
#include "cc/delegated_renderer_layer_impl.h"
#include "cc/gl_renderer.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/io_surface_layer_impl.h"
#include "cc/layer_impl.h"
#include "cc/layer_tiling_data.h"
#include "cc/layer_tree_impl.h"
#include "cc/math_util.h"
#include "cc/quad_sink.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/scrollbar_geometry_fixed_thumb.h"
#include "cc/scrollbar_layer_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/texture_draw_quad.h"
#include "cc/texture_layer_impl.h"
#include "cc/tile_draw_quad.h"
#include "cc/tiled_layer_impl.h"
#include "cc/video_layer_impl.h"
#include "media/base/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

using ::testing::Mock;
using ::testing::Return;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::_;
using media::VideoFrame;

namespace cc {
namespace {

// This test is parametrized to run all tests with the
// m_settings.pageScalePinchZoomEnabled field enabled and disabled.
class LayerTreeHostImplTest : public testing::TestWithParam<bool>,
                              public LayerTreeHostImplClient {
public:
    LayerTreeHostImplTest()
        : m_proxy(scoped_ptr<Thread>(NULL))
        , m_alwaysImplThread(&m_proxy)
        , m_alwaysMainThreadBlocked(&m_proxy)
        , m_onCanDrawStateChangedCalled(false)
        , m_hasPendingTree(false)
        , m_didRequestCommit(false)
        , m_didRequestRedraw(false)
        , m_didSwapUseIncompleteTexture(false)
        , m_didUploadVisibleHighResolutionTile(false)
        , m_reduceMemoryResult(true)
    {
        media::InitializeMediaLibraryForTesting();
    }

    virtual void SetUp()
    {
        LayerTreeSettings settings;
        settings.minimumOcclusionTrackingSize = gfx::Size();
        settings.pageScalePinchZoomEnabled = GetParam();

        m_hostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);
        m_hostImpl->initializeRenderer(createOutputSurface());
        m_hostImpl->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    }

    virtual void TearDown()
    {
    }

    virtual void didLoseOutputSurfaceOnImplThread() OVERRIDE { }
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE { }
    virtual void onVSyncParametersChanged(base::TimeTicks, base::TimeDelta) OVERRIDE { }
    virtual void onCanDrawStateChanged(bool canDraw) OVERRIDE { m_onCanDrawStateChangedCalled = true; }
    virtual void onHasPendingTreeStateChanged(bool hasPendingTree) OVERRIDE { m_hasPendingTree = hasPendingTree; }
    virtual void setNeedsRedrawOnImplThread() OVERRIDE { m_didRequestRedraw = true; }
    virtual void didSwapUseIncompleteTextureOnImplThread() OVERRIDE { m_didSwapUseIncompleteTexture = true; }
    virtual void didUploadVisibleHighResolutionTileOnImplTread() OVERRIDE { m_didUploadVisibleHighResolutionTile = true; }
    virtual void setNeedsCommitOnImplThread() OVERRIDE { m_didRequestCommit = true; }
    virtual void setNeedsManageTilesOnImplThread() OVERRIDE { }
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime) OVERRIDE { }
    virtual bool reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE { return m_reduceMemoryResult; }
    virtual void sendManagedMemoryStats() OVERRIDE { }
    virtual bool isInsideDraw() OVERRIDE { return false; }

    void setReduceMemoryResult(bool reduceMemoryResult) { m_reduceMemoryResult = reduceMemoryResult; }

    void createLayerTreeHost(bool partialSwap, scoped_ptr<OutputSurface> outputSurface)
    {
        LayerTreeSettings settings;
        settings.minimumOcclusionTrackingSize = gfx::Size();
        settings.partialSwapEnabled = partialSwap;

        m_hostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

        m_hostImpl->initializeRenderer(outputSurface.Pass());
        m_hostImpl->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    }

    void setupRootLayerImpl(scoped_ptr<LayerImpl> root)
    {
        root->setAnchorPoint(gfx::PointF(0, 0));
        root->setPosition(gfx::PointF(0, 0));
        root->setBounds(gfx::Size(10, 10));
        root->setContentBounds(gfx::Size(10, 10));
        root->setDrawsContent(true);
        root->drawProperties().visible_content_rect = gfx::Rect(0, 0, 10, 10);
        m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    }

    static void expectClearedScrollDeltasRecursive(LayerImpl* layer)
    {
        ASSERT_EQ(layer->scrollDelta(), gfx::Vector2d());
        for (size_t i = 0; i < layer->children().size(); ++i)
            expectClearedScrollDeltasRecursive(layer->children()[i]);
    }

    static void expectContains(const ScrollAndScaleSet& scrollInfo, int id, const gfx::Vector2d& scrollDelta)
    {
        int timesEncountered = 0;

        for (size_t i = 0; i < scrollInfo.scrolls.size(); ++i) {
            if (scrollInfo.scrolls[i].layerId != id)
                continue;
            EXPECT_VECTOR_EQ(scrollDelta, scrollInfo.scrolls[i].scrollDelta);
            timesEncountered++;
        }

        ASSERT_EQ(timesEncountered, 1);
    }

    static void expectNone(const ScrollAndScaleSet& scrollInfo, int id)
    {
        int timesEncountered = 0;

        for (size_t i = 0; i < scrollInfo.scrolls.size(); ++i) {
            if (scrollInfo.scrolls[i].layerId != id)
                continue;
            timesEncountered++;
        }

        ASSERT_EQ(0, timesEncountered);
    }

    void setupScrollAndContentsLayers(const gfx::Size& contentSize)
    {
        scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
        root->setScrollable(true);
        root->setScrollOffset(gfx::Vector2d(0, 0));
        root->setMaxScrollOffset(gfx::Vector2d(contentSize.width(), contentSize.height()));
        root->setBounds(contentSize);
        root->setContentBounds(contentSize);
        root->setPosition(gfx::PointF(0, 0));
        root->setAnchorPoint(gfx::PointF(0, 0));

        scoped_ptr<LayerImpl> contents = LayerImpl::create(m_hostImpl->activeTree(), 2);
        contents->setDrawsContent(true);
        contents->setBounds(contentSize);
        contents->setContentBounds(contentSize);
        contents->setPosition(gfx::PointF(0, 0));
        contents->setAnchorPoint(gfx::PointF(0, 0));
        root->addChild(contents.Pass());
        m_hostImpl->activeTree()->SetRootLayer(root.Pass());
        m_hostImpl->activeTree()->FindRootScrollLayer();
    }

    scoped_ptr<LayerImpl> createScrollableLayer(int id, const gfx::Size& size)
    {
        scoped_ptr<LayerImpl> layer = LayerImpl::create(m_hostImpl->activeTree(), id);
        layer->setScrollable(true);
        layer->setDrawsContent(true);
        layer->setBounds(size);
        layer->setContentBounds(size);
        layer->setMaxScrollOffset(gfx::Vector2d(size.width() * 2, size.height() * 2));
        return layer.Pass();
    }

    void initializeRendererAndDrawFrame()
    {
        m_hostImpl->initializeRenderer(createOutputSurface());
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }

    void pinchZoomPanViewportForcesCommitRedraw(const float deviceScaleFactor);
    void pinchZoomPanViewportTest(const float deviceScaleFactor);
    void pinchZoomPanViewportAndScrollTest(const float deviceScaleFactor);
    void pinchZoomPanViewportAndScrollBoundaryTest(const float deviceScaleFactor);

protected:
    virtual scoped_ptr<OutputSurface> createOutputSurface() { return createFakeOutputSurface(); }

    void drawOneFrame() {
      LayerTreeHostImpl::FrameData frameData;
      m_hostImpl->prepareToDraw(frameData);
      m_hostImpl->didDrawAllLayers(frameData);
    }

    FakeProxy m_proxy;
    DebugScopedSetImplThread m_alwaysImplThread;
    DebugScopedSetMainThreadBlocked m_alwaysMainThreadBlocked;

    scoped_ptr<LayerTreeHostImpl> m_hostImpl;
    bool m_onCanDrawStateChangedCalled;
    bool m_hasPendingTree;
    bool m_didRequestCommit;
    bool m_didRequestRedraw;
    bool m_didSwapUseIncompleteTexture;
    bool m_didUploadVisibleHighResolutionTile;
    bool m_reduceMemoryResult;
};

class FakeWebGraphicsContext3DMakeCurrentFails : public FakeWebGraphicsContext3D {
public:
    virtual bool makeContextCurrent() { return false; }
};

TEST_P(LayerTreeHostImplTest, notifyIfCanDrawChanged)
{
    // Note: It is not possible to disable the renderer once it has been set,
    // so we do not need to test that disabling the renderer notifies us
    // that canDraw changed.
    EXPECT_FALSE(m_hostImpl->canDraw());
    m_onCanDrawStateChangedCalled = false;

    setupScrollAndContentsLayers(gfx::Size(100, 100));
    EXPECT_TRUE(m_hostImpl->canDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle the root layer to make sure it toggles canDraw
    m_hostImpl->activeTree()->SetRootLayer(scoped_ptr<LayerImpl>());
    EXPECT_FALSE(m_hostImpl->canDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    setupScrollAndContentsLayers(gfx::Size(100, 100));
    EXPECT_TRUE(m_hostImpl->canDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle the device viewport size to make sure it toggles canDraw.
    m_hostImpl->setViewportSize(gfx::Size(100, 100), gfx::Size(0, 0));
    EXPECT_FALSE(m_hostImpl->canDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    m_hostImpl->setViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));
    EXPECT_TRUE(m_hostImpl->canDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle contents textures purged without causing any evictions,
    // and make sure that it does not change canDraw.
    setReduceMemoryResult(false);
    m_hostImpl->setManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memoryAllocationLimitBytes() - 1));
    EXPECT_TRUE(m_hostImpl->canDraw());
    EXPECT_FALSE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle contents textures purged to make sure it toggles canDraw.
    setReduceMemoryResult(true);
    m_hostImpl->setManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memoryAllocationLimitBytes() - 1));
    EXPECT_FALSE(m_hostImpl->canDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    m_hostImpl->activeTree()->ResetContentsTexturesPurged();
    EXPECT_TRUE(m_hostImpl->canDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;
}

TEST_P(LayerTreeHostImplTest, scrollDeltaNoLayers)
{
    ASSERT_FALSE(m_hostImpl->rootLayer());

    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 0u);
}

TEST_P(LayerTreeHostImplTest, scrollDeltaTreeButNoChanges)
{
    {
        scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
        root->addChild(LayerImpl::create(m_hostImpl->activeTree(), 2));
        root->addChild(LayerImpl::create(m_hostImpl->activeTree(), 3));
        root->children()[1]->addChild(LayerImpl::create(m_hostImpl->activeTree(), 4));
        root->children()[1]->addChild(LayerImpl::create(m_hostImpl->activeTree(), 5));
        root->children()[1]->children()[0]->addChild(LayerImpl::create(m_hostImpl->activeTree(), 6));
        m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    }
    LayerImpl* root = m_hostImpl->rootLayer();

    expectClearedScrollDeltasRecursive(root);

    scoped_ptr<ScrollAndScaleSet> scrollInfo;

    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 0u);
    expectClearedScrollDeltasRecursive(root);

    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 0u);
    expectClearedScrollDeltasRecursive(root);
}

TEST_P(LayerTreeHostImplTest, scrollDeltaRepeatedScrolls)
{
    gfx::Vector2d scrollOffset(20, 30);
    gfx::Vector2d scrollDelta(11, -15);
    {
        scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
        root->setScrollOffset(scrollOffset);
        root->setScrollable(true);
        root->setMaxScrollOffset(gfx::Vector2d(100, 100));
        root->scrollBy(scrollDelta);
        m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    }
    LayerImpl* root = m_hostImpl->rootLayer();

    scoped_ptr<ScrollAndScaleSet> scrollInfo;

    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 1u);
    EXPECT_VECTOR_EQ(root->sentScrollDelta(), scrollDelta);
    expectContains(*scrollInfo, root->id(), scrollDelta);

    gfx::Vector2d scrollDelta2(-5, 27);
    root->scrollBy(scrollDelta2);
    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 1u);
    EXPECT_VECTOR_EQ(root->sentScrollDelta(), scrollDelta + scrollDelta2);
    expectContains(*scrollInfo, root->id(), scrollDelta + scrollDelta2);

    root->scrollBy(gfx::Vector2d());
    scrollInfo = m_hostImpl->processScrollDeltas();
    EXPECT_EQ(root->sentScrollDelta(), scrollDelta + scrollDelta2);
}

TEST_P(LayerTreeHostImplTest, scrollRootCallsCommitAndRedraw)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->scrollEnd();
    EXPECT_TRUE(m_didRequestRedraw);
    EXPECT_TRUE(m_didRequestCommit);
}

TEST_P(LayerTreeHostImplTest, scrollWithoutRootLayer)
{
    // We should not crash when trying to scroll an empty layer tree.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
}

TEST_P(LayerTreeHostImplTest, scrollWithoutRenderer)
{
    LayerTreeSettings settings;
    m_hostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    // Initialization will fail here.
    m_hostImpl->initializeRenderer(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new FakeWebGraphicsContext3DMakeCurrentFails)).PassAs<OutputSurface>());
    m_hostImpl->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));

    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // We should not crash when trying to scroll after the renderer initialization fails.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
}

TEST_P(LayerTreeHostImplTest, replaceTreeWhileScrolling)
{
    const int scrollLayerId = 1;

    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    // We should not crash if the tree is replaced while we are scrolling.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->activeTree()->DetachLayerTree();

    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // We should still be scrolling, because the scrolled layer also exists in the new tree.
    gfx::Vector2d scrollDelta(0, 10);
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo, scrollLayerId, scrollDelta);
}

TEST_P(LayerTreeHostImplTest, clearRootRenderSurfaceAndScroll)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    // We should be able to scroll even if the root layer loses its render surface after the most
    // recent render.
    m_hostImpl->rootLayer()->clearRenderSurface();
    m_hostImpl->setNeedsUpdateDrawProperties();

    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
}

TEST_P(LayerTreeHostImplTest, wheelEventHandlers)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();
    LayerImpl* root = m_hostImpl->rootLayer();

    root->setHaveWheelEventHandlers(true);

    // With registered event handlers, wheel scrolls have to go to the main thread.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);

    // But gesture scrolls can still be handled.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
}

TEST_P(LayerTreeHostImplTest, shouldScrollOnMainThread)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();
    LayerImpl* root = m_hostImpl->rootLayer();

    root->setShouldScrollOnMainThread(true);

    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollOnMainThread);
}

TEST_P(LayerTreeHostImplTest, nonFastScrollableRegionBasic)
{
    setupScrollAndContentsLayers(gfx::Size(200, 200));
    m_hostImpl->setViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

    LayerImpl* root = m_hostImpl->rootLayer();
    root->setContentsScale(2, 2);
    root->setNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));

    initializeRendererAndDrawFrame();

    // All scroll types inside the non-fast scrollable region should fail.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(25, 25), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(25, 25), InputHandlerClient::Gesture), InputHandlerClient::ScrollOnMainThread);

    // All scroll types outside this region should succeed.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(75, 75), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->scrollEnd();
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(75, 75), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->scrollEnd();
}

TEST_P(LayerTreeHostImplTest, nonFastScrollableRegionWithOffset)
{
    setupScrollAndContentsLayers(gfx::Size(200, 200));
    m_hostImpl->setViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

    LayerImpl* root = m_hostImpl->rootLayer();
    root->setContentsScale(2, 2);
    root->setNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));
    root->setPosition(gfx::PointF(-25, 0));

    initializeRendererAndDrawFrame();

    // This point would fall into the non-fast scrollable region except that we've moved the layer down by 25 pixels.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(40, 10), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 1));
    m_hostImpl->scrollEnd();

    // This point is still inside the non-fast region.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(10, 10), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
}

TEST_P(LayerTreeHostImplTest, scrollByReturnsCorrectValue)
{
    setupScrollAndContentsLayers(gfx::Size(200, 200));
    m_hostImpl->setViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

    initializeRendererAndDrawFrame();

    EXPECT_EQ(InputHandlerClient::ScrollStarted,
        m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));

    // Trying to scroll to the left/top will not succeed.
    EXPECT_FALSE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
    EXPECT_FALSE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, -10)));
    EXPECT_FALSE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(-10, -10)));

    // Scrolling to the right/bottom will succeed.
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(10, 0)));
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 10)));
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(10, 10)));

    // Scrolling to left/top will now succeed.
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, -10)));
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(-10, -10)));

    // Scrolling diagonally against an edge will succeed.
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(10, -10)));
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(-10, 10)));

    // Trying to scroll more than the available space will also succeed.
    EXPECT_TRUE(m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(5000, 5000)));
}

TEST_P(LayerTreeHostImplTest, maxScrollOffsetChangedByDeviceScaleFactor)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));

    float deviceScaleFactor = 2;
    gfx::Size layoutViewport(25, 25);
    gfx::Size deviceViewport(gfx::ToFlooredSize(gfx::ScaleSize(layoutViewport, deviceScaleFactor)));
    m_hostImpl->setViewportSize(layoutViewport, deviceViewport);
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);
    EXPECT_EQ(m_hostImpl->rootLayer()->maxScrollOffset(), gfx::Vector2d(25, 25));

    deviceScaleFactor = 1;
    m_hostImpl->setViewportSize(layoutViewport, layoutViewport);
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);
    EXPECT_EQ(m_hostImpl->rootLayer()->maxScrollOffset(), gfx::Vector2d(75, 75));
}

TEST_P(LayerTreeHostImplTest, clearRootRenderSurfaceAndHitTestTouchHandlerRegion)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    // We should be able to hit test for touch event handlers even if the root layer loses
    // its render surface after the most recent render.
    m_hostImpl->rootLayer()->clearRenderSurface();
    m_hostImpl->setNeedsUpdateDrawProperties();

    EXPECT_EQ(m_hostImpl->haveTouchEventHandlersAt(gfx::Point(0, 0)), false);
}

TEST_P(LayerTreeHostImplTest, implPinchZoom)
{
    // This test is specific to the page-scale based pinch zoom.
    if (!m_hostImpl->settings().pageScalePinchZoomEnabled)
        return;

    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->rootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = 1, maxPageScale = 4;
    const gfx::Transform identityScaleTransform;

    // The impl-based pinch zoom should not adjust the max scroll position.
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollDelta(gfx::Vector2d());

        float pageScaleDelta = 2;
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(50, 50));
        m_hostImpl->pinchGestureEnd();
        EXPECT_TRUE(m_didRequestRedraw);
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);

        EXPECT_EQ(m_hostImpl->rootLayer()->maxScrollOffset(), gfx::Vector2d(50, 50));
    }

    // Scrolling after a pinch gesture should always be in local space.  The scroll deltas do not
    // have the page scale factor applied.
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollDelta(gfx::Vector2d());

        float pageScaleDelta = 2;
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(0, 0));
        m_hostImpl->pinchGestureEnd();

        gfx::Vector2d scrollDelta(0, 10);
        EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
        m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->scrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), scrollDelta);
    }
}

TEST_P(LayerTreeHostImplTest, pinchGesture)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->rootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = m_hostImpl->settings().pageScalePinchZoomEnabled ? 1 : 0.5;
    const float maxPageScale = 4;
    const gfx::Transform identityScaleTransform;

    // Basic pinch zoom in gesture
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollDelta(gfx::Vector2d());

        float pageScaleDelta = 2;
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(50, 50));
        m_hostImpl->pinchGestureEnd();
        EXPECT_TRUE(m_didRequestRedraw);
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
    }

    // Zoom-in clamping
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollDelta(gfx::Vector2d());
        float pageScaleDelta = 10;

        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(50, 50));
        m_hostImpl->pinchGestureEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, maxPageScale);
    }

    // Zoom-out clamping
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollDelta(gfx::Vector2d());
        scrollLayer->setScrollOffset(gfx::Vector2d(50, 50));

        float pageScaleDelta = 0.1f;
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(0, 0));
        m_hostImpl->pinchGestureEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, minPageScale);

        if (!m_hostImpl->settings().pageScalePinchZoomEnabled) {
            // Pushed to (0,0) via clamping against contents layer size.
            expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(-50, -50));
        } else {
            EXPECT_TRUE(scrollInfo->scrolls.empty());
        }
    }

    // Two-finger panning should not happen based on pinch events only
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollDelta(gfx::Vector2d());
        scrollLayer->setScrollOffset(gfx::Vector2d(20, 20));

        float pageScaleDelta = 1;
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(10, 10));
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(20, 20));
        m_hostImpl->pinchGestureEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
        EXPECT_TRUE(scrollInfo->scrolls.empty());
    }

    // Two-finger panning should work with interleaved scroll events
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollDelta(gfx::Vector2d());
        scrollLayer->setScrollOffset(gfx::Vector2d(20, 20));

        float pageScaleDelta = 1;
        m_hostImpl->scrollBegin(gfx::Point(10, 10), InputHandlerClient::Wheel);
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(10, 10));
        m_hostImpl->scrollBy(gfx::Point(10, 10), gfx::Vector2d(-10, -10));
        m_hostImpl->pinchGestureUpdate(pageScaleDelta, gfx::Point(20, 20));
        m_hostImpl->pinchGestureEnd();
        m_hostImpl->scrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
        expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(-10, -10));
    }
}

TEST_P(LayerTreeHostImplTest, pageScaleAnimation)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->rootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = 0.5;
    const float maxPageScale = 4;
    const base::TimeTicks startTime = base::TimeTicks() + base::TimeDelta::FromSeconds(1);
    const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
    const base::TimeTicks halfwayThroughAnimation = startTime + duration / 2;
    const base::TimeTicks endTime = startTime + duration;
    const gfx::Transform identityScaleTransform;

    // Non-anchor zoom-in
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollOffset(gfx::Vector2d(50, 50));

        m_hostImpl->startPageScaleAnimation(gfx::Vector2d(0, 0), false, 2, startTime, duration);
        m_hostImpl->animate(halfwayThroughAnimation, base::Time());
        EXPECT_TRUE(m_didRequestRedraw);
        m_hostImpl->animate(endTime, base::Time());
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, 2);
        expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(-50, -50));
    }

    // Anchor zoom-out
    {
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->setImplTransform(identityScaleTransform);
        scrollLayer->setScrollOffset(gfx::Vector2d(50, 50));

        m_hostImpl->startPageScaleAnimation(gfx::Vector2d(25, 25), true, minPageScale, startTime, duration);
        m_hostImpl->animate(endTime, base::Time());
        EXPECT_TRUE(m_didRequestRedraw);
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, minPageScale);
        // Pushed to (0,0) via clamping against contents layer size.
        expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(-50, -50));
    }
}

TEST_P(LayerTreeHostImplTest, inhibitScrollAndPageScaleUpdatesWhilePinchZooming)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->rootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = m_hostImpl->settings().pageScalePinchZoomEnabled ? 1 : 0.5;
    const float maxPageScale = 4;

    // Pinch zoom in.
    {
        // Start a pinch in gesture at the bottom right corner of the viewport.
        const float zoomInDelta = 2;
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(zoomInDelta, gfx::Point(50, 50));

        // Because we are pinch zooming in, we shouldn't get any scroll or page
        // scale deltas.
        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, 1);
        EXPECT_EQ(scrollInfo->scrolls.size(), 0u);

        // Once the gesture ends, we get the final scroll and page scale values.
        m_hostImpl->pinchGestureEnd();
        scrollInfo = m_hostImpl->processScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, zoomInDelta);
        if (!m_hostImpl->settings().pageScalePinchZoomEnabled) {
            expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(25, 25));
        } else {
            EXPECT_TRUE(scrollInfo->scrolls.empty());
        }
    }

    // Pinch zoom out.
    {
        // Start a pinch out gesture at the bottom right corner of the viewport.
        const float zoomOutDelta = 0.75;
        m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        m_hostImpl->pinchGestureBegin();
        m_hostImpl->pinchGestureUpdate(zoomOutDelta, gfx::Point(50, 50));

        // Since we are pinch zooming out, we should get an update to zoom all
        // the way out to the minimum page scale.
        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        if (!m_hostImpl->settings().pageScalePinchZoomEnabled) {
            EXPECT_EQ(scrollInfo->pageScaleDelta, minPageScale);
            expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(0, 0));
        } else {
            EXPECT_EQ(scrollInfo->pageScaleDelta, 1);
            EXPECT_TRUE(scrollInfo->scrolls.empty());
        }

        // Once the gesture ends, we get the final scroll and page scale values.
        m_hostImpl->pinchGestureEnd();
        scrollInfo = m_hostImpl->processScrollDeltas();
        if (m_hostImpl->settings().pageScalePinchZoomEnabled) {
            EXPECT_EQ(scrollInfo->pageScaleDelta, minPageScale);
            expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(25, 25));
        } else {
            EXPECT_EQ(scrollInfo->pageScaleDelta, zoomOutDelta);
            expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(8, 8));
        }
    }
}

TEST_P(LayerTreeHostImplTest, inhibitScrollAndPageScaleUpdatesWhileAnimatingPageScale)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->rootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = 0.5;
    const float maxPageScale = 4;
    const base::TimeTicks startTime = base::TimeTicks() + base::TimeDelta::FromSeconds(1);
    const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
    const base::TimeTicks halfwayThroughAnimation = startTime + duration / 2;
    const base::TimeTicks endTime = startTime + duration;

    const float pageScaleDelta = 2;
    gfx::Vector2d target(25, 25);
    gfx::Vector2d scaledTarget = target;
    if (!m_hostImpl->settings().pageScalePinchZoomEnabled)
      scaledTarget = gfx::Vector2d(12, 12);

    m_hostImpl->setPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
    m_hostImpl->startPageScaleAnimation(target, false, pageScaleDelta, startTime, duration);

    // We should immediately get the final zoom and scroll values for the
    // animation.
    m_hostImpl->animate(halfwayThroughAnimation, base::Time());
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
    expectContains(*scrollInfo, scrollLayer->id(), scaledTarget);

    // Scrolling during the animation is ignored.
    const gfx::Vector2d scrollDelta(0, 10);
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(target.x(), target.y()), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();

    // The final page scale and scroll deltas should match what we got
    // earlier.
    m_hostImpl->animate(endTime, base::Time());
    scrollInfo = m_hostImpl->processScrollDeltas();
    EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
    expectContains(*scrollInfo, scrollLayer->id(), scaledTarget);
}

TEST_P(LayerTreeHostImplTest, compositorFrameMetadata)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->setViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    m_hostImpl->setPageScaleFactorAndLimits(1.0f, 0.5f, 4.0f);
    initializeRendererAndDrawFrame();

    {
        CompositorFrameMetadata metadata = m_hostImpl->makeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 0.0f), metadata.root_scroll_offset);
        EXPECT_EQ(1.0f, metadata.page_scale_factor);
        EXPECT_EQ(gfx::SizeF(50.0f, 50.0f), metadata.viewport_size);
        EXPECT_EQ(gfx::SizeF(100.0f, 100.0f), metadata.root_layer_size);
        EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
        EXPECT_EQ(4.0f, metadata.max_page_scale_factor);
    }

    // Scrolling should update metadata immediately.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    {
        CompositorFrameMetadata metadata = m_hostImpl->makeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 10.0f), metadata.root_scroll_offset);
    }
    m_hostImpl->scrollEnd();

    {
        CompositorFrameMetadata metadata = m_hostImpl->makeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 10.0f), metadata.root_scroll_offset);
    }

    // Page scale should update metadata correctly (shrinking only the viewport).
    m_hostImpl->pinchGestureBegin();
    m_hostImpl->pinchGestureUpdate(2.0f, gfx::Point(0, 0));
    m_hostImpl->pinchGestureEnd();

    {
        CompositorFrameMetadata metadata = m_hostImpl->makeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 10.0f), metadata.root_scroll_offset);
        EXPECT_EQ(2, metadata.page_scale_factor);
        EXPECT_EQ(gfx::SizeF(25.0f, 25.0f), metadata.viewport_size);
        EXPECT_EQ(gfx::SizeF(100.0f, 100.0f), metadata.root_layer_size);
        EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
        EXPECT_EQ(4.0f, metadata.max_page_scale_factor);
    }

    // Likewise if set from the main thread.
    m_hostImpl->processScrollDeltas();
    m_hostImpl->setPageScaleFactorAndLimits(4.0f, 0.5f, 4.0f);
    {
        CompositorFrameMetadata metadata = m_hostImpl->makeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 10.0f), metadata.root_scroll_offset);
        EXPECT_EQ(4.0f, metadata.page_scale_factor);
        EXPECT_EQ(gfx::SizeF(12.5f, 12.5f), metadata.viewport_size);
        EXPECT_EQ(gfx::SizeF(100.0f, 100.0f), metadata.root_layer_size);
        EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
        EXPECT_EQ(4.0f, metadata.max_page_scale_factor);
    }
}

class DidDrawCheckLayer : public TiledLayerImpl {
public:
    static scoped_ptr<LayerImpl> create(LayerTreeImpl* treeImpl, int id) { return scoped_ptr<LayerImpl>(new DidDrawCheckLayer(treeImpl, id)); }

    virtual void didDraw(ResourceProvider*) OVERRIDE
    {
        m_didDrawCalled = true;
    }

    virtual void willDraw(ResourceProvider*) OVERRIDE
    {
        m_willDrawCalled = true;
    }

    bool didDrawCalled() const { return m_didDrawCalled; }
    bool willDrawCalled() const { return m_willDrawCalled; }

    void clearDidDrawCheck()
    {
        m_didDrawCalled = false;
        m_willDrawCalled = false;
    }

protected:
    DidDrawCheckLayer(LayerTreeImpl* treeImpl, int id)
        : TiledLayerImpl(treeImpl, id)
        , m_didDrawCalled(false)
        , m_willDrawCalled(false)
    {
        setAnchorPoint(gfx::PointF(0, 0));
        setBounds(gfx::Size(10, 10));
        setContentBounds(gfx::Size(10, 10));
        setDrawsContent(true);
        setSkipsDraw(false);
        drawProperties().visible_content_rect = gfx::Rect(0, 0, 10, 10);

        scoped_ptr<LayerTilingData> tiler = LayerTilingData::create(gfx::Size(100, 100), LayerTilingData::HasBorderTexels);
        tiler->setBounds(contentBounds());
        setTilingData(*tiler.get());
    }

private:
    bool m_didDrawCalled;
    bool m_willDrawCalled;
};

TEST_P(LayerTreeHostImplTest, didDrawNotCalledOnHiddenLayer)
{
    // The root layer is always drawn, so run this test on a child layer that
    // will be masked out by the root layer's bounds.
    m_hostImpl->activeTree()->SetRootLayer(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->rootLayer());
    root->setMasksToBounds(true);

    root->addChild(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 2));
    DidDrawCheckLayer* layer = static_cast<DidDrawCheckLayer*>(root->children()[0]);
    // Ensure visibleContentRect for layer is empty
    layer->setPosition(gfx::PointF(100, 100));
    layer->setBounds(gfx::Size(10, 10));
    layer->setContentBounds(gfx::Size(10, 10));

    LayerTreeHostImpl::FrameData frame;

    EXPECT_FALSE(layer->willDrawCalled());
    EXPECT_FALSE(layer->didDrawCalled());

    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    EXPECT_FALSE(layer->willDrawCalled());
    EXPECT_FALSE(layer->didDrawCalled());

    EXPECT_TRUE(layer->visibleContentRect().IsEmpty());

    // Ensure visibleContentRect for layer layer is not empty
    layer->setPosition(gfx::PointF(0, 0));

    EXPECT_FALSE(layer->willDrawCalled());
    EXPECT_FALSE(layer->didDrawCalled());

    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    EXPECT_TRUE(layer->willDrawCalled());
    EXPECT_TRUE(layer->didDrawCalled());

    EXPECT_FALSE(layer->visibleContentRect().IsEmpty());
}

TEST_P(LayerTreeHostImplTest, willDrawNotCalledOnOccludedLayer)
{
    gfx::Size bigSize(1000, 1000);
    m_hostImpl->setViewportSize(bigSize, bigSize);

    m_hostImpl->activeTree()->SetRootLayer(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->rootLayer());

    root->addChild(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 2));
    DidDrawCheckLayer* occludedLayer = static_cast<DidDrawCheckLayer*>(root->children()[0]);

    root->addChild(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 3));
    DidDrawCheckLayer* topLayer = static_cast<DidDrawCheckLayer*>(root->children()[1]);
    // This layer covers the occludedLayer above. Make this layer large so it can occlude.
    topLayer->setBounds(bigSize);
    topLayer->setContentBounds(bigSize);
    topLayer->setContentsOpaque(true);

    LayerTreeHostImpl::FrameData frame;

    EXPECT_FALSE(occludedLayer->willDrawCalled());
    EXPECT_FALSE(occludedLayer->didDrawCalled());
    EXPECT_FALSE(topLayer->willDrawCalled());
    EXPECT_FALSE(topLayer->didDrawCalled());

    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    EXPECT_FALSE(occludedLayer->willDrawCalled());
    EXPECT_FALSE(occludedLayer->didDrawCalled());
    EXPECT_TRUE(topLayer->willDrawCalled());
    EXPECT_TRUE(topLayer->didDrawCalled());
}

TEST_P(LayerTreeHostImplTest, didDrawCalledOnAllLayers)
{
    m_hostImpl->activeTree()->SetRootLayer(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->rootLayer());

    root->addChild(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 2));
    DidDrawCheckLayer* layer1 = static_cast<DidDrawCheckLayer*>(root->children()[0]);

    layer1->addChild(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 3));
    DidDrawCheckLayer* layer2 = static_cast<DidDrawCheckLayer*>(layer1->children()[0]);

    layer1->setOpacity(0.3f);
    layer1->setPreserves3D(false);

    EXPECT_FALSE(root->didDrawCalled());
    EXPECT_FALSE(layer1->didDrawCalled());
    EXPECT_FALSE(layer2->didDrawCalled());

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    EXPECT_TRUE(root->didDrawCalled());
    EXPECT_TRUE(layer1->didDrawCalled());
    EXPECT_TRUE(layer2->didDrawCalled());

    EXPECT_NE(root->renderSurface(), layer1->renderSurface());
    EXPECT_TRUE(!!layer1->renderSurface());
}

class MissingTextureAnimatingLayer : public DidDrawCheckLayer {
public:
    static scoped_ptr<LayerImpl> create(LayerTreeImpl* treeImpl, int id, bool tileMissing, bool skipsDraw, bool animating, ResourceProvider* resourceProvider)
    {
        return scoped_ptr<LayerImpl>(new MissingTextureAnimatingLayer(treeImpl, id, tileMissing, skipsDraw, animating, resourceProvider));
    }

private:
    MissingTextureAnimatingLayer(LayerTreeImpl* treeImpl, int id, bool tileMissing, bool skipsDraw, bool animating, ResourceProvider* resourceProvider)
        : DidDrawCheckLayer(treeImpl, id)
    {
        scoped_ptr<LayerTilingData> tilingData = LayerTilingData::create(gfx::Size(10, 10), LayerTilingData::NoBorderTexels);
        tilingData->setBounds(bounds());
        setTilingData(*tilingData.get());
        setSkipsDraw(skipsDraw);
        if (!tileMissing) {
            ResourceProvider::ResourceId resource = resourceProvider->createResource(gfx::Size(), GL_RGBA, ResourceProvider::TextureUsageAny);
            resourceProvider->allocateForTesting(resource);
            pushTileProperties(0, 0, resource, gfx::Rect(), false);
        }
        if (animating)
            addAnimatedTransformToLayer(*this, 10, 3, 0);
    }
};

TEST_P(LayerTreeHostImplTest, prepareToDrawFailsWhenAnimationUsesCheckerboard)
{
    // When the texture is not missing, we draw as usual.
    m_hostImpl->activeTree()->SetRootLayer(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->rootLayer());
    root->addChild(MissingTextureAnimatingLayer::create(m_hostImpl->activeTree(), 2, false, false, true, m_hostImpl->resourceProvider()));

    LayerTreeHostImpl::FrameData frame;

    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // When a texture is missing and we're not animating, we draw as usual with checkerboarding.
    m_hostImpl->activeTree()->SetRootLayer(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 3));
    root = static_cast<DidDrawCheckLayer*>(m_hostImpl->rootLayer());
    root->addChild(MissingTextureAnimatingLayer::create(m_hostImpl->activeTree(), 4, true, false, false, m_hostImpl->resourceProvider()));

    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // When a texture is missing and we're animating, we don't want to draw anything.
    m_hostImpl->activeTree()->SetRootLayer(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 5));
    root = static_cast<DidDrawCheckLayer*>(m_hostImpl->rootLayer());
    root->addChild(MissingTextureAnimatingLayer::create(m_hostImpl->activeTree(), 6, true, false, true, m_hostImpl->resourceProvider()));

    EXPECT_FALSE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // When the layer skips draw and we're animating, we still draw the frame.
    m_hostImpl->activeTree()->SetRootLayer(DidDrawCheckLayer::create(m_hostImpl->activeTree(), 7));
    root = static_cast<DidDrawCheckLayer*>(m_hostImpl->rootLayer());
    root->addChild(MissingTextureAnimatingLayer::create(m_hostImpl->activeTree(), 8, false, true, true, m_hostImpl->resourceProvider()));

    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);
}

TEST_P(LayerTreeHostImplTest, scrollRootIgnored)
{
    scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
    root->setScrollable(false);
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    initializeRendererAndDrawFrame();

    // Scroll event is ignored because layer is not scrollable.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
    EXPECT_FALSE(m_didRequestRedraw);
    EXPECT_FALSE(m_didRequestCommit);
}

TEST_P(LayerTreeHostImplTest, scrollNonCompositedRoot)
{
    // Test the configuration where a non-composited root layer is embedded in a
    // scrollable outer layer.
    gfx::Size surfaceSize(10, 10);

    scoped_ptr<LayerImpl> contentLayer = LayerImpl::create(m_hostImpl->activeTree(), 1);
    contentLayer->setDrawsContent(true);
    contentLayer->setPosition(gfx::PointF(0, 0));
    contentLayer->setAnchorPoint(gfx::PointF(0, 0));
    contentLayer->setBounds(surfaceSize);
    contentLayer->setContentBounds(gfx::Size(surfaceSize.width() * 2, surfaceSize.height() * 2));
    contentLayer->setContentsScale(2, 2);

    scoped_ptr<LayerImpl> scrollLayer = LayerImpl::create(m_hostImpl->activeTree(), 2);
    scrollLayer->setScrollable(true);
    scrollLayer->setMaxScrollOffset(gfx::Vector2d(surfaceSize.width(), surfaceSize.height()));
    scrollLayer->setBounds(surfaceSize);
    scrollLayer->setContentBounds(surfaceSize);
    scrollLayer->setPosition(gfx::PointF(0, 0));
    scrollLayer->setAnchorPoint(gfx::PointF(0, 0));
    scrollLayer->addChild(contentLayer.Pass());

    m_hostImpl->activeTree()->SetRootLayer(scrollLayer.Pass());
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->scrollEnd();
    EXPECT_TRUE(m_didRequestRedraw);
    EXPECT_TRUE(m_didRequestCommit);
}

TEST_P(LayerTreeHostImplTest, scrollChildCallsCommitAndRedraw)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
    root->setBounds(surfaceSize);
    root->setContentBounds(surfaceSize);
    root->addChild(createScrollableLayer(2, surfaceSize));
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->scrollEnd();
    EXPECT_TRUE(m_didRequestRedraw);
    EXPECT_TRUE(m_didRequestCommit);
}

TEST_P(LayerTreeHostImplTest, scrollMissesChild)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
    root->addChild(createScrollableLayer(2, surfaceSize));
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scroll event is ignored because the input coordinate is outside the layer boundaries.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(15, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
    EXPECT_FALSE(m_didRequestRedraw);
    EXPECT_FALSE(m_didRequestCommit);
}

TEST_P(LayerTreeHostImplTest, scrollMissesBackfacingChild)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
    scoped_ptr<LayerImpl> child = createScrollableLayer(2, surfaceSize);
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);

    gfx::Transform matrix;
    matrix.RotateAboutXAxis(180);
    child->setTransform(matrix);
    child->setDoubleSided(false);

    root->addChild(child.Pass());
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    initializeRendererAndDrawFrame();

    // Scroll event is ignored because the scrollable layer is not facing the viewer and there is
    // nothing scrollable behind it.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
    EXPECT_FALSE(m_didRequestRedraw);
    EXPECT_FALSE(m_didRequestCommit);
}

TEST_P(LayerTreeHostImplTest, scrollBlockedByContentLayer)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> contentLayer = createScrollableLayer(1, surfaceSize);
    contentLayer->setShouldScrollOnMainThread(true);
    contentLayer->setScrollable(false);

    scoped_ptr<LayerImpl> scrollLayer = createScrollableLayer(2, surfaceSize);
    scrollLayer->addChild(contentLayer.Pass());

    m_hostImpl->activeTree()->SetRootLayer(scrollLayer.Pass());
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scrolling fails because the content layer is asking to be scrolled on the main thread.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
}

TEST_P(LayerTreeHostImplTest, scrollRootAndChangePageScaleOnMainThread)
{
    gfx::Size surfaceSize(10, 10);
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    gfx::Vector2d scrollDelta(0, 10);
    gfx::Vector2d expectedScrollDelta(scrollDelta);
    gfx::Vector2d expectedMaxScroll(m_hostImpl->rootLayer()->maxScrollOffset());
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();

    // Set new page scale from main thread.
    m_hostImpl->setPageScaleFactorAndLimits(pageScale, pageScale, pageScale);

    if (!m_hostImpl->settings().pageScalePinchZoomEnabled) {
        // The scale should apply to the scroll delta.
        expectedScrollDelta = gfx::ToFlooredVector2d(gfx::ScaleVector2d(expectedScrollDelta, pageScale));
    }
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);

    // The scroll range should also have been updated.
    EXPECT_EQ(m_hostImpl->rootLayer()->maxScrollOffset(), expectedMaxScroll);

    // The page scale delta remains constant because the impl thread did not scale.
    // TODO: If possible, use gfx::Transform() or Skia equality functions. At
    //       the moment we avoid that because skia does exact bit-wise equality
    //       checking that does not consider -0 == +0.
    //       http://code.google.com/p/chromium/issues/detail?id=162747
    EXPECT_EQ(1.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(0, 0));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(0, 1));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(0, 2));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(0, 3));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(1, 0));
    EXPECT_EQ(1.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(1, 1));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(1, 2));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(1, 3));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(2, 0));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(2, 1));
    EXPECT_EQ(1.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(2, 2));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(2, 3));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(3, 0));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(3, 1));
    EXPECT_EQ(0.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(3, 2));
    EXPECT_EQ(1.0, m_hostImpl->rootLayer()->implTransform().matrix().getDouble(3, 3));
}

TEST_P(LayerTreeHostImplTest, scrollRootAndChangePageScaleOnImplThread)
{
    gfx::Size surfaceSize(10, 10);
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    m_hostImpl->setPageScaleFactorAndLimits(1, 1, pageScale);
    initializeRendererAndDrawFrame();

    gfx::Vector2d scrollDelta(0, 10);
    gfx::Vector2d expectedScrollDelta(scrollDelta);
    gfx::Vector2d expectedMaxScroll(m_hostImpl->rootLayer()->maxScrollOffset());
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();

    // Set new page scale on impl thread by pinching.
    m_hostImpl->pinchGestureBegin();
    m_hostImpl->pinchGestureUpdate(pageScale, gfx::Point());
    m_hostImpl->pinchGestureEnd();
    drawOneFrame();

    // The scroll delta is not scaled because the main thread did not scale.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);

    // The scroll range should also have been updated.
    EXPECT_EQ(m_hostImpl->rootLayer()->maxScrollOffset(), expectedMaxScroll);

    // The page scale delta should match the new scale on the impl side.
    gfx::Transform expectedScale;
    expectedScale.Scale(pageScale, pageScale);
    EXPECT_EQ(m_hostImpl->rootLayer()->implTransform(), expectedScale);
}

TEST_P(LayerTreeHostImplTest, pageScaleDeltaAppliedToRootScrollLayerOnly)
{
    gfx::Size surfaceSize(10, 10);
    float defaultPageScale = 1;
    gfx::Transform defaultPageScaleMatrix;

    float newPageScale = 2;
    gfx::Transform newPageScaleMatrix;
    newPageScaleMatrix.Scale(newPageScale, newPageScale);

    // Create a normal scrollable root layer and another scrollable child layer.
    setupScrollAndContentsLayers(surfaceSize);
    LayerImpl* root = m_hostImpl->rootLayer();
    LayerImpl* child = root->children()[0];

    scoped_ptr<LayerImpl> scrollableChild = createScrollableLayer(3, surfaceSize);
    child->addChild(scrollableChild.Pass());
    LayerImpl* grandChild = child->children()[0];

    // Set new page scale on impl thread by pinching.
    m_hostImpl->pinchGestureBegin();
    m_hostImpl->pinchGestureUpdate(newPageScale, gfx::Point());
    m_hostImpl->pinchGestureEnd();
    drawOneFrame();

    // The page scale delta should only be applied to the scrollable root layer.
    EXPECT_EQ(root->implTransform(), newPageScaleMatrix);
    EXPECT_EQ(child->implTransform(), defaultPageScaleMatrix);
    EXPECT_EQ(grandChild->implTransform(), defaultPageScaleMatrix);

    // Make sure all the layers are drawn with the page scale delta applied, i.e., the page scale
    // delta on the root layer is applied hierarchically.
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    EXPECT_EQ(root->drawTransform().matrix().getDouble(0, 0), newPageScale);
    EXPECT_EQ(root->drawTransform().matrix().getDouble(1, 1), newPageScale);
    EXPECT_EQ(child->drawTransform().matrix().getDouble(0, 0), newPageScale);
    EXPECT_EQ(child->drawTransform().matrix().getDouble(1, 1), newPageScale);
    EXPECT_EQ(grandChild->drawTransform().matrix().getDouble(0, 0), newPageScale);
    EXPECT_EQ(grandChild->drawTransform().matrix().getDouble(1, 1), newPageScale);
}

TEST_P(LayerTreeHostImplTest, scrollChildAndChangePageScaleOnMainThread)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
    root->setBounds(surfaceSize);
    root->setContentBounds(surfaceSize);
    // Also mark the root scrollable so it becomes the root scroll layer.
    root->setScrollable(true);
    int scrollLayerId = 2;
    root->addChild(createScrollableLayer(scrollLayerId, surfaceSize));
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    LayerImpl* child = m_hostImpl->rootLayer()->children()[0];

    gfx::Vector2d scrollDelta(0, 10);
    gfx::Vector2d expectedScrollDelta(scrollDelta);
    gfx::Vector2d expectedMaxScroll(child->maxScrollOffset());
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();

    float pageScale = 2;
    m_hostImpl->setPageScaleFactorAndLimits(pageScale, 1, pageScale);

    drawOneFrame();

    if (!m_hostImpl->settings().pageScalePinchZoomEnabled) {
        // The scale should apply to the scroll delta.
        expectedScrollDelta = gfx::ToFlooredVector2d(gfx::ScaleVector2d(expectedScrollDelta, pageScale));
    }
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), scrollLayerId, expectedScrollDelta);

    // The scroll range should not have changed.
    EXPECT_EQ(child->maxScrollOffset(), expectedMaxScroll);

    // The page scale delta remains constant because the impl thread did not scale.
    gfx::Transform identityTransform;
    EXPECT_EQ(child->implTransform(), gfx::Transform());
}

TEST_P(LayerTreeHostImplTest, scrollChildBeyondLimit)
{
    // Scroll a child layer beyond its maximum scroll range and make sure the
    // parent layer is scrolled on the axis on which the child was unable to
    // scroll.
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);

    scoped_ptr<LayerImpl> grandChild = createScrollableLayer(3, surfaceSize);
    grandChild->setScrollOffset(gfx::Vector2d(0, 5));

    scoped_ptr<LayerImpl> child = createScrollableLayer(2, surfaceSize);
    child->setScrollOffset(gfx::Vector2d(3, 0));
    child->addChild(grandChild.Pass());

    root->addChild(child.Pass());
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();
    {
        gfx::Vector2d scrollDelta(-8, -7);
        EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
        m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->scrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();

        // The grand child should have scrolled up to its limit.
        LayerImpl* child = m_hostImpl->rootLayer()->children()[0];
        LayerImpl* grandChild = child->children()[0];
        expectContains(*scrollInfo.get(), grandChild->id(), gfx::Vector2d(0, -5));

        // The child should have only scrolled on the other axis.
        expectContains(*scrollInfo.get(), child->id(), gfx::Vector2d(-3, 0));
    }
}

TEST_P(LayerTreeHostImplTest, scrollEventBubbling)
{
    // When we try to scroll a non-scrollable child layer, the scroll delta
    // should be applied to one of its ancestors if possible.
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);
    scoped_ptr<LayerImpl> child = createScrollableLayer(2, surfaceSize);

    child->setScrollable(false);
    root->addChild(child.Pass());

    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();
    {
        gfx::Vector2d scrollDelta(0, 4);
        EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
        m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->scrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();

        // Only the root should have scrolled.
        ASSERT_EQ(scrollInfo->scrolls.size(), 1u);
        expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), scrollDelta);
    }
}

TEST_P(LayerTreeHostImplTest, scrollBeforeRedraw)
{
    gfx::Size surfaceSize(10, 10);
    m_hostImpl->activeTree()->SetRootLayer(createScrollableLayer(1, surfaceSize));
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);

    // Draw one frame and then immediately rebuild the layer tree to mimic a tree synchronization.
    initializeRendererAndDrawFrame();
    m_hostImpl->activeTree()->DetachLayerTree();
    m_hostImpl->activeTree()->SetRootLayer(createScrollableLayer(2, surfaceSize));
    m_hostImpl->activeTree()->FindRootScrollLayer();

    // Scrolling should still work even though we did not draw yet.
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
}

TEST_P(LayerTreeHostImplTest, scrollAxisAlignedRotatedLayer)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // Rotate the root layer 90 degrees counter-clockwise about its center.
    gfx::Transform rotateTransform;
    rotateTransform.Rotate(-90);
    m_hostImpl->rootLayer()->setTransform(rotateTransform);

    gfx::Size surfaceSize(50, 50);
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scroll to the right in screen coordinates with a gesture.
    gfx::Vector2d gestureScrollDelta(10, 0);
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), gestureScrollDelta);
    m_hostImpl->scrollEnd();

    // The layer should have scrolled down in its local coordinates.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), gfx::Vector2d(0, gestureScrollDelta.x()));

    // Reset and scroll down with the wheel.
    m_hostImpl->rootLayer()->setScrollDelta(gfx::Vector2dF());
    gfx::Vector2d wheelScrollDelta(0, 10);
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), wheelScrollDelta);
    m_hostImpl->scrollEnd();

    // The layer should have scrolled down in its local coordinates.
    scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), wheelScrollDelta);
}

TEST_P(LayerTreeHostImplTest, scrollNonAxisAlignedRotatedLayer)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    int childLayerId = 3;
    float childLayerAngle = -20;

    // Create a child layer that is rotated to a non-axis-aligned angle.
    scoped_ptr<LayerImpl> child = createScrollableLayer(childLayerId, m_hostImpl->rootLayer()->contentBounds());
    gfx::Transform rotateTransform;
    rotateTransform.Translate(-50, -50);
    rotateTransform.Rotate(childLayerAngle);
    rotateTransform.Translate(50, 50);
    child->setTransform(rotateTransform);

    // Only allow vertical scrolling.
    child->setMaxScrollOffset(gfx::Vector2d(0, child->contentBounds().height()));
    m_hostImpl->rootLayer()->addChild(child.Pass());

    gfx::Size surfaceSize(50, 50);
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    {
        // Scroll down in screen coordinates with a gesture.
        gfx::Vector2d gestureScrollDelta(0, 10);
        EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
        m_hostImpl->scrollBy(gfx::Point(), gestureScrollDelta);
        m_hostImpl->scrollEnd();

        // The child layer should have scrolled down in its local coordinates an amount proportional to
        // the angle between it and the input scroll delta.
        gfx::Vector2d expectedScrollDelta(0, gestureScrollDelta.y() * std::cos(MathUtil::Deg2Rad(childLayerAngle)));
        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        expectContains(*scrollInfo.get(), childLayerId, expectedScrollDelta);

        // The root layer should not have scrolled, because the input delta was close to the layer's
        // axis of movement.
        EXPECT_EQ(scrollInfo->scrolls.size(), 1u);
    }

    {
        // Now reset and scroll the same amount horizontally.
        m_hostImpl->rootLayer()->children()[1]->setScrollDelta(gfx::Vector2dF());
        gfx::Vector2d gestureScrollDelta(10, 0);
        EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
        m_hostImpl->scrollBy(gfx::Point(), gestureScrollDelta);
        m_hostImpl->scrollEnd();

        // The child layer should have scrolled down in its local coordinates an amount proportional to
        // the angle between it and the input scroll delta.
        gfx::Vector2d expectedScrollDelta(0, -gestureScrollDelta.x() * std::sin(MathUtil::Deg2Rad(childLayerAngle)));
        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
        expectContains(*scrollInfo.get(), childLayerId, expectedScrollDelta);

        // The root layer should have scrolled more, since the input scroll delta was mostly
        // orthogonal to the child layer's vertical scroll axis.
        gfx::Vector2d expectedRootScrollDelta(gestureScrollDelta.x() * std::pow(std::cos(MathUtil::Deg2Rad(childLayerAngle)), 2), 0);
        expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedRootScrollDelta);
    }
}

TEST_P(LayerTreeHostImplTest, scrollScaledLayer)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // Scale the layer to twice its normal size.
    int scale = 2;
    gfx::Transform scaleTransform;
    scaleTransform.Scale(scale, scale);
    m_hostImpl->rootLayer()->setTransform(scaleTransform);

    gfx::Size surfaceSize(50, 50);
    m_hostImpl->setViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scroll down in screen coordinates with a gesture.
    gfx::Vector2d scrollDelta(0, 10);
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();

    // The layer should have scrolled down in its local coordinates, but half he amount.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), gfx::Vector2d(0, scrollDelta.y() / scale));

    // Reset and scroll down with the wheel.
    m_hostImpl->rootLayer()->setScrollDelta(gfx::Vector2dF());
    gfx::Vector2d wheelScrollDelta(0, 10);
    EXPECT_EQ(m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->scrollBy(gfx::Point(), wheelScrollDelta);
    m_hostImpl->scrollEnd();

    // The scale should not have been applied to the scroll delta.
    scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), wheelScrollDelta);
}

class BlendStateTrackerContext: public FakeWebGraphicsContext3D {
public:
    BlendStateTrackerContext() : m_blend(false) { }

    virtual void enable(WebKit::WGC3Denum cap)
    {
        if (cap == GL_BLEND)
            m_blend = true;
    }

    virtual void disable(WebKit::WGC3Denum cap)
    {
        if (cap == GL_BLEND)
            m_blend = false;
    }

    bool blend() const { return m_blend; }

private:
    bool m_blend;
};

class BlendStateCheckLayer : public LayerImpl {
public:
    static scoped_ptr<LayerImpl> create(LayerTreeImpl* treeImpl, int id, ResourceProvider* resourceProvider) { return scoped_ptr<LayerImpl>(new BlendStateCheckLayer(treeImpl, id, resourceProvider)); }

    virtual void appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData) OVERRIDE
    {
        m_quadsAppended = true;

        gfx::Rect opaqueRect;
        if (contentsOpaque())
            opaqueRect = m_quadRect;
        else
            opaqueRect = m_opaqueContentRect;

        SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
        scoped_ptr<TileDrawQuad> testBlendingDrawQuad = TileDrawQuad::Create();
        testBlendingDrawQuad->SetNew(sharedQuadState, m_quadRect, opaqueRect, m_resourceId, gfx::RectF(0, 0, 1, 1), gfx::Size(1, 1), false, false, false, false, false);
        testBlendingDrawQuad->visible_rect = m_quadVisibleRect;
        EXPECT_EQ(m_blend, testBlendingDrawQuad->ShouldDrawWithBlending());
        EXPECT_EQ(m_hasRenderSurface, !!renderSurface());
        quadSink.append(testBlendingDrawQuad.PassAs<DrawQuad>(), appendQuadsData);
    }

    void setExpectation(bool blend, bool hasRenderSurface)
    {
        m_blend = blend;
        m_hasRenderSurface = hasRenderSurface;
        m_quadsAppended = false;
    }

    bool quadsAppended() const { return m_quadsAppended; }

    void setQuadRect(const gfx::Rect& rect) { m_quadRect = rect; }
    void setQuadVisibleRect(const gfx::Rect& rect) { m_quadVisibleRect = rect; }
    void setOpaqueContentRect(const gfx::Rect& rect) { m_opaqueContentRect = rect; }

private:
    BlendStateCheckLayer(LayerTreeImpl* treeImpl, int id, ResourceProvider* resourceProvider)
        : LayerImpl(treeImpl, id)
        , m_blend(false)
        , m_hasRenderSurface(false)
        , m_quadsAppended(false)
        , m_quadRect(5, 5, 5, 5)
        , m_quadVisibleRect(5, 5, 5, 5)
        , m_resourceId(resourceProvider->createResource(gfx::Size(1, 1), GL_RGBA, ResourceProvider::TextureUsageAny))
    {
        resourceProvider->allocateForTesting(m_resourceId);
        setAnchorPoint(gfx::PointF(0, 0));
        setBounds(gfx::Size(10, 10));
        setContentBounds(gfx::Size(10, 10));
        setDrawsContent(true);
    }

    bool m_blend;
    bool m_hasRenderSurface;
    bool m_quadsAppended;
    gfx::Rect m_quadRect;
    gfx::Rect m_opaqueContentRect;
    gfx::Rect m_quadVisibleRect;
    ResourceProvider::ResourceId m_resourceId;
};

TEST_P(LayerTreeHostImplTest, blendingOffWhenDrawingOpaqueLayers)
{
    {
        scoped_ptr<LayerImpl> root = LayerImpl::create(m_hostImpl->activeTree(), 1);
        root->setAnchorPoint(gfx::PointF(0, 0));
        root->setBounds(gfx::Size(10, 10));
        root->setContentBounds(root->bounds());
        root->setDrawsContent(false);
        m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    }
    LayerImpl* root = m_hostImpl->rootLayer();

    root->addChild(BlendStateCheckLayer::create(m_hostImpl->activeTree(), 2, m_hostImpl->resourceProvider()));
    BlendStateCheckLayer* layer1 = static_cast<BlendStateCheckLayer*>(root->children()[0]);
    layer1->setPosition(gfx::PointF(2, 2));

    LayerTreeHostImpl::FrameData frame;

    // Opaque layer, drawn without blending.
    layer1->setContentsOpaque(true);
    layer1->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Layer with translucent content and painting, so drawn with blending.
    layer1->setContentsOpaque(false);
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Layer with translucent opacity, drawn with blending.
    layer1->setContentsOpaque(true);
    layer1->setOpacity(0.5);
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Layer with translucent opacity and painting, drawn with blending.
    layer1->setContentsOpaque(true);
    layer1->setOpacity(0.5);
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    layer1->addChild(BlendStateCheckLayer::create(m_hostImpl->activeTree(), 3, m_hostImpl->resourceProvider()));
    BlendStateCheckLayer* layer2 = static_cast<BlendStateCheckLayer*>(layer1->children()[0]);
    layer2->setPosition(gfx::PointF(4, 4));

    // 2 opaque layers, drawn without blending.
    layer1->setContentsOpaque(true);
    layer1->setOpacity(1);
    layer1->setExpectation(false, false);
    layer2->setContentsOpaque(true);
    layer2->setOpacity(1);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Parent layer with translucent content, drawn with blending.
    // Child layer with opaque content, drawn without blending.
    layer1->setContentsOpaque(false);
    layer1->setExpectation(true, false);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Parent layer with translucent content but opaque painting, drawn without blending.
    // Child layer with opaque content, drawn without blending.
    layer1->setContentsOpaque(true);
    layer1->setExpectation(false, false);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Parent layer with translucent opacity and opaque content. Since it has a
    // drawing child, it's drawn to a render surface which carries the opacity,
    // so it's itself drawn without blending.
    // Child layer with opaque content, drawn without blending (parent surface
    // carries the inherited opacity).
    layer1->setContentsOpaque(true);
    layer1->setOpacity(0.5);
    layer1->setExpectation(false, true);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Draw again, but with child non-opaque, to make sure
    // layer1 not culled.
    layer1->setContentsOpaque(true);
    layer1->setOpacity(1);
    layer1->setExpectation(false, false);
    layer2->setContentsOpaque(true);
    layer2->setOpacity(0.5);
    layer2->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // A second way of making the child non-opaque.
    layer1->setContentsOpaque(true);
    layer1->setOpacity(1);
    layer1->setExpectation(false, false);
    layer2->setContentsOpaque(false);
    layer2->setOpacity(1);
    layer2->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // And when the layer says its not opaque but is painted opaque, it is not blended.
    layer1->setContentsOpaque(true);
    layer1->setOpacity(1);
    layer1->setExpectation(false, false);
    layer2->setContentsOpaque(true);
    layer2->setOpacity(1);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Layer with partially opaque contents, drawn with blending.
    layer1->setContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(5, 5, 5, 5));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Layer with partially opaque contents partially culled, drawn with blending.
    layer1->setContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(5, 5, 5, 2));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Layer with partially opaque contents culled, drawn with blending.
    layer1->setContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(7, 5, 3, 5));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

    // Layer with partially opaque contents and translucent contents culled, drawn without blending.
    layer1->setContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(5, 5, 2, 5));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->didDrawAllLayers(frame);

}

TEST_P(LayerTreeHostImplTest, viewportCovered)
{
    m_hostImpl->initializeRenderer(createOutputSurface());
    m_hostImpl->activeTree()->set_background_color(SK_ColorGRAY);

    gfx::Size viewportSize(1000, 1000);
    m_hostImpl->setViewportSize(viewportSize, viewportSize);

    m_hostImpl->activeTree()->SetRootLayer(LayerImpl::create(m_hostImpl->activeTree(), 1));
    m_hostImpl->rootLayer()->addChild(BlendStateCheckLayer::create(m_hostImpl->activeTree(), 2, m_hostImpl->resourceProvider()));
    BlendStateCheckLayer* child = static_cast<BlendStateCheckLayer*>(m_hostImpl->rootLayer()->children()[0]);
    child->setExpectation(false, false);
    child->setContentsOpaque(true);

    // No gutter rects
    {
        gfx::Rect layerRect(0, 0, 1000, 1000);
        child->setPosition(layerRect.origin());
        child->setBounds(layerRect.size());
        child->setContentBounds(layerRect.size());
        child->setQuadRect(gfx::Rect(gfx::Point(), layerRect.size()));
        child->setQuadVisibleRect(gfx::Rect(gfx::Point(), layerRect.size()));

        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        ASSERT_EQ(1u, frame.renderPasses.size());

        size_t numGutterQuads = 0;
        for (size_t i = 0; i < frame.renderPasses[0]->quad_list.size(); ++i)
            numGutterQuads += (frame.renderPasses[0]->quad_list[i]->material == DrawQuad::SOLID_COLOR) ? 1 : 0;
        EXPECT_EQ(0u, numGutterQuads);
        EXPECT_EQ(1u, frame.renderPasses[0]->quad_list.size());

        LayerTestCommon::verifyQuadsExactlyCoverRect(frame.renderPasses[0]->quad_list, gfx::Rect(gfx::Point(), viewportSize));
        m_hostImpl->didDrawAllLayers(frame);
    }

    // Empty visible content area (fullscreen gutter rect)
    {
        gfx::Rect layerRect(0, 0, 0, 0);
        child->setPosition(layerRect.origin());
        child->setBounds(layerRect.size());
        child->setContentBounds(layerRect.size());
        child->setQuadRect(gfx::Rect(gfx::Point(), layerRect.size()));
        child->setQuadVisibleRect(gfx::Rect(gfx::Point(), layerRect.size()));

        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        ASSERT_EQ(1u, frame.renderPasses.size());

        size_t numGutterQuads = 0;
        for (size_t i = 0; i < frame.renderPasses[0]->quad_list.size(); ++i)
            numGutterQuads += (frame.renderPasses[0]->quad_list[i]->material == DrawQuad::SOLID_COLOR) ? 1 : 0;
        EXPECT_EQ(1u, numGutterQuads);
        EXPECT_EQ(1u, frame.renderPasses[0]->quad_list.size());

        LayerTestCommon::verifyQuadsExactlyCoverRect(frame.renderPasses[0]->quad_list, gfx::Rect(gfx::Point(), viewportSize));
        m_hostImpl->didDrawAllLayers(frame);
    }

    // Content area in middle of clip rect (four surrounding gutter rects)
    {
        gfx::Rect layerRect(500, 500, 200, 200);
        child->setPosition(layerRect.origin());
        child->setBounds(layerRect.size());
        child->setContentBounds(layerRect.size());
        child->setQuadRect(gfx::Rect(gfx::Point(), layerRect.size()));
        child->setQuadVisibleRect(gfx::Rect(gfx::Point(), layerRect.size()));

        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        ASSERT_EQ(1u, frame.renderPasses.size());

        size_t numGutterQuads = 0;
        for (size_t i = 0; i < frame.renderPasses[0]->quad_list.size(); ++i)
            numGutterQuads += (frame.renderPasses[0]->quad_list[i]->material == DrawQuad::SOLID_COLOR) ? 1 : 0;
        EXPECT_EQ(4u, numGutterQuads);
        EXPECT_EQ(5u, frame.renderPasses[0]->quad_list.size());

        LayerTestCommon::verifyQuadsExactlyCoverRect(frame.renderPasses[0]->quad_list, gfx::Rect(gfx::Point(), viewportSize));
        m_hostImpl->didDrawAllLayers(frame);
    }

}


class ReshapeTrackerContext: public FakeWebGraphicsContext3D {
public:
    ReshapeTrackerContext() : m_reshapeCalled(false) { }

    virtual void reshape(int width, int height)
    {
        m_reshapeCalled = true;
    }

    bool reshapeCalled() const { return m_reshapeCalled; }

private:
    bool m_reshapeCalled;
};

class FakeDrawableLayerImpl: public LayerImpl {
public:
    static scoped_ptr<LayerImpl> create(LayerTreeImpl* treeImpl, int id) { return scoped_ptr<LayerImpl>(new FakeDrawableLayerImpl(treeImpl, id)); }
protected:
    FakeDrawableLayerImpl(LayerTreeImpl* treeImpl, int id) : LayerImpl(treeImpl, id) { }
};

// Only reshape when we know we are going to draw. Otherwise, the reshape
// can leave the window at the wrong size if we never draw and the proper
// viewport size is never set.
TEST_P(LayerTreeHostImplTest, reshapeNotCalledUntilDraw)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new ReshapeTrackerContext)).PassAs<OutputSurface>();
    ReshapeTrackerContext* reshapeTracker = static_cast<ReshapeTrackerContext*>(outputSurface->Context3D());
    m_hostImpl->initializeRenderer(outputSurface.Pass());

    scoped_ptr<LayerImpl> root = FakeDrawableLayerImpl::create(m_hostImpl->activeTree(), 1);
    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setBounds(gfx::Size(10, 10));
    root->setDrawsContent(true);
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    EXPECT_FALSE(reshapeTracker->reshapeCalled());

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    EXPECT_TRUE(reshapeTracker->reshapeCalled());
    m_hostImpl->didDrawAllLayers(frame);
}

class PartialSwapTrackerContext : public FakeWebGraphicsContext3D {
public:
    virtual void postSubBufferCHROMIUM(int x, int y, int width, int height)
    {
        m_partialSwapRect = gfx::Rect(x, y, width, height);
    }

    virtual WebKit::WebString getString(WebKit::WGC3Denum name)
    {
        if (name == GL_EXTENSIONS)
            return WebKit::WebString("GL_CHROMIUM_post_sub_buffer GL_CHROMIUM_set_visibility");

        return WebKit::WebString();
    }

    gfx::Rect partialSwapRect() const { return m_partialSwapRect; }

private:
    gfx::Rect m_partialSwapRect;
};

// Make sure damage tracking propagates all the way to the graphics context,
// where it should request to swap only the subBuffer that is damaged.
TEST_P(LayerTreeHostImplTest, partialSwapReceivesDamageRect)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapTrackerContext)).PassAs<OutputSurface>();
    PartialSwapTrackerContext* partialSwapTracker = static_cast<PartialSwapTrackerContext*>(outputSurface->Context3D());

    // This test creates its own LayerTreeHostImpl, so
    // that we can force partial swap enabled.
    LayerTreeSettings settings;
    settings.partialSwapEnabled = true;
    scoped_ptr<LayerTreeHostImpl> layerTreeHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);
    layerTreeHostImpl->initializeRenderer(outputSurface.Pass());
    layerTreeHostImpl->setViewportSize(gfx::Size(500, 500), gfx::Size(500, 500));

    scoped_ptr<LayerImpl> root = FakeDrawableLayerImpl::create(layerTreeHostImpl->activeTree(), 1);
    scoped_ptr<LayerImpl> child = FakeDrawableLayerImpl::create(layerTreeHostImpl->activeTree(), 2);
    child->setPosition(gfx::PointF(12, 13));
    child->setAnchorPoint(gfx::PointF(0, 0));
    child->setBounds(gfx::Size(14, 15));
    child->setContentBounds(gfx::Size(14, 15));
    child->setDrawsContent(true);
    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setBounds(gfx::Size(500, 500));
    root->setContentBounds(gfx::Size(500, 500));
    root->setDrawsContent(true);
    root->addChild(child.Pass());
    layerTreeHostImpl->activeTree()->SetRootLayer(root.Pass());

    LayerTreeHostImpl::FrameData frame;

    // First frame, the entire screen should get swapped.
    EXPECT_TRUE(layerTreeHostImpl->prepareToDraw(frame));
    layerTreeHostImpl->drawLayers(frame);
    layerTreeHostImpl->didDrawAllLayers(frame);
    layerTreeHostImpl->swapBuffers();
    gfx::Rect actualSwapRect = partialSwapTracker->partialSwapRect();
    gfx::Rect expectedSwapRect = gfx::Rect(gfx::Point(), gfx::Size(500, 500));
    EXPECT_EQ(expectedSwapRect.x(), actualSwapRect.x());
    EXPECT_EQ(expectedSwapRect.y(), actualSwapRect.y());
    EXPECT_EQ(expectedSwapRect.width(), actualSwapRect.width());
    EXPECT_EQ(expectedSwapRect.height(), actualSwapRect.height());

    // Second frame, only the damaged area should get swapped. Damage should be the union
    // of old and new child rects.
    // expected damage rect: gfx::Rect(gfx::Point(), gfx::Size(26, 28));
    // expected swap rect: vertically flipped, with origin at bottom left corner.
    layerTreeHostImpl->rootLayer()->children()[0]->setPosition(gfx::PointF(0, 0));
    EXPECT_TRUE(layerTreeHostImpl->prepareToDraw(frame));
    layerTreeHostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);
    layerTreeHostImpl->swapBuffers();
    actualSwapRect = partialSwapTracker->partialSwapRect();
    expectedSwapRect = gfx::Rect(gfx::Point(0, 500-28), gfx::Size(26, 28));
    EXPECT_EQ(expectedSwapRect.x(), actualSwapRect.x());
    EXPECT_EQ(expectedSwapRect.y(), actualSwapRect.y());
    EXPECT_EQ(expectedSwapRect.width(), actualSwapRect.width());
    EXPECT_EQ(expectedSwapRect.height(), actualSwapRect.height());

    // Make sure that partial swap is constrained to the viewport dimensions
    // expected damage rect: gfx::Rect(gfx::Point(), gfx::Size(500, 500));
    // expected swap rect: flipped damage rect, but also clamped to viewport
    layerTreeHostImpl->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    layerTreeHostImpl->rootLayer()->setOpacity(0.7f); // this will damage everything
    EXPECT_TRUE(layerTreeHostImpl->prepareToDraw(frame));
    layerTreeHostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);
    layerTreeHostImpl->swapBuffers();
    actualSwapRect = partialSwapTracker->partialSwapRect();
    expectedSwapRect = gfx::Rect(gfx::Point(), gfx::Size(10, 10));
    EXPECT_EQ(expectedSwapRect.x(), actualSwapRect.x());
    EXPECT_EQ(expectedSwapRect.y(), actualSwapRect.y());
    EXPECT_EQ(expectedSwapRect.width(), actualSwapRect.width());
    EXPECT_EQ(expectedSwapRect.height(), actualSwapRect.height());
}

TEST_P(LayerTreeHostImplTest, rootLayerDoesntCreateExtraSurface)
{
    scoped_ptr<LayerImpl> root = FakeDrawableLayerImpl::create(m_hostImpl->activeTree(), 1);
    scoped_ptr<LayerImpl> child = FakeDrawableLayerImpl::create(m_hostImpl->activeTree(), 2);
    child->setAnchorPoint(gfx::PointF(0, 0));
    child->setBounds(gfx::Size(10, 10));
    child->setContentBounds(gfx::Size(10, 10));
    child->setDrawsContent(true);
    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setBounds(gfx::Size(10, 10));
    root->setContentBounds(gfx::Size(10, 10));
    root->setDrawsContent(true);
    root->setOpacity(0.7f);
    root->addChild(child.Pass());

    m_hostImpl->activeTree()->SetRootLayer(root.Pass());

    LayerTreeHostImpl::FrameData frame;

    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    EXPECT_EQ(1u, frame.renderSurfaceLayerList->size());
    EXPECT_EQ(1u, frame.renderPasses.size());
    m_hostImpl->didDrawAllLayers(frame);
}

class FakeLayerWithQuads : public LayerImpl {
public:
    static scoped_ptr<LayerImpl> create(LayerTreeImpl* treeImpl, int id) { return scoped_ptr<LayerImpl>(new FakeLayerWithQuads(treeImpl, id)); }

    virtual void appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData) OVERRIDE
    {
        SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());

        SkColor gray = SkColorSetRGB(100, 100, 100);
        gfx::Rect quadRect(gfx::Point(0, 0), contentBounds());
        scoped_ptr<SolidColorDrawQuad> myQuad = SolidColorDrawQuad::Create();
        myQuad->SetNew(sharedQuadState, quadRect, gray);
        quadSink.append(myQuad.PassAs<DrawQuad>(), appendQuadsData);
    }

private:
    FakeLayerWithQuads(LayerTreeImpl* treeImpl, int id)
        : LayerImpl(treeImpl, id)
    {
    }
};

class MockContext : public FakeWebGraphicsContext3D {
public:
    MOCK_METHOD1(useProgram, void(WebKit::WebGLId program));
    MOCK_METHOD5(uniform4f, void(WebKit::WGC3Dint location, WebKit::WGC3Dfloat x, WebKit::WGC3Dfloat y, WebKit::WGC3Dfloat z, WebKit::WGC3Dfloat w));
    MOCK_METHOD4(uniformMatrix4fv, void(WebKit::WGC3Dint location, WebKit::WGC3Dsizei count, WebKit::WGC3Dboolean transpose, const WebKit::WGC3Dfloat* value));
    MOCK_METHOD4(drawElements, void(WebKit::WGC3Denum mode, WebKit::WGC3Dsizei count, WebKit::WGC3Denum type, WebKit::WGC3Dintptr offset));
    MOCK_METHOD1(getString, WebKit::WebString(WebKit::WGC3Denum name));
    MOCK_METHOD0(getRequestableExtensionsCHROMIUM, WebKit::WebString());
    MOCK_METHOD1(enable, void(WebKit::WGC3Denum cap));
    MOCK_METHOD1(disable, void(WebKit::WGC3Denum cap));
    MOCK_METHOD4(scissor, void(WebKit::WGC3Dint x, WebKit::WGC3Dint y, WebKit::WGC3Dsizei width, WebKit::WGC3Dsizei height));
};

class MockContextHarness {
private:
    MockContext* m_context;
public:
    MockContextHarness(MockContext* context)
        : m_context(context)
    {
        // Catch "uninteresting" calls
        EXPECT_CALL(*m_context, useProgram(_))
            .Times(0);

        EXPECT_CALL(*m_context, drawElements(_, _, _, _))
            .Times(0);

        // These are not asserted
        EXPECT_CALL(*m_context, uniformMatrix4fv(_, _, _, _))
            .WillRepeatedly(Return());

        EXPECT_CALL(*m_context, uniform4f(_, _, _, _, _))
            .WillRepeatedly(Return());

        // Any other strings are empty
        EXPECT_CALL(*m_context, getString(_))
            .WillRepeatedly(Return(WebKit::WebString()));

        // Support for partial swap, if needed
        EXPECT_CALL(*m_context, getString(GL_EXTENSIONS))
            .WillRepeatedly(Return(WebKit::WebString("GL_CHROMIUM_post_sub_buffer")));

        EXPECT_CALL(*m_context, getRequestableExtensionsCHROMIUM())
            .WillRepeatedly(Return(WebKit::WebString("GL_CHROMIUM_post_sub_buffer")));

        // Any un-sanctioned calls to enable() are OK
        EXPECT_CALL(*m_context, enable(_))
            .WillRepeatedly(Return());

        // Any un-sanctioned calls to disable() are OK
        EXPECT_CALL(*m_context, disable(_))
            .WillRepeatedly(Return());
    }

    void mustDrawSolidQuad()
    {
        EXPECT_CALL(*m_context, drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0))
            .WillOnce(Return())
            .RetiresOnSaturation();

        EXPECT_CALL(*m_context, useProgram(_))
            .WillOnce(Return())
            .RetiresOnSaturation();

    }

    void mustSetScissor(int x, int y, int width, int height)
    {
        EXPECT_CALL(*m_context, enable(GL_SCISSOR_TEST))
            .WillRepeatedly(Return());

        EXPECT_CALL(*m_context, scissor(x, y, width, height))
            .Times(AtLeast(1))
            .WillRepeatedly(Return());
    }

    void mustSetNoScissor()
    {
        EXPECT_CALL(*m_context, disable(GL_SCISSOR_TEST))
            .WillRepeatedly(Return());

        EXPECT_CALL(*m_context, enable(GL_SCISSOR_TEST))
            .Times(0);

        EXPECT_CALL(*m_context, scissor(_, _, _, _))
            .Times(0);
    }
};

TEST_P(LayerTreeHostImplTest, noPartialSwap)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new MockContext)).PassAs<OutputSurface>();
    MockContext* mockContext = static_cast<MockContext*>(outputSurface->Context3D());
    MockContextHarness harness(mockContext);

    // Run test case
    createLayerTreeHost(false, outputSurface.Pass());
    setupRootLayerImpl(FakeLayerWithQuads::create(m_hostImpl->activeTree(), 1));

    // without partial swap, and no clipping, no scissor is set.
    harness.mustDrawSolidQuad();
    harness.mustSetNoScissor();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);

    // without partial swap, but a layer does clip its subtree, one scissor is set.
    m_hostImpl->rootLayer()->setMasksToBounds(true);
    harness.mustDrawSolidQuad();
    harness.mustSetScissor(0, 0, 10, 10);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);
}

TEST_P(LayerTreeHostImplTest, partialSwap)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new MockContext)).PassAs<OutputSurface>();
    MockContext* mockContext = static_cast<MockContext*>(outputSurface->Context3D());
    MockContextHarness harness(mockContext);

    createLayerTreeHost(true, outputSurface.Pass());
    setupRootLayerImpl(FakeLayerWithQuads::create(m_hostImpl->activeTree(), 1));

    // The first frame is not a partially-swapped one.
    harness.mustSetScissor(0, 0, 10, 10);
    harness.mustDrawSolidQuad();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);

    // Damage a portion of the frame.
    m_hostImpl->rootLayer()->setUpdateRect(gfx::Rect(0, 0, 2, 3));

    // The second frame will be partially-swapped (the y coordinates are flipped).
    harness.mustSetScissor(0, 7, 2, 3);
    harness.mustDrawSolidQuad();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);
}

class PartialSwapContext : public FakeWebGraphicsContext3D {
public:
    WebKit::WebString getString(WebKit::WGC3Denum name)
    {
        if (name == GL_EXTENSIONS)
            return WebKit::WebString("GL_CHROMIUM_post_sub_buffer");
        return WebKit::WebString();
    }

    WebKit::WebString getRequestableExtensionsCHROMIUM()
    {
        return WebKit::WebString("GL_CHROMIUM_post_sub_buffer");
    }

    // Unlimited texture size.
    virtual void getIntegerv(WebKit::WGC3Denum pname, WebKit::WGC3Dint* value)
    {
        if (pname == GL_MAX_TEXTURE_SIZE)
            *value = 8192;
    }
};

static scoped_ptr<LayerTreeHostImpl> setupLayersForOpacity(bool partialSwap, LayerTreeHostImplClient* client, Proxy* proxy)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    LayerTreeSettings settings;
    settings.partialSwapEnabled = partialSwap;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, client, proxy);
    myHostImpl->initializeRenderer(outputSurface.Pass());
    myHostImpl->setViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

    /*
      Layers are created as follows:

         +--------------------+
         |                  1 |
         |  +-----------+     |
         |  |         2 |     |
         |  | +-------------------+
         |  | |   3               |
         |  | +-------------------+
         |  |           |     |
         |  +-----------+     |
         |                    |
         |                    |
         +--------------------+

         Layers 1, 2 have render surfaces
     */
    scoped_ptr<LayerImpl> root = LayerImpl::create(myHostImpl->activeTree(), 1);
    scoped_ptr<LayerImpl> child = LayerImpl::create(myHostImpl->activeTree(), 2);
    scoped_ptr<LayerImpl> grandChild = FakeLayerWithQuads::create(myHostImpl->activeTree(), 3);

    gfx::Rect rootRect(0, 0, 100, 100);
    gfx::Rect childRect(10, 10, 50, 50);
    gfx::Rect grandChildRect(5, 5, 150, 150);

    root->createRenderSurface();
    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(rootRect.x(), rootRect.y()));
    root->setBounds(gfx::Size(rootRect.width(), rootRect.height()));
    root->setContentBounds(root->bounds());
    root->drawProperties().visible_content_rect = rootRect;
    root->setDrawsContent(false);
    root->renderSurface()->setContentRect(gfx::Rect(gfx::Point(), gfx::Size(rootRect.width(), rootRect.height())));

    child->setAnchorPoint(gfx::PointF(0, 0));
    child->setPosition(gfx::PointF(childRect.x(), childRect.y()));
    child->setOpacity(0.5f);
    child->setBounds(gfx::Size(childRect.width(), childRect.height()));
    child->setContentBounds(child->bounds());
    child->drawProperties().visible_content_rect = childRect;
    child->setDrawsContent(false);
    child->setForceRenderSurface(true);

    grandChild->setAnchorPoint(gfx::PointF(0, 0));
    grandChild->setPosition(gfx::Point(grandChildRect.x(), grandChildRect.y()));
    grandChild->setBounds(gfx::Size(grandChildRect.width(), grandChildRect.height()));
    grandChild->setContentBounds(grandChild->bounds());
    grandChild->drawProperties().visible_content_rect = grandChildRect;
    grandChild->setDrawsContent(true);

    child->addChild(grandChild.Pass());
    root->addChild(child.Pass());

    myHostImpl->activeTree()->SetRootLayer(root.Pass());
    return myHostImpl.Pass();
}

TEST_P(LayerTreeHostImplTest, contributingLayerEmptyScissorPartialSwap)
{
    scoped_ptr<LayerTreeHostImpl> myHostImpl = setupLayersForOpacity(true, this, &m_proxy);

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Verify all quads have been computed
        ASSERT_EQ(2U, frame.renderPasses.size());
        ASSERT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(1U, frame.renderPasses[1]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.renderPasses[0]->quad_list[0]->material);
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, contributingLayerEmptyScissorNoPartialSwap)
{
    scoped_ptr<LayerTreeHostImpl> myHostImpl = setupLayersForOpacity(false, this, &m_proxy);

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Verify all quads have been computed
        ASSERT_EQ(2U, frame.renderPasses.size());
        ASSERT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(1U, frame.renderPasses[1]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.renderPasses[0]->quad_list[0]->material);
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

// Fake WebKit::WebGraphicsContext3D that tracks the number of textures in use.
class TrackingWebGraphicsContext3D : public FakeWebGraphicsContext3D {
public:
    TrackingWebGraphicsContext3D()
        : FakeWebGraphicsContext3D()
        , m_numTextures(0)
    { }

    virtual WebKit::WebGLId createTexture() OVERRIDE
    {
        WebKit::WebGLId id = FakeWebGraphicsContext3D::createTexture();

        m_textures[id] = true;
        ++m_numTextures;
        return id;
    }

    virtual void deleteTexture(WebKit::WebGLId id) OVERRIDE
    {
        if (m_textures.find(id) == m_textures.end())
            return;

        m_textures[id] = false;
        --m_numTextures;
    }

    virtual WebKit::WebString getString(WebKit::WGC3Denum name) OVERRIDE
    {
        if (name == GL_EXTENSIONS)
            return WebKit::WebString("GL_CHROMIUM_iosurface GL_ARB_texture_rectangle");

        return WebKit::WebString();
    }

    unsigned numTextures() const { return m_numTextures; }

private:
    base::hash_map<WebKit::WebGLId, bool> m_textures;
    unsigned m_numTextures;
};

static unsigned createResourceId(ResourceProvider* resourceProvider)
{
    return resourceProvider->createResource(
        gfx::Size(20, 12),
        resourceProvider->bestTextureFormat(),
        ResourceProvider::TextureUsageAny);
}

static unsigned createTextureId(ResourceProvider* resourceProvider)
{
    return ResourceProvider::ScopedReadLockGL(
        resourceProvider, createResourceId(resourceProvider)).textureId();
}

TEST_P(LayerTreeHostImplTest, layersFreeTextures)
{
    scoped_ptr<FakeWebGraphicsContext3D> context =
            FakeWebGraphicsContext3D::Create();
    FakeWebGraphicsContext3D* context3d = context.get();
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(
        context.PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
    m_hostImpl->initializeRenderer(outputSurface.Pass());

    scoped_ptr<LayerImpl> rootLayer(LayerImpl::create(m_hostImpl->activeTree(), 1));
    rootLayer->setBounds(gfx::Size(10, 10));
    rootLayer->setAnchorPoint(gfx::PointF(0, 0));

    scoped_refptr<VideoFrame> softwareFrame(media::VideoFrame::CreateColorFrame(
        gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta()));
    FakeVideoFrameProvider provider;
    provider.set_frame(softwareFrame);
    scoped_ptr<VideoLayerImpl> videoLayer = VideoLayerImpl::create(m_hostImpl->activeTree(), 4, &provider);
    videoLayer->setBounds(gfx::Size(10, 10));
    videoLayer->setAnchorPoint(gfx::PointF(0, 0));
    videoLayer->setContentBounds(gfx::Size(10, 10));
    videoLayer->setDrawsContent(true);
    rootLayer->addChild(videoLayer.PassAs<LayerImpl>());

    scoped_ptr<IOSurfaceLayerImpl> ioSurfaceLayer = IOSurfaceLayerImpl::create(m_hostImpl->activeTree(), 5);
    ioSurfaceLayer->setBounds(gfx::Size(10, 10));
    ioSurfaceLayer->setAnchorPoint(gfx::PointF(0, 0));
    ioSurfaceLayer->setContentBounds(gfx::Size(10, 10));
    ioSurfaceLayer->setDrawsContent(true);
    ioSurfaceLayer->setIOSurfaceProperties(1, gfx::Size(10, 10));
    rootLayer->addChild(ioSurfaceLayer.PassAs<LayerImpl>());

    m_hostImpl->activeTree()->SetRootLayer(rootLayer.Pass());

    EXPECT_EQ(0u, context3d->NumTextures());

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);
    m_hostImpl->swapBuffers();

    EXPECT_GT(context3d->NumTextures(), 0u);

    // Kill the layer tree.
    m_hostImpl->activeTree()->SetRootLayer(LayerImpl::create(m_hostImpl->activeTree(), 100));
    // There should be no textures left in use after.
    EXPECT_EQ(0u, context3d->NumTextures());
}

class MockDrawQuadsToFillScreenContext : public FakeWebGraphicsContext3D {
public:
    MOCK_METHOD1(useProgram, void(WebKit::WebGLId program));
    MOCK_METHOD4(drawElements, void(WebKit::WGC3Denum mode, WebKit::WGC3Dsizei count, WebKit::WGC3Denum type, WebKit::WGC3Dintptr offset));
};

TEST_P(LayerTreeHostImplTest, hasTransparentBackground)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new MockDrawQuadsToFillScreenContext)).PassAs<OutputSurface>();
    MockDrawQuadsToFillScreenContext* mockContext = static_cast<MockDrawQuadsToFillScreenContext*>(outputSurface->Context3D());

    // Run test case
    createLayerTreeHost(false, outputSurface.Pass());
    setupRootLayerImpl(LayerImpl::create(m_hostImpl->activeTree(), 1));
    m_hostImpl->activeTree()->set_background_color(SK_ColorWHITE);

    // Verify one quad is drawn when transparent background set is not set.
    m_hostImpl->activeTree()->set_has_transparent_background(false);
    EXPECT_CALL(*mockContext, useProgram(_))
        .Times(1);
    EXPECT_CALL(*mockContext, drawElements(_, _, _, _))
        .Times(1);
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);
    Mock::VerifyAndClearExpectations(&mockContext);

    // Verify no quads are drawn when transparent background is set.
    m_hostImpl->activeTree()->set_has_transparent_background(true);
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);
    Mock::VerifyAndClearExpectations(&mockContext);
}

static void addDrawingLayerTo(LayerImpl* parent, int id, const gfx::Rect& layerRect, LayerImpl** result)
{
    scoped_ptr<LayerImpl> layer = FakeLayerWithQuads::create(parent->layerTreeImpl(), id);
    LayerImpl* layerPtr = layer.get();
    layerPtr->setAnchorPoint(gfx::PointF(0, 0));
    layerPtr->setPosition(gfx::PointF(layerRect.origin()));
    layerPtr->setBounds(layerRect.size());
    layerPtr->setContentBounds(layerRect.size());
    layerPtr->setDrawsContent(true); // only children draw content
    layerPtr->setContentsOpaque(true);
    parent->addChild(layer.Pass());
    if (result)
        *result = layerPtr;
}

static void setupLayersForTextureCaching(LayerTreeHostImpl* layerTreeHostImpl, LayerImpl*& rootPtr, LayerImpl*& intermediateLayerPtr, LayerImpl*& surfaceLayerPtr, LayerImpl*& childPtr, const gfx::Size& rootSize)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    layerTreeHostImpl->initializeRenderer(outputSurface.Pass());
    layerTreeHostImpl->setViewportSize(rootSize, rootSize);

    scoped_ptr<LayerImpl> root = LayerImpl::create(layerTreeHostImpl->activeTree(), 1);
    rootPtr = root.get();

    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(0, 0));
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setDrawsContent(true);
    layerTreeHostImpl->activeTree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(10, 10, rootSize.width(), rootSize.height()), &intermediateLayerPtr);
    intermediateLayerPtr->setDrawsContent(false); // only children draw content

    // Surface layer is the layer that changes its opacity
    // It will contain other layers that draw content.
    addDrawingLayerTo(intermediateLayerPtr, 3, gfx::Rect(10, 10, rootSize.width(), rootSize.height()), &surfaceLayerPtr);
    surfaceLayerPtr->setDrawsContent(false); // only children draw content
    surfaceLayerPtr->setOpacity(0.5f);
    surfaceLayerPtr->setForceRenderSurface(true); // This will cause it to have a surface

    // Child of the surface layer will produce some quads
    addDrawingLayerTo(surfaceLayerPtr, 4, gfx::Rect(5, 5, rootSize.width() - 25, rootSize.height() - 25), &childPtr);
}

class GLRendererWithReleaseTextures : public GLRenderer {
public:
    using GLRenderer::releaseRenderPassTextures;
};

TEST_P(LayerTreeHostImplTest, textureCachingWithOcclusion)
{
    LayerTreeSettings settings;
    settings.minimumOcclusionTrackingSize = gfx::Size();
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    // Layers are structure as follows:
    //
    //  R +-- S1 +- L10 (owning)
    //    |      +- L11
    //    |      +- L12
    //    |
    //    +-- S2 +- L20 (owning)
    //           +- L21
    //
    // Occlusion:
    // L12 occludes L11 (internal)
    // L20 occludes L10 (external)
    // L21 occludes L20 (internal)

    LayerImpl* rootPtr;
    LayerImpl* layerS1Ptr;
    LayerImpl* layerS2Ptr;

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    gfx::Size rootSize(1000, 1000);

    myHostImpl->initializeRenderer(outputSurface.Pass());
    myHostImpl->setViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::create(myHostImpl->activeTree(), 1);
    rootPtr = root.get();

    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(0, 0));
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setDrawsContent(true);
    root->setMasksToBounds(true);
    myHostImpl->activeTree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(300, 300, 300, 300), &layerS1Ptr);
    layerS1Ptr->setForceRenderSurface(true);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(10, 10, 10, 10), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(0, 0, 30, 30), 0); // L12

    addDrawingLayerTo(rootPtr, 5, gfx::Rect(550, 250, 300, 400), &layerS2Ptr);
    layerS2Ptr->setForceRenderSurface(true);

    addDrawingLayerTo(layerS2Ptr, 6, gfx::Rect(20, 20, 5, 5), 0); // L21

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 2 quads (1 is occluded); for S2, there is 2 quads.
        ASSERT_EQ(3U, frame.renderPasses.size());

        EXPECT_EQ(2U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(2U, frame.renderPasses[1]->quad_list.size());
        EXPECT_EQ(2U, frame.renderPasses[2]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(150, 150);
    layerS2Ptr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads
        // For S1, the number of quads depends on what got unoccluded, so not asserted beyond being positive.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.renderPasses.size());

        EXPECT_GT(frame.renderPasses[0]->quad_list.size(), 0U);
        EXPECT_EQ(2U, frame.renderPasses[1]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // "Re-occlude" surface S1 and repeat draw.
    // Must remove S1's render pass since it is now available in full.
    // S2 has no change so must also be removed.
    transform = layerS2Ptr->transform();
    transform.Translate(-15, -15);
    layerS2Ptr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 1 render pass - for the root.
        ASSERT_EQ(1U, frame.renderPasses.size());

        EXPECT_EQ(2U, frame.renderPasses[0]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

}

TEST_P(LayerTreeHostImplTest, textureCachingWithOcclusionEarlyOut)
{
    LayerTreeSettings settings;
    settings.minimumOcclusionTrackingSize = gfx::Size();
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    // Layers are structure as follows:
    //
    //  R +-- S1 +- L10 (owning, non drawing)
    //    |      +- L11 (corner, unoccluded)
    //    |      +- L12 (corner, unoccluded)
    //    |      +- L13 (corner, unoccluded)
    //    |      +- L14 (corner, entirely occluded)
    //    |
    //    +-- S2 +- L20 (owning, drawing)
    //

    LayerImpl* rootPtr;
    LayerImpl* layerS1Ptr;
    LayerImpl* layerS2Ptr;

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    gfx::Size rootSize(1000, 1000);

    myHostImpl->initializeRenderer(outputSurface.Pass());
    myHostImpl->setViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::create(myHostImpl->activeTree(), 1);
    rootPtr = root.get();

    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(0, 0));
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setDrawsContent(true);
    root->setMasksToBounds(true);
    myHostImpl->activeTree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(0, 0, 800, 800), &layerS1Ptr);
    layerS1Ptr->setForceRenderSurface(true);
    layerS1Ptr->setDrawsContent(false);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(0, 0, 300, 300), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(0, 500, 300, 300), 0); // L12
    addDrawingLayerTo(layerS1Ptr, 5, gfx::Rect(500, 0, 300, 300), 0); // L13
    addDrawingLayerTo(layerS1Ptr, 6, gfx::Rect(500, 500, 300, 300), 0); // L14
    addDrawingLayerTo(layerS1Ptr, 9, gfx::Rect(500, 500, 300, 300), 0); // L14

    addDrawingLayerTo(rootPtr, 7, gfx::Rect(450, 450, 450, 450), &layerS2Ptr);
    layerS2Ptr->setForceRenderSurface(true);

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 3 quads; for S2, there is 1 quad.
        ASSERT_EQ(3U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());

        // L14 is culled, so only 3 quads.
        EXPECT_EQ(3U, frame.renderPasses[1]->quad_list.size());
        EXPECT_EQ(2U, frame.renderPasses[2]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(100, 100);
    layerS2Ptr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads
        // For S1, the number of quads depends on what got unoccluded, so not asserted beyond being positive.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.renderPasses.size());

        EXPECT_GT(frame.renderPasses[0]->quad_list.size(), 0U);
        EXPECT_EQ(2U, frame.renderPasses[1]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // "Re-occlude" surface S1 and repeat draw.
    // Must remove S1's render pass since it is now available in full.
    // S2 has no change so must also be removed.
    transform = layerS2Ptr->transform();
    transform.Translate(-15, -15);
    layerS2Ptr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 1 render pass - for the root.
        ASSERT_EQ(1U, frame.renderPasses.size());

        EXPECT_EQ(2U, frame.renderPasses[0]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, textureCachingWithOcclusionExternalOverInternal)
{
    LayerTreeSettings settings;
    settings.minimumOcclusionTrackingSize = gfx::Size();
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    // Layers are structured as follows:
    //
    //  R +-- S1 +- L10 (owning, drawing)
    //    |      +- L11 (corner, occluded by L12)
    //    |      +- L12 (opposite corner)
    //    |
    //    +-- S2 +- L20 (owning, drawing)
    //

    LayerImpl* rootPtr;
    LayerImpl* layerS1Ptr;
    LayerImpl* layerS2Ptr;

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    gfx::Size rootSize(1000, 1000);

    myHostImpl->initializeRenderer(outputSurface.Pass());
    myHostImpl->setViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::create(myHostImpl->activeTree(), 1);
    rootPtr = root.get();

    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(0, 0));
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setDrawsContent(true);
    root->setMasksToBounds(true);
    myHostImpl->activeTree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(0, 0, 400, 400), &layerS1Ptr);
    layerS1Ptr->setForceRenderSurface(true);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(0, 0, 300, 300), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(100, 0, 300, 300), 0); // L12

    addDrawingLayerTo(rootPtr, 7, gfx::Rect(200, 0, 300, 300), &layerS2Ptr);
    layerS2Ptr->setForceRenderSurface(true);

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 3 quads; for S2, there is 1 quad.
        ASSERT_EQ(3U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(3U, frame.renderPasses[1]->quad_list.size());
        EXPECT_EQ(2U, frame.renderPasses[2]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(300, 0);
    layerS2Ptr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads
        // For S1, the number of quads depends on what got unoccluded, so not asserted beyond being positive.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.renderPasses.size());

        EXPECT_GT(frame.renderPasses[0]->quad_list.size(), 0U);
        EXPECT_EQ(2U, frame.renderPasses[1]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, textureCachingWithOcclusionExternalNotAligned)
{
    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    // Layers are structured as follows:
    //
    //  R +-- S1 +- L10 (rotated, drawing)
    //           +- L11 (occupies half surface)

    LayerImpl* rootPtr;
    LayerImpl* layerS1Ptr;

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    gfx::Size rootSize(1000, 1000);

    myHostImpl->initializeRenderer(outputSurface.Pass());
    myHostImpl->setViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::create(myHostImpl->activeTree(), 1);
    rootPtr = root.get();

    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(0, 0));
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setDrawsContent(true);
    root->setMasksToBounds(true);
    myHostImpl->activeTree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(0, 0, 400, 400), &layerS1Ptr);
    layerS1Ptr->setForceRenderSurface(true);
    gfx::Transform transform = layerS1Ptr->transform();
    transform.Translate(200, 200);
    transform.Rotate(45);
    transform.Translate(-200, -200);
    layerS1Ptr->setTransform(transform);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(200, 0, 200, 400), 0); // L11

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 2 render passes.
        ASSERT_EQ(2U, frame.renderPasses.size());

        EXPECT_EQ(2U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(1U, frame.renderPasses[1]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change opacity and draw. Verify we used cached texture.
    layerS1Ptr->setOpacity(0.2f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // One render pass must be gone due to cached texture.
        ASSERT_EQ(1U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, textureCachingWithOcclusionPartialSwap)
{
    LayerTreeSettings settings;
    settings.minimumOcclusionTrackingSize = gfx::Size();
    settings.partialSwapEnabled = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    // Layers are structure as follows:
    //
    //  R +-- S1 +- L10 (owning)
    //    |      +- L11
    //    |      +- L12
    //    |
    //    +-- S2 +- L20 (owning)
    //           +- L21
    //
    // Occlusion:
    // L12 occludes L11 (internal)
    // L20 occludes L10 (external)
    // L21 occludes L20 (internal)

    LayerImpl* rootPtr;
    LayerImpl* layerS1Ptr;
    LayerImpl* layerS2Ptr;

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    gfx::Size rootSize(1000, 1000);

    myHostImpl->initializeRenderer(outputSurface.Pass());
    myHostImpl->setViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::create(myHostImpl->activeTree(), 1);
    rootPtr = root.get();

    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(0, 0));
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setDrawsContent(true);
    root->setMasksToBounds(true);
    myHostImpl->activeTree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(300, 300, 300, 300), &layerS1Ptr);
    layerS1Ptr->setForceRenderSurface(true);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(10, 10, 10, 10), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(0, 0, 30, 30), 0); // L12

    addDrawingLayerTo(rootPtr, 5, gfx::Rect(550, 250, 300, 400), &layerS2Ptr);
    layerS2Ptr->setForceRenderSurface(true);

    addDrawingLayerTo(layerS2Ptr, 6, gfx::Rect(20, 20, 5, 5), 0); // L21

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 2 quads (one is occluded); for S2, there is 2 quads.
        ASSERT_EQ(3U, frame.renderPasses.size());

        EXPECT_EQ(2U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(2U, frame.renderPasses[1]->quad_list.size());
        EXPECT_EQ(2U, frame.renderPasses[2]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(150, 150);
    layerS2Ptr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads.
        // For S1, there are 2 quads.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.renderPasses.size());

        EXPECT_EQ(2U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(2U, frame.renderPasses[1]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // "Re-occlude" surface S1 and repeat draw.
    // Must remove S1's render pass since it is now available in full.
    // S2 has no change so must also be removed.
    transform = layerS2Ptr->transform();
    transform.Translate(-15, -15);
    layerS2Ptr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Root render pass only.
        ASSERT_EQ(1U, frame.renderPasses.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, textureCachingWithScissor)
{
    LayerTreeSettings settings;
    settings.minimumOcclusionTrackingSize = gfx::Size();
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    /*
      Layers are created as follows:

         +--------------------+
         |                  1 |
         |  +-----------+     |
         |  |         2 |     |
         |  | +-------------------+
         |  | |   3               |
         |  | +-------------------+
         |  |           |     |
         |  +-----------+     |
         |                    |
         |                    |
         +--------------------+

         Layers 1, 2 have render surfaces
     */
    scoped_ptr<LayerImpl> root = LayerImpl::create(myHostImpl->activeTree(), 1);
    scoped_ptr<TiledLayerImpl> child = TiledLayerImpl::create(myHostImpl->activeTree(), 2);
    scoped_ptr<LayerImpl> grandChild = LayerImpl::create(myHostImpl->activeTree(), 3);

    gfx::Rect rootRect(0, 0, 100, 100);
    gfx::Rect childRect(10, 10, 50, 50);
    gfx::Rect grandChildRect(5, 5, 150, 150);

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();
    myHostImpl->initializeRenderer(outputSurface.Pass());

    root->setAnchorPoint(gfx::PointF(0, 0));
    root->setPosition(gfx::PointF(rootRect.x(), rootRect.y()));
    root->setBounds(gfx::Size(rootRect.width(), rootRect.height()));
    root->setContentBounds(root->bounds());
    root->setDrawsContent(true);
    root->setMasksToBounds(true);

    child->setAnchorPoint(gfx::PointF(0, 0));
    child->setPosition(gfx::PointF(childRect.x(), childRect.y()));
    child->setOpacity(0.5);
    child->setBounds(gfx::Size(childRect.width(), childRect.height()));
    child->setContentBounds(child->bounds());
    child->setDrawsContent(true);
    child->setSkipsDraw(false);

    // child layer has 10x10 tiles.
    scoped_ptr<LayerTilingData> tiler = LayerTilingData::create(gfx::Size(10, 10), LayerTilingData::HasBorderTexels);
    tiler->setBounds(child->contentBounds());
    child->setTilingData(*tiler.get());

    grandChild->setAnchorPoint(gfx::PointF(0, 0));
    grandChild->setPosition(gfx::Point(grandChildRect.x(), grandChildRect.y()));
    grandChild->setBounds(gfx::Size(grandChildRect.width(), grandChildRect.height()));
    grandChild->setContentBounds(grandChild->bounds());
    grandChild->setDrawsContent(true);

    TiledLayerImpl* childPtr = child.get();
    RenderPass::Id childPassId(childPtr->id(), 0);

    child->addChild(grandChild.Pass());
    root->addChild(child.PassAs<LayerImpl>());
    myHostImpl->activeTree()->SetRootLayer(root.Pass());
    myHostImpl->setViewportSize(rootRect.size(), rootRect.size());

    EXPECT_FALSE(myHostImpl->renderer()->haveCachedResourcesForRenderPassId(childPassId));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));
        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // We should have cached textures for surface 2.
    EXPECT_TRUE(myHostImpl->renderer()->haveCachedResourcesForRenderPassId(childPassId));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));
        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // We should still have cached textures for surface 2 after drawing with no damage.
    EXPECT_TRUE(myHostImpl->renderer()->haveCachedResourcesForRenderPassId(childPassId));

    // Damage a single tile of surface 2.
    childPtr->setUpdateRect(gfx::Rect(10, 10, 10, 10));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));
        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // We should have a cached texture for surface 2 again even though it was damaged.
    EXPECT_TRUE(myHostImpl->renderer()->haveCachedResourcesForRenderPassId(childPassId));
}

TEST_P(LayerTreeHostImplTest, surfaceTextureCaching)
{
    LayerTreeSettings settings;
    settings.minimumOcclusionTrackingSize = gfx::Size();
    settings.partialSwapEnabled = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    LayerImpl* rootPtr;
    LayerImpl* intermediateLayerPtr;
    LayerImpl* surfaceLayerPtr;
    LayerImpl* childPtr;

    setupLayersForTextureCaching(myHostImpl.get(), rootPtr, intermediateLayerPtr, surfaceLayerPtr, childPtr, gfx::Size(100, 100));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.renderPasses.size());
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(1U, frame.renderPasses[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[1]->quad_list[0]);
        RenderPass* targetPass = frame.renderPassesById[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Draw without any change
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_TRUE(frame.renderPassesById.find(quad->render_pass_id) == frame.renderPassesById.end());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change opacity and draw
    surfaceLayerPtr->setOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_TRUE(frame.renderPassesById.find(quad->render_pass_id) == frame.renderPassesById.end());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change less benign property and draw - should have contents changed flag
    surfaceLayerPtr->setStackingOrderChanged(true);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.renderPasses[0]->quad_list[0]->material);

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[1]->quad_list[0]);
        RenderPass* targetPass = frame.renderPassesById[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change opacity again, and evict the cached surface texture.
    surfaceLayerPtr->setOpacity(0.5f);
    static_cast<GLRendererWithReleaseTextures*>(myHostImpl->renderer())->releaseRenderPassTextures();

    // Change opacity and draw
    surfaceLayerPtr->setOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive two render passes
        ASSERT_EQ(2U, frame.renderPasses.size());

        // Even though not enough properties changed, the entire thing must be
        // redrawn as we don't have cached textures
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(1U, frame.renderPasses[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[1]->quad_list[0]);
        RenderPass* targetPass = frame.renderPassesById[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_TRUE(targetPass->damage_rect.IsEmpty());

        // Was our surface evicted?
        EXPECT_FALSE(myHostImpl->renderer()->haveCachedResourcesForRenderPassId(targetPass->id));

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Draw without any change, to make sure the state is clear
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_TRUE(frame.renderPassesById.find(quad->render_pass_id) == frame.renderPassesById.end());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change location of the intermediate layer
    gfx::Transform transform = intermediateLayerPtr->transform();
    transform.matrix().setDouble(0, 3, 1.0001);
    intermediateLayerPtr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive one render pass, as the other one should be culled.
        ASSERT_EQ(1U, frame.renderPasses.size());
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_TRUE(frame.renderPassesById.find(quad->render_pass_id) == frame.renderPassesById.end());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, surfaceTextureCachingNoPartialSwap)
{
    LayerTreeSettings settings;
    settings.minimumOcclusionTrackingSize = gfx::Size();
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::create(settings, this, &m_proxy);

    LayerImpl* rootPtr;
    LayerImpl* intermediateLayerPtr;
    LayerImpl* surfaceLayerPtr;
    LayerImpl* childPtr;

    setupLayersForTextureCaching(myHostImpl.get(), rootPtr, intermediateLayerPtr, surfaceLayerPtr, childPtr, gfx::Size(100, 100));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.renderPasses.size());
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(1U, frame.renderPasses[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[1]->quad_list[0]);
        RenderPass* targetPass = frame.renderPassesById[quad->render_pass_id];
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        EXPECT_FALSE(frame.renderPasses[0]->damage_rect.IsEmpty());
        EXPECT_FALSE(frame.renderPasses[1]->damage_rect.IsEmpty());

        EXPECT_FALSE(frame.renderPasses[0]->has_occlusion_from_outside_target_surface);
        EXPECT_FALSE(frame.renderPasses[1]->has_occlusion_from_outside_target_surface);

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Draw without any change
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Even though there was no change, we set the damage to entire viewport.
        // One of the passes should be culled as a result, since contents didn't change
        // and we have cached texture.
        ASSERT_EQ(1U, frame.renderPasses.size());
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());

        EXPECT_TRUE(frame.renderPasses[0]->damage_rect.IsEmpty());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change opacity and draw
    surfaceLayerPtr->setOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_TRUE(frame.renderPassesById.find(quad->render_pass_id) == frame.renderPassesById.end());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change less benign property and draw - should have contents changed flag
    surfaceLayerPtr->setStackingOrderChanged(true);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.renderPasses.size());

        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.renderPasses[0]->quad_list[0]->material);

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[1]->quad_list[0]);
        RenderPass* targetPass = frame.renderPassesById[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change opacity again, and evict the cached surface texture.
    surfaceLayerPtr->setOpacity(0.5f);
    static_cast<GLRendererWithReleaseTextures*>(myHostImpl->renderer())->releaseRenderPassTextures();

    // Change opacity and draw
    surfaceLayerPtr->setOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive two render passes
        ASSERT_EQ(2U, frame.renderPasses.size());

        // Even though not enough properties changed, the entire thing must be
        // redrawn as we don't have cached textures
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());
        EXPECT_EQ(1U, frame.renderPasses[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[1]->quad_list[0]);
        RenderPass* targetPass = frame.renderPassesById[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_TRUE(targetPass->damage_rect.IsEmpty());

        // Was our surface evicted?
        EXPECT_FALSE(myHostImpl->renderer()->haveCachedResourcesForRenderPassId(targetPass->id));

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Draw without any change, to make sure the state is clear
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Even though there was no change, we set the damage to entire viewport.
        // One of the passes should be culled as a result, since contents didn't change
        // and we have cached texture.
        ASSERT_EQ(1U, frame.renderPasses.size());
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }

    // Change location of the intermediate layer
    gfx::Transform transform = intermediateLayerPtr->transform();
    transform.matrix().setDouble(0, 3, 1.0001);
    intermediateLayerPtr->setTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->prepareToDraw(frame));

        // Must receive one render pass, as the other one should be culled.
        ASSERT_EQ(1U, frame.renderPasses.size());
        EXPECT_EQ(1U, frame.renderPasses[0]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_TRUE(frame.renderPassesById.find(quad->render_pass_id) == frame.renderPassesById.end());

        myHostImpl->drawLayers(frame);
        myHostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, releaseContentsTextureShouldTriggerCommit)
{
    setReduceMemoryResult(false);

    // Even if changing the memory limit didn't result in anything being
    // evicted, we need to re-commit because the new value may result in us
    // drawing something different than before.
    setReduceMemoryResult(false);
    m_hostImpl->setManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memoryAllocationLimitBytes() - 1));
    EXPECT_TRUE(m_didRequestCommit);
    m_didRequestCommit = false;

    // Especially if changing the memory limit caused evictions, we need
    // to re-commit.
    setReduceMemoryResult(true);
    m_hostImpl->setManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memoryAllocationLimitBytes() - 1));
    EXPECT_TRUE(m_didRequestCommit);
    m_didRequestCommit = false;

    // But if we set it to the same value that it was before, we shouldn't
    // re-commit.
    m_hostImpl->setManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memoryAllocationLimitBytes()));
    EXPECT_FALSE(m_didRequestCommit);
}

struct RenderPassRemovalTestData : public LayerTreeHostImpl::FrameData {
    ScopedPtrHashMap<RenderPass::Id, TestRenderPass> renderPassCache;
    scoped_ptr<SharedQuadState> sharedQuadState;
};

class TestRenderer : public GLRenderer, public RendererClient {
public:
    static scoped_ptr<TestRenderer> create(ResourceProvider* resourceProvider, OutputSurface* outputSurface, Proxy* proxy)
    {
        scoped_ptr<TestRenderer> renderer(new TestRenderer(resourceProvider, outputSurface, proxy));
        if (!renderer->initialize())
            return scoped_ptr<TestRenderer>();

        return renderer.Pass();
    }

    void clearCachedTextures() { m_textures.clear(); }
    void setHaveCachedResourcesForRenderPassId(RenderPass::Id id) { m_textures.insert(id); }

    virtual bool haveCachedResourcesForRenderPassId(RenderPass::Id id) const OVERRIDE { return m_textures.count(id); }

    // RendererClient implementation.
    virtual const gfx::Size& deviceViewportSize() const OVERRIDE { return m_viewportSize; }
    virtual const LayerTreeSettings& settings() const OVERRIDE { return m_settings; }
    virtual void didLoseOutputSurface() OVERRIDE { }
    virtual void onSwapBuffersComplete() OVERRIDE { }
    virtual void setFullRootLayerDamage() OVERRIDE { }
    virtual void setManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { }
    virtual void enforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { }
    virtual bool hasImplThread() const OVERRIDE { return false; }
    virtual bool shouldClearRootRenderPass() const OVERRIDE { return true; }
    virtual CompositorFrameMetadata makeCompositorFrameMetadata() const
        OVERRIDE { return CompositorFrameMetadata(); }

protected:
    TestRenderer(ResourceProvider* resourceProvider, OutputSurface* outputSurface, Proxy* proxy) : GLRenderer(this, outputSurface, resourceProvider) { }

private:
    LayerTreeSettings m_settings;
    gfx::Size m_viewportSize;
    base::hash_set<RenderPass::Id> m_textures;
};

static void configureRenderPassTestData(const char* testScript, RenderPassRemovalTestData& testData, TestRenderer* renderer)
{
    renderer->clearCachedTextures();

    // One shared state for all quads - we don't need the correct details
    testData.sharedQuadState = SharedQuadState::Create();
    testData.sharedQuadState->SetAll(gfx::Transform(), gfx::Rect(), gfx::Rect(), gfx::Rect(), false, 1.0);

    const char* currentChar = testScript;

    // Pre-create root pass
    RenderPass::Id rootRenderPassId = RenderPass::Id(testScript[0], testScript[1]);
    scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
    pass->SetNew(rootRenderPassId, gfx::Rect(), gfx::Rect(), gfx::Transform());
    testData.renderPassCache.add(rootRenderPassId, pass.Pass());
    while (*currentChar) {
        int layerId = *currentChar;
        currentChar++;
        ASSERT_TRUE(currentChar);
        int index = *currentChar;
        currentChar++;

        RenderPass::Id renderPassId = RenderPass::Id(layerId, index);

        bool isReplica = false;
        if (!testData.renderPassCache.contains(renderPassId))
            isReplica = true;

        scoped_ptr<TestRenderPass> renderPass = testData.renderPassCache.take(renderPassId);

        // Cycle through quad data and create all quads
        while (*currentChar && *currentChar != '\n') {
            if (*currentChar == 's') {
                // Solid color draw quad
                scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
                quad->SetNew(testData.sharedQuadState.get(), gfx::Rect(0, 0, 10, 10), SK_ColorWHITE);

                renderPass->AppendQuad(quad.PassAs<DrawQuad>());
                currentChar++;
            } else if ((*currentChar >= 'A') && (*currentChar <= 'Z')) {
                // RenderPass draw quad
                int layerId = *currentChar;
                currentChar++;
                ASSERT_TRUE(currentChar);
                int index = *currentChar;
                currentChar++;
                RenderPass::Id newRenderPassId = RenderPass::Id(layerId, index);
                ASSERT_NE(rootRenderPassId, newRenderPassId);
                bool hasTexture = false;
                bool contentsChanged = true;

                if (*currentChar == '[') {
                    currentChar++;
                    while (*currentChar && *currentChar != ']') {
                        switch (*currentChar) {
                        case 'c':
                            contentsChanged = false;
                            break;
                        case 't':
                            hasTexture = true;
                            break;
                        }
                        currentChar++;
                    }
                    if (*currentChar == ']')
                        currentChar++;
                }

                if (testData.renderPassCache.find(newRenderPassId) == testData.renderPassCache.end()) {
                    if (hasTexture)
                        renderer->setHaveCachedResourcesForRenderPassId(newRenderPassId);

                    scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
                    pass->SetNew(newRenderPassId, gfx::Rect(), gfx::Rect(), gfx::Transform());
                    testData.renderPassCache.add(newRenderPassId, pass.Pass());
                }

                gfx::Rect quadRect = gfx::Rect(0, 0, 1, 1);
                gfx::Rect contentsChangedRect = contentsChanged ? quadRect : gfx::Rect();
                scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
                quad->SetNew(testData.sharedQuadState.get(), quadRect, newRenderPassId, isReplica, 1, contentsChangedRect, gfx::RectF(0, 0, 1, 1), WebKit::WebFilterOperations(), skia::RefPtr<SkImageFilter>(), WebKit::WebFilterOperations());
                renderPass->AppendQuad(quad.PassAs<DrawQuad>());
            }
        }
        testData.renderPassesById[renderPassId] = renderPass.get();
        testData.renderPasses.insert(testData.renderPasses.begin(), renderPass.PassAs<RenderPass>());
        if (*currentChar)
            currentChar++;
    }
}

void dumpRenderPassTestData(const RenderPassRemovalTestData& testData, char* buffer)
{
    char* pos = buffer;
    for (RenderPassList::const_reverse_iterator it = testData.renderPasses.rbegin(); it != testData.renderPasses.rend(); ++it) {
        const RenderPass* currentPass = *it;
        *pos = currentPass->id.layer_id;
        pos++;
        *pos = currentPass->id.index;
        pos++;

        QuadList::const_iterator quadListIterator = currentPass->quad_list.begin();
        while (quadListIterator != currentPass->quad_list.end()) {
            DrawQuad* currentQuad = *quadListIterator;
            switch (currentQuad->material) {
            case DrawQuad::SOLID_COLOR:
                *pos = 's';
                pos++;
                break;
            case DrawQuad::RENDER_PASS:
                *pos = RenderPassDrawQuad::MaterialCast(currentQuad)->render_pass_id.layer_id;
                pos++;
                *pos = RenderPassDrawQuad::MaterialCast(currentQuad)->render_pass_id.index;
                pos++;
                break;
            default:
                *pos = 'x';
                pos++;
                break;
            }

            quadListIterator++;
        }
        *pos = '\n';
        pos++;
    }
    *pos = '\0';
}

// Each RenderPassList is represented by a string which describes the configuration.
// The syntax of the string is as follows:
//
//                                                      RsssssX[c]ssYsssZ[t]ssW[ct]
// Identifies the render pass---------------------------^ ^^^ ^ ^   ^     ^     ^
// These are solid color quads-----------------------------+  | |   |     |     |
// Identifies RenderPassDrawQuad's RenderPass-----------------+ |   |     |     |
// This quad's contents didn't change---------------------------+   |     |     |
// This quad's contents changed and it has no texture---------------+     |     |
// This quad has texture but its contents changed-------------------------+     |
// This quad's contents didn't change and it has texture - will be removed------+
//
// Expected results have exactly the same syntax, except they do not use square brackets,
// since we only check the structure, not attributes.
//
// Test case configuration consists of initialization script and expected results,
// all in the same format.
struct TestCase {
    const char* name;
    const char* initScript;
    const char* expectedResult;
};

TestCase removeRenderPassesCases[] =
    {
        {
            "Single root pass",
            "R0ssss\n",
            "R0ssss\n"
        }, {
            "Single pass - no quads",
            "R0\n",
            "R0\n"
        }, {
            "Two passes, no removal",
            "R0ssssA0sss\n"
            "A0ssss\n",
            "R0ssssA0sss\n"
            "A0ssss\n"
        }, {
            "Two passes, remove last",
            "R0ssssA0[ct]sss\n"
            "A0ssss\n",
            "R0ssssA0sss\n"
        }, {
            "Have texture but contents changed - leave pass",
            "R0ssssA0[t]sss\n"
            "A0ssss\n",
            "R0ssssA0sss\n"
            "A0ssss\n"
        }, {
            "Contents didn't change but no texture - leave pass",
            "R0ssssA0[c]sss\n"
            "A0ssss\n",
            "R0ssssA0sss\n"
            "A0ssss\n"
        }, {
            "Replica: two quads reference the same pass; remove",
            "R0ssssA0[ct]A0[ct]sss\n"
            "A0ssss\n",
            "R0ssssA0A0sss\n"
        }, {
            "Replica: two quads reference the same pass; leave",
            "R0ssssA0[c]A0[c]sss\n"
            "A0ssss\n",
            "R0ssssA0A0sss\n"
            "A0ssss\n",
        }, {
            "Many passes, remove all",
            "R0ssssA0[ct]sss\n"
            "A0sssB0[ct]C0[ct]s\n"
            "B0sssD0[ct]ssE0[ct]F0[ct]\n"
            "E0ssssss\n"
            "C0G0[ct]\n"
            "D0sssssss\n"
            "F0sssssss\n"
            "G0sss\n",

            "R0ssssA0sss\n"
        }, {
            "Deep recursion, remove all",

            "R0sssssA0[ct]ssss\n"
            "A0ssssB0sss\n"
            "B0C0\n"
            "C0D0\n"
            "D0E0\n"
            "E0F0\n"
            "F0G0\n"
            "G0H0\n"
            "H0sssI0sss\n"
            "I0J0\n"
            "J0ssss\n",

            "R0sssssA0ssss\n"
        }, {
            "Wide recursion, remove all",
            "R0A0[ct]B0[ct]C0[ct]D0[ct]E0[ct]F0[ct]G0[ct]H0[ct]I0[ct]J0[ct]\n"
            "A0s\n"
            "B0s\n"
            "C0ssss\n"
            "D0ssss\n"
            "E0s\n"
            "F0\n"
            "G0s\n"
            "H0s\n"
            "I0s\n"
            "J0ssss\n",

            "R0A0B0C0D0E0F0G0H0I0J0\n"
        }, {
            "Remove passes regardless of cache state",
            "R0ssssA0[ct]sss\n"
            "A0sssB0C0s\n"
            "B0sssD0[c]ssE0[t]F0\n"
            "E0ssssss\n"
            "C0G0\n"
            "D0sssssss\n"
            "F0sssssss\n"
            "G0sss\n",

            "R0ssssA0sss\n"
        }, {
            "Leave some passes, remove others",

            "R0ssssA0[c]sss\n"
            "A0sssB0[t]C0[ct]s\n"
            "B0sssD0[c]ss\n"
            "C0G0\n"
            "D0sssssss\n"
            "G0sss\n",

            "R0ssssA0sss\n"
            "A0sssB0C0s\n"
            "B0sssD0ss\n"
            "D0sssssss\n"
        }, {
            0, 0, 0
        }
    };

static void verifyRenderPassTestData(TestCase& testCase, RenderPassRemovalTestData& testData)
{
    char actualResult[1024];
    dumpRenderPassTestData(testData, actualResult);
    EXPECT_STREQ(testCase.expectedResult, actualResult) << "In test case: " << testCase.name;
}

TEST_P(LayerTreeHostImplTest, testRemoveRenderPasses)
{
    scoped_ptr<OutputSurface> outputSurface(createOutputSurface());
    ASSERT_TRUE(outputSurface->Context3D());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));

    scoped_ptr<TestRenderer> renderer(TestRenderer::create(resourceProvider.get(), outputSurface.get(), &m_proxy));

    int testCaseIndex = 0;
    while (removeRenderPassesCases[testCaseIndex].name) {
        RenderPassRemovalTestData testData;
        configureRenderPassTestData(removeRenderPassesCases[testCaseIndex].initScript, testData, renderer.get());
        LayerTreeHostImpl::removeRenderPasses(LayerTreeHostImpl::CullRenderPassesWithCachedTextures(*renderer), testData);
        verifyRenderPassTestData(removeRenderPassesCases[testCaseIndex], testData);
        testCaseIndex++;
    }
}

// Make sure that scrolls that only pan the pinch viewport, and not the document,
// still force redraw/commit.
void LayerTreeHostImplTest::pinchZoomPanViewportForcesCommitRedraw(const float deviceScaleFactor)
{
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);

    gfx::Size layoutSurfaceSize(10, 20);
    gfx::Size deviceSurfaceSize(layoutSurfaceSize.width() * static_cast<int>(deviceScaleFactor),
                                layoutSurfaceSize.height() * static_cast<int>(deviceScaleFactor));
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, layoutSurfaceSize);
    // For this test we want to force scrolls to only pan the pinchZoomViewport
    // and not the document, we can verify commit/redraw are requested.
    root->setMaxScrollOffset(gfx::Vector2d());
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(layoutSurfaceSize, deviceSurfaceSize);
    m_hostImpl->setPageScaleFactorAndLimits(1, 1, pageScale);
    initializeRendererAndDrawFrame();

    // Set new page scale on impl thread by pinching.
    m_hostImpl->pinchGestureBegin();
    m_hostImpl->pinchGestureUpdate(pageScale, gfx::Point());
    m_hostImpl->pinchGestureEnd();
    drawOneFrame();

    gfx::Transform expectedImplTransform;
    expectedImplTransform.Scale(pageScale, pageScale);

    // Verify the pinch zoom took place.
    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());

    // The implTransform ignores the scroll if !pageScalePinchZoomEnabled,
    // so no point in continuing without it.
    if (!m_hostImpl->settings().pageScalePinchZoomEnabled)
        return;

    m_didRequestCommit = false;
    m_didRequestRedraw = false;

    // This scroll will force the viewport to pan horizontally.
    gfx::Vector2d scrollDelta(5, 0);
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();

    EXPECT_EQ(true, m_didRequestCommit);
    EXPECT_EQ(true, m_didRequestRedraw);

    m_didRequestCommit = false;
    m_didRequestRedraw = false;

    // This scroll will force the viewport to pan vertically.
    scrollDelta = gfx::Vector2d(0, 5);
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->scrollEnd();

    EXPECT_EQ(true, m_didRequestCommit);
    EXPECT_EQ(true, m_didRequestRedraw);
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportForcesCommitDeviceScaleFactor1)
{
    pinchZoomPanViewportForcesCommitRedraw(1);
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportForcesCommitDeviceScaleFactor2)
{
    pinchZoomPanViewportForcesCommitRedraw(2);
}

// The following test confirms correct operation of scroll of the pinchZoomViewport.
// The device scale factor directly affects computation of the implTransform, so
// we test the two most common use cases.
void LayerTreeHostImplTest::pinchZoomPanViewportTest(const float deviceScaleFactor)
{
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);

    gfx::Size layoutSurfaceSize(10, 20);
    gfx::Size deviceSurfaceSize(layoutSurfaceSize.width() * static_cast<int>(deviceScaleFactor),
                                layoutSurfaceSize.height() * static_cast<int>(deviceScaleFactor));
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, layoutSurfaceSize);
    // For this test we want to force scrolls to move the pinchZoomViewport so
    // we can see the scroll component on the implTransform.
    root->setMaxScrollOffset(gfx::Vector2d());
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(layoutSurfaceSize, deviceSurfaceSize);
    m_hostImpl->setPageScaleFactorAndLimits(1, 1, pageScale);
    initializeRendererAndDrawFrame();

    // Set new page scale on impl thread by pinching.
    m_hostImpl->pinchGestureBegin();
    m_hostImpl->pinchGestureUpdate(pageScale, gfx::Point());
    m_hostImpl->pinchGestureEnd();
    drawOneFrame();

    gfx::Transform expectedImplTransform;
    expectedImplTransform.Scale(pageScale, pageScale);

    EXPECT_EQ(m_hostImpl->rootLayer()->implTransform(), expectedImplTransform);

    // The implTransform ignores the scroll if !pageScalePinchZoomEnabled,
    // so no point in continuing without it.
    if (!m_hostImpl->settings().pageScalePinchZoomEnabled)
        return;

    gfx::Vector2d scrollDelta(5, 0);
    // TODO(wjmaclean): Fix the math here so that the expectedTranslation is
    // scaled instead of the scroll input.
    gfx::Vector2d scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    gfx::Vector2d expectedMaxScroll(m_hostImpl->rootLayer()->maxScrollOffset());
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    gfx::Vector2dF expectedTranslation = gfx::ScaleVector2d(scrollDelta, m_hostImpl->deviceScaleFactor());
    expectedImplTransform.Translate(-expectedTranslation.x(), -expectedTranslation.y());

    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());
    // No change expected.
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());
    // None of the scroll delta should have been used for document scroll.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectNone(*scrollInfo.get(), m_hostImpl->rootLayer()->id());

    // Test scroll in y-direction also.
    scrollDelta = gfx::Vector2d(0, 5);
    scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    expectedTranslation = gfx::ScaleVector2d(scrollDelta, m_hostImpl->deviceScaleFactor());
    expectedImplTransform.Translate(-expectedTranslation.x(), -expectedTranslation.y());

    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());
    // No change expected.
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());
    // None of the scroll delta should have been used for document scroll.
    scrollInfo = m_hostImpl->processScrollDeltas();
    expectNone(*scrollInfo.get(), m_hostImpl->rootLayer()->id());
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportWithDeviceScaleFactor1)
{
    pinchZoomPanViewportTest(1);
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportWithDeviceScaleFactor2)
{
    pinchZoomPanViewportTest(2);
}

// This test verifies the correct behaviour of the document-then-pinchZoomViewport
// scrolling model, in both x- and y-directions.
void LayerTreeHostImplTest::pinchZoomPanViewportAndScrollTest(const float deviceScaleFactor)
{
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);

    gfx::Size layoutSurfaceSize(10, 20);
    gfx::Size deviceSurfaceSize(layoutSurfaceSize.width() * static_cast<int>(deviceScaleFactor),
                                layoutSurfaceSize.height() * static_cast<int>(deviceScaleFactor));
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, layoutSurfaceSize);
    // For this test we want to scrolls to move both the document and the
    // pinchZoomViewport so we can see some scroll component on the implTransform.
    root->setMaxScrollOffset(gfx::Vector2d(3, 4));
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(layoutSurfaceSize, deviceSurfaceSize);
    m_hostImpl->setPageScaleFactorAndLimits(1, 1, pageScale);
    initializeRendererAndDrawFrame();

    // Set new page scale on impl thread by pinching.
    m_hostImpl->pinchGestureBegin();
    m_hostImpl->pinchGestureUpdate(pageScale, gfx::Point());
    m_hostImpl->pinchGestureEnd();
    drawOneFrame();

    gfx::Transform expectedImplTransform;
    expectedImplTransform.Scale(pageScale, pageScale);

    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());

    // The implTransform ignores the scroll if !pageScalePinchZoomEnabled,
    // so no point in continuing without it.
    if (!m_hostImpl->settings().pageScalePinchZoomEnabled)
        return;

    // Scroll document only: scrollDelta chosen to move document horizontally
    // to its max scroll offset.
    gfx::Vector2d scrollDelta(3, 0);
    gfx::Vector2d scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    gfx::Vector2d expectedScrollDelta(scrollDelta);
    gfx::Vector2d expectedMaxScroll(m_hostImpl->rootLayer()->maxScrollOffset());
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    // The scroll delta is not scaled because the main thread did not scale.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());

    // Verify we did not change the implTransform this time.
    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());

    // Further scrolling should move the pinchZoomViewport only.
    scrollDelta = gfx::Vector2d(2, 0);
    scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    gfx::Vector2d expectedPanDelta(scrollDelta);
    gfx::Vector2dF expectedTranslation = gfx::ScaleVector2d(expectedPanDelta, m_hostImpl->deviceScaleFactor());
    expectedImplTransform.Translate(-expectedTranslation.x(), -expectedTranslation.y());

    EXPECT_EQ(m_hostImpl->rootLayer()->implTransform(), expectedImplTransform);

    // The scroll delta on the main thread should not have been affected by this.
    scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());

    // Perform same test sequence in y-direction also.
    // Document only scroll.
    scrollDelta = gfx::Vector2d(0, 4);
    scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    expectedScrollDelta += scrollDelta;
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    // The scroll delta is not scaled because the main thread did not scale.
    scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());

    // Verify we did not change the implTransform this time.
    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());

    // pinchZoomViewport scroll only.
    scrollDelta = gfx::Vector2d(0, 1);
    scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    expectedPanDelta = scrollDelta;
    expectedTranslation = gfx::ScaleVector2d(expectedPanDelta, m_hostImpl->deviceScaleFactor());
    expectedImplTransform.Translate(-expectedTranslation.x(), -expectedTranslation.y());

    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());

    // The scroll delta on the main thread should not have been affected by this.
    scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportAndScrollWithDeviceScaleFactor)
{
    pinchZoomPanViewportAndScrollTest(1);
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportAndScrollWithDeviceScaleFactor2)
{
    pinchZoomPanViewportAndScrollTest(2);
}

// This test verifies the correct behaviour of the document-then-pinchZoomViewport
// scrolling model, in both x- and y-directions, but this time using a single scroll
// that crosses the 'boundary' of what will cause document-only scroll and what will
// cause both document-scroll and zoomViewport panning.
void LayerTreeHostImplTest::pinchZoomPanViewportAndScrollBoundaryTest(const float deviceScaleFactor)
{
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);

    gfx::Size layoutSurfaceSize(10, 20);
    gfx::Size deviceSurfaceSize(layoutSurfaceSize.width() * static_cast<int>(deviceScaleFactor),
                                layoutSurfaceSize.height() * static_cast<int>(deviceScaleFactor));
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, layoutSurfaceSize);
    // For this test we want to scrolls to move both the document and the
    // pinchZoomViewport so we can see some scroll component on the implTransform.
    root->setMaxScrollOffset(gfx::Vector2d(3, 4));
    m_hostImpl->activeTree()->SetRootLayer(root.Pass());
    m_hostImpl->activeTree()->FindRootScrollLayer();
    m_hostImpl->setViewportSize(layoutSurfaceSize, deviceSurfaceSize);
    m_hostImpl->setPageScaleFactorAndLimits(1, 1, pageScale);
    initializeRendererAndDrawFrame();

    // Set new page scale on impl thread by pinching.
    m_hostImpl->pinchGestureBegin();
    m_hostImpl->pinchGestureUpdate(pageScale, gfx::Point());
    m_hostImpl->pinchGestureEnd();
    drawOneFrame();

    gfx::Transform expectedImplTransform;
    expectedImplTransform.Scale(pageScale, pageScale);

    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());

    // The implTransform ignores the scroll if !pageScalePinchZoomEnabled,
    // so no point in continuing without it.
    if (!m_hostImpl->settings().pageScalePinchZoomEnabled)
        return;

    // Scroll document and pann zoomViewport in one scroll-delta.
    gfx::Vector2d scrollDelta(5, 0);
    gfx::Vector2d scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    gfx::Vector2d expectedScrollDelta(gfx::Vector2d(3, 0)); // This component gets handled by document scroll.
    gfx::Vector2d expectedMaxScroll(m_hostImpl->rootLayer()->maxScrollOffset());

    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    // The scroll delta is not scaled because the main thread did not scale.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->processScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());

    gfx::Vector2d expectedPanDelta(2, 0); // This component gets handled by zoomViewport pan.
    gfx::Vector2dF expectedTranslation = gfx::ScaleVector2d(expectedPanDelta, m_hostImpl->deviceScaleFactor());
    expectedImplTransform.Translate(-expectedTranslation.x(), -expectedTranslation.y());

    EXPECT_EQ(m_hostImpl->rootLayer()->implTransform(), expectedImplTransform);

    // Perform same test sequence in y-direction also.
    scrollDelta = gfx::Vector2d(0, 5);
    scrollDeltaInZoomedViewport = ToFlooredVector2d(gfx::ScaleVector2d(scrollDelta, m_hostImpl->totalPageScaleFactorForTesting()));
    expectedScrollDelta += gfx::Vector2d(0, 4); // This component gets handled by document scroll.
    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->scrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
    m_hostImpl->scrollBy(gfx::Point(), scrollDeltaInZoomedViewport);
    m_hostImpl->scrollEnd();
    drawOneFrame();

    // The scroll delta is not scaled because the main thread did not scale.
    scrollInfo = m_hostImpl->processScrollDeltas(); // This component gets handled by zoomViewport pan.
    expectContains(*scrollInfo.get(), m_hostImpl->rootLayer()->id(), expectedScrollDelta);
    EXPECT_EQ(expectedMaxScroll, m_hostImpl->rootLayer()->maxScrollOffset());

    expectedPanDelta = gfx::Vector2d(0, 1);
    expectedTranslation = gfx::ScaleVector2d(expectedPanDelta, m_hostImpl->deviceScaleFactor());
    expectedImplTransform.Translate(-expectedTranslation.x(), -expectedTranslation.y());

    EXPECT_EQ(expectedImplTransform, m_hostImpl->rootLayer()->implTransform());
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportAndScrollBoundaryWithDeviceScaleFactor)
{
    pinchZoomPanViewportAndScrollBoundaryTest(1);
}

TEST_P(LayerTreeHostImplTest, pinchZoomPanViewportAndScrollBoundaryWithDeviceScaleFactor2)
{
    pinchZoomPanViewportAndScrollBoundaryTest(2);
}

class LayerTreeHostImplTestWithDelegatingRenderer : public LayerTreeHostImplTest {
protected:
    virtual scoped_ptr<OutputSurface> createOutputSurface()
    {
        // Creates an output surface with a parent to use a delegating renderer.
        WebKit::WebGraphicsContext3D::Attributes attrs;
        return FakeOutputSurface::CreateDelegating3d(FakeWebGraphicsContext3D::Create(attrs).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
    }

    void drawFrameAndTestDamage(const gfx::RectF& expectedDamage) {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
        ASSERT_EQ(1u, frame.renderPasses.size());

        // Verify the damage rect for the root render pass.
        const RenderPass* rootRenderPass = frame.renderPasses.back();
        EXPECT_RECT_EQ(expectedDamage, rootRenderPass->damage_rect);

        // Verify the root layer's quad is generated and not being culled.
        ASSERT_EQ(1u, rootRenderPass->quad_list.size());
        gfx::Rect expectedVisibleRect(m_hostImpl->rootLayer()->contentBounds());
        EXPECT_RECT_EQ(expectedVisibleRect, rootRenderPass->quad_list[0]->visible_rect);

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }
};

TEST_P(LayerTreeHostImplTestWithDelegatingRenderer, FrameIncludesDamageRect)
{
    // Draw a frame. In the first frame, the entire viewport should be damaged.
    gfx::Rect fullFrameDamage = gfx::Rect(m_hostImpl->deviceViewportSize());
    drawFrameAndTestDamage(fullFrameDamage);

    // The second frame should have no damage, but the quads should still be generated.
    gfx::Rect noDamage = gfx::Rect(m_hostImpl->deviceViewportSize());
    drawFrameAndTestDamage(noDamage);
}

class FakeMaskLayerImpl : public LayerImpl {
public:
    static scoped_ptr<FakeMaskLayerImpl> create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new FakeMaskLayerImpl(treeImpl, id));
    }

    virtual ResourceProvider::ResourceId contentsResourceId() const { return 0; }

private:
    FakeMaskLayerImpl(LayerTreeImpl* treeImpl, int id) : LayerImpl(treeImpl, id) { }
};

TEST_P(LayerTreeHostImplTest, maskLayerWithScaling)
{
    // Root
    //  |
    //  +-- Scaling Layer (adds a 2x scale)
    //       |
    //       +-- Content Layer
    //             +--Mask
    scoped_ptr<LayerImpl> scopedRoot = LayerImpl::create(m_hostImpl->activeTree(), 1);
    LayerImpl* root = scopedRoot.get();
    m_hostImpl->activeTree()->SetRootLayer(scopedRoot.Pass());

    scoped_ptr<LayerImpl> scopedScalingLayer = LayerImpl::create(m_hostImpl->activeTree(), 2);
    LayerImpl* scalingLayer = scopedScalingLayer.get();
    root->addChild(scopedScalingLayer.Pass());

    scoped_ptr<LayerImpl> scopedContentLayer = LayerImpl::create(m_hostImpl->activeTree(), 3);
    LayerImpl* contentLayer = scopedContentLayer.get();
    scalingLayer->addChild(scopedContentLayer.Pass());

    scoped_ptr<FakeMaskLayerImpl> scopedMaskLayer = FakeMaskLayerImpl::create(m_hostImpl->activeTree(), 4);
    FakeMaskLayerImpl* maskLayer = scopedMaskLayer.get();
    contentLayer->setMaskLayer(scopedMaskLayer.PassAs<LayerImpl>());

    gfx::Size rootSize(100, 100);
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setPosition(gfx::PointF());
    root->setAnchorPoint(gfx::PointF());

    gfx::Size scalingLayerSize(50, 50);
    scalingLayer->setBounds(scalingLayerSize);
    scalingLayer->setContentBounds(scalingLayerSize);
    scalingLayer->setPosition(gfx::PointF());
    scalingLayer->setAnchorPoint(gfx::PointF());
    gfx::Transform scale;
    scale.Scale(2.0, 2.0);
    scalingLayer->setTransform(scale);

    contentLayer->setBounds(scalingLayerSize);
    contentLayer->setContentBounds(scalingLayerSize);
    contentLayer->setPosition(gfx::PointF());
    contentLayer->setAnchorPoint(gfx::PointF());
    contentLayer->setDrawsContent(true);

    maskLayer->setBounds(scalingLayerSize);
    maskLayer->setContentBounds(scalingLayerSize);
    maskLayer->setPosition(gfx::PointF());
    maskLayer->setAnchorPoint(gfx::PointF());
    maskLayer->setDrawsContent(true);


    // Check that the tree scaling is correctly taken into account for the mask,
    // that should fully map onto the quad.
    float deviceScaleFactor = 1.f;
    m_hostImpl->setViewportSize(rootSize, rootSize);
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));

        ASSERT_EQ(1u, frame.renderPasses.size());
        ASSERT_EQ(1u, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }


    // Applying a DSF should change the render surface size, but won't affect
    // which part of the mask is used.
    deviceScaleFactor = 2.f;
    gfx::Size deviceViewport(gfx::ToFlooredSize(gfx::ScaleSize(rootSize, deviceScaleFactor)));
    m_hostImpl->setViewportSize(rootSize, deviceViewport);
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);
    m_hostImpl->setNeedsUpdateDrawProperties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));

        ASSERT_EQ(1u, frame.renderPasses.size());
        ASSERT_EQ(1u, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }


    // Applying an equivalent content scale on the content layer and the mask
    // should still result in the same part of the mask being used.
    gfx::Size contentsBounds(gfx::ToRoundedSize(gfx::ScaleSize(scalingLayerSize, deviceScaleFactor)));
    contentLayer->setContentBounds(contentsBounds);
    contentLayer->setContentsScale(deviceScaleFactor, deviceScaleFactor);
    maskLayer->setContentBounds(contentsBounds);
    maskLayer->setContentsScale(deviceScaleFactor, deviceScaleFactor);
    m_hostImpl->setNeedsUpdateDrawProperties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));

        ASSERT_EQ(1u, frame.renderPasses.size());
        ASSERT_EQ(1u, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }
}

TEST_P(LayerTreeHostImplTest, maskLayerWithDifferentBounds)
{
    // The mask layer has bounds 100x100 but is attached to a layer with bounds 50x50.

    scoped_ptr<LayerImpl> scopedRoot = LayerImpl::create(m_hostImpl->activeTree(), 1);
    LayerImpl* root = scopedRoot.get();
    m_hostImpl->activeTree()->SetRootLayer(scopedRoot.Pass());

    scoped_ptr<LayerImpl> scopedContentLayer = LayerImpl::create(m_hostImpl->activeTree(), 3);
    LayerImpl* contentLayer = scopedContentLayer.get();
    root->addChild(scopedContentLayer.Pass());

    scoped_ptr<FakeMaskLayerImpl> scopedMaskLayer = FakeMaskLayerImpl::create(m_hostImpl->activeTree(), 4);
    FakeMaskLayerImpl* maskLayer = scopedMaskLayer.get();
    contentLayer->setMaskLayer(scopedMaskLayer.PassAs<LayerImpl>());

    gfx::Size rootSize(100, 100);
    root->setBounds(rootSize);
    root->setContentBounds(rootSize);
    root->setPosition(gfx::PointF());
    root->setAnchorPoint(gfx::PointF());

    gfx::Size layerSize(50, 50);
    contentLayer->setBounds(layerSize);
    contentLayer->setContentBounds(layerSize);
    contentLayer->setPosition(gfx::PointF());
    contentLayer->setAnchorPoint(gfx::PointF());
    contentLayer->setDrawsContent(true);

    gfx::Size maskSize(100, 100);
    maskLayer->setBounds(maskSize);
    maskLayer->setContentBounds(maskSize);
    maskLayer->setPosition(gfx::PointF());
    maskLayer->setAnchorPoint(gfx::PointF());
    maskLayer->setDrawsContent(true);


    // Check that the mask fills the surface.
    float deviceScaleFactor = 1.f;
    m_hostImpl->setViewportSize(rootSize, rootSize);
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));

        ASSERT_EQ(1u, frame.renderPasses.size());
        ASSERT_EQ(1u, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }


    // Applying a DSF should change the render surface size, but won't affect
    // which part of the mask is used.
    deviceScaleFactor = 2.f;
    gfx::Size deviceViewport(gfx::ToFlooredSize(gfx::ScaleSize(rootSize, deviceScaleFactor)));
    m_hostImpl->setViewportSize(rootSize, deviceViewport);
    m_hostImpl->setDeviceScaleFactor(deviceScaleFactor);
    m_hostImpl->setNeedsUpdateDrawProperties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));

        ASSERT_EQ(1u, frame.renderPasses.size());
        ASSERT_EQ(1u, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }


    // Applying an equivalent content scale on the content layer and the mask
    // should still result in the same part of the mask being used.
    gfx::Size layerSizeLarge(gfx::ToRoundedSize(gfx::ScaleSize(layerSize, deviceScaleFactor)));
    contentLayer->setContentBounds(layerSizeLarge);
    contentLayer->setContentsScale(deviceScaleFactor, deviceScaleFactor);
    gfx::Size maskSizeLarge(gfx::ToRoundedSize(gfx::ScaleSize(maskSize, deviceScaleFactor)));
    maskLayer->setContentBounds(maskSizeLarge);
    maskLayer->setContentsScale(deviceScaleFactor, deviceScaleFactor);
    m_hostImpl->setNeedsUpdateDrawProperties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));

        ASSERT_EQ(1u, frame.renderPasses.size());
        ASSERT_EQ(1u, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }

    // Applying a different contents scale to the mask layer will still result
    // in the mask covering the owning layer.
    maskLayer->setContentBounds(maskSize);
    maskLayer->setContentsScale(deviceScaleFactor, deviceScaleFactor);
    m_hostImpl->setNeedsUpdateDrawProperties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));

        ASSERT_EQ(1u, frame.renderPasses.size());
        ASSERT_EQ(1u, frame.renderPasses[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.renderPasses[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.renderPasses[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->drawLayers(frame);
        m_hostImpl->didDrawAllLayers(frame);
    }
}

INSTANTIATE_TEST_CASE_P(LayerTreeHostImplTests,
                        LayerTreeHostImplTest,
                        ::testing::Values(false, true));

}  // namespace
}  // namespace cc
