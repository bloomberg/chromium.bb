// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <cmath>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "cc/base/math_util.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/io_surface_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/scrollbar_geometry_fixed_thumb.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/gl_renderer.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
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

class LayerTreeHostImplTest : public testing::Test,
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
        , m_didUploadVisibleTile(false)
        , m_reduceMemoryResult(true)
    {
        media::InitializeMediaLibraryForTesting();
    }

    virtual void SetUp()
    {
        LayerTreeSettings settings;
        settings.minimum_occlusion_tracking_size = gfx::Size();

        m_hostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);
        m_hostImpl->InitializeRenderer(createOutputSurface());
        m_hostImpl->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    }

    virtual void TearDown()
    {
    }

    virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE { }
    virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE { }
    virtual void OnVSyncParametersChanged(base::TimeTicks, base::TimeDelta) OVERRIDE { }
    virtual void OnCanDrawStateChanged(bool canDraw) OVERRIDE { m_onCanDrawStateChangedCalled = true; }
    virtual void OnHasPendingTreeStateChanged(bool hasPendingTree) OVERRIDE { m_hasPendingTree = hasPendingTree; }
    virtual void SetNeedsRedrawOnImplThread() OVERRIDE { m_didRequestRedraw = true; }
    virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE { m_didUploadVisibleTile = true; }
    virtual void SetNeedsCommitOnImplThread() OVERRIDE { m_didRequestCommit = true; }
    virtual void SetNeedsManageTilesOnImplThread() OVERRIDE { }
    virtual void PostAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime) OVERRIDE { }
    virtual bool ReduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE { return m_reduceMemoryResult; }
    virtual void ReduceWastedContentsTextureMemoryOnImplThread() OVERRIDE { }
    virtual void SendManagedMemoryStats() OVERRIDE { }
    virtual bool IsInsideDraw() OVERRIDE { return false; }
    virtual void RenewTreePriority() OVERRIDE { }
    virtual void RequestScrollbarAnimationOnImplThread(base::TimeDelta delay) OVERRIDE { }

    void setReduceMemoryResult(bool reduceMemoryResult) { m_reduceMemoryResult = reduceMemoryResult; }

    void createLayerTreeHost(bool partialSwap, scoped_ptr<OutputSurface> outputSurface)
    {
        LayerTreeSettings settings;
        settings.minimum_occlusion_tracking_size = gfx::Size();
        settings.partial_swap_enabled = partialSwap;

        m_hostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

        m_hostImpl->InitializeRenderer(outputSurface.Pass());
        m_hostImpl->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    }

    void setupRootLayerImpl(scoped_ptr<LayerImpl> root)
    {
        root->SetAnchorPoint(gfx::PointF(0, 0));
        root->SetPosition(gfx::PointF(0, 0));
        root->SetBounds(gfx::Size(10, 10));
        root->SetContentBounds(gfx::Size(10, 10));
        root->SetDrawsContent(true);
        root->draw_properties().visible_content_rect = gfx::Rect(0, 0, 10, 10);
        m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    }

    static void expectClearedScrollDeltasRecursive(LayerImpl* layer)
    {
        ASSERT_EQ(layer->scroll_delta(), gfx::Vector2d());
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
        scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
        root->SetScrollable(true);
        root->SetScrollOffset(gfx::Vector2d(0, 0));
        root->SetMaxScrollOffset(gfx::Vector2d(contentSize.width(), contentSize.height()));
        root->SetBounds(contentSize);
        root->SetContentBounds(contentSize);
        root->SetPosition(gfx::PointF(0, 0));
        root->SetAnchorPoint(gfx::PointF(0, 0));

        scoped_ptr<LayerImpl> contents = LayerImpl::Create(m_hostImpl->active_tree(), 2);
        contents->SetDrawsContent(true);
        contents->SetBounds(contentSize);
        contents->SetContentBounds(contentSize);
        contents->SetPosition(gfx::PointF(0, 0));
        contents->SetAnchorPoint(gfx::PointF(0, 0));
        root->AddChild(contents.Pass());
        m_hostImpl->active_tree()->SetRootLayer(root.Pass());
        m_hostImpl->active_tree()->DidBecomeActive();
    }

    scoped_ptr<LayerImpl> createScrollableLayer(int id, const gfx::Size& size)
    {
        scoped_ptr<LayerImpl> layer = LayerImpl::Create(m_hostImpl->active_tree(), id);
        layer->SetScrollable(true);
        layer->SetDrawsContent(true);
        layer->SetBounds(size);
        layer->SetContentBounds(size);
        layer->SetMaxScrollOffset(gfx::Vector2d(size.width() * 2, size.height() * 2));
        return layer.Pass();
    }

    void initializeRendererAndDrawFrame()
    {
        m_hostImpl->InitializeRenderer(createOutputSurface());
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }

    void pinchZoomPanViewportForcesCommitRedraw(const float deviceScaleFactor);
    void pinchZoomPanViewportTest(const float deviceScaleFactor);
    void pinchZoomPanViewportAndScrollTest(const float deviceScaleFactor);
    void pinchZoomPanViewportAndScrollBoundaryTest(const float deviceScaleFactor);

protected:
    virtual scoped_ptr<OutputSurface> createOutputSurface() { return CreateFakeOutputSurface(); }

    void drawOneFrame() {
      LayerTreeHostImpl::FrameData frameData;
      m_hostImpl->PrepareToDraw(&frameData);
      m_hostImpl->DidDrawAllLayers(frameData);
    }

    FakeProxy m_proxy;
    DebugScopedSetImplThread m_alwaysImplThread;
    DebugScopedSetMainThreadBlocked m_alwaysMainThreadBlocked;

    scoped_ptr<LayerTreeHostImpl> m_hostImpl;
    FakeRenderingStatsInstrumentation m_statsInstrumentation;
    bool m_onCanDrawStateChangedCalled;
    bool m_hasPendingTree;
    bool m_didRequestCommit;
    bool m_didRequestRedraw;
    bool m_didUploadVisibleTile;
    bool m_reduceMemoryResult;
};

class TestWebGraphicsContext3DMakeCurrentFails : public TestWebGraphicsContext3D {
public:
    virtual bool makeContextCurrent() { return false; }
};

TEST_F(LayerTreeHostImplTest, notifyIfCanDrawChanged)
{
    // Note: It is not possible to disable the renderer once it has been set,
    // so we do not need to test that disabling the renderer notifies us
    // that canDraw changed.
    EXPECT_FALSE(m_hostImpl->CanDraw());
    m_onCanDrawStateChangedCalled = false;

    setupScrollAndContentsLayers(gfx::Size(100, 100));
    EXPECT_TRUE(m_hostImpl->CanDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle the root layer to make sure it toggles canDraw
    m_hostImpl->active_tree()->SetRootLayer(scoped_ptr<LayerImpl>());
    EXPECT_FALSE(m_hostImpl->CanDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    setupScrollAndContentsLayers(gfx::Size(100, 100));
    EXPECT_TRUE(m_hostImpl->CanDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle the device viewport size to make sure it toggles canDraw.
    m_hostImpl->SetViewportSize(gfx::Size(100, 100), gfx::Size(0, 0));
    EXPECT_FALSE(m_hostImpl->CanDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    m_hostImpl->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));
    EXPECT_TRUE(m_hostImpl->CanDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle contents textures purged without causing any evictions,
    // and make sure that it does not change canDraw.
    setReduceMemoryResult(false);
    m_hostImpl->SetManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memory_allocation_limit_bytes() - 1));
    EXPECT_TRUE(m_hostImpl->CanDraw());
    EXPECT_FALSE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    // Toggle contents textures purged to make sure it toggles canDraw.
    setReduceMemoryResult(true);
    m_hostImpl->SetManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memory_allocation_limit_bytes() - 1));
    EXPECT_FALSE(m_hostImpl->CanDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;

    m_hostImpl->active_tree()->ResetContentsTexturesPurged();
    EXPECT_TRUE(m_hostImpl->CanDraw());
    EXPECT_TRUE(m_onCanDrawStateChangedCalled);
    m_onCanDrawStateChangedCalled = false;
}

TEST_F(LayerTreeHostImplTest, scrollDeltaNoLayers)
{
    ASSERT_FALSE(m_hostImpl->active_tree()->root_layer());

    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 0u);
}

TEST_F(LayerTreeHostImplTest, scrollDeltaTreeButNoChanges)
{
    {
        scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
        root->AddChild(LayerImpl::Create(m_hostImpl->active_tree(), 2));
        root->AddChild(LayerImpl::Create(m_hostImpl->active_tree(), 3));
        root->children()[1]->AddChild(LayerImpl::Create(m_hostImpl->active_tree(), 4));
        root->children()[1]->AddChild(LayerImpl::Create(m_hostImpl->active_tree(), 5));
        root->children()[1]->children()[0]->AddChild(LayerImpl::Create(m_hostImpl->active_tree(), 6));
        m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    }
    LayerImpl* root = m_hostImpl->active_tree()->root_layer();

    expectClearedScrollDeltasRecursive(root);

    scoped_ptr<ScrollAndScaleSet> scrollInfo;

    scrollInfo = m_hostImpl->ProcessScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 0u);
    expectClearedScrollDeltasRecursive(root);

    scrollInfo = m_hostImpl->ProcessScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 0u);
    expectClearedScrollDeltasRecursive(root);
}

TEST_F(LayerTreeHostImplTest, scrollDeltaRepeatedScrolls)
{
    gfx::Vector2d scrollOffset(20, 30);
    gfx::Vector2d scrollDelta(11, -15);
    {
        scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
        root->SetScrollOffset(scrollOffset);
        root->SetScrollable(true);
        root->SetMaxScrollOffset(gfx::Vector2d(100, 100));
        root->ScrollBy(scrollDelta);
        m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    }
    LayerImpl* root = m_hostImpl->active_tree()->root_layer();

    scoped_ptr<ScrollAndScaleSet> scrollInfo;

    scrollInfo = m_hostImpl->ProcessScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 1u);
    EXPECT_VECTOR_EQ(root->sent_scroll_delta(), scrollDelta);
    expectContains(*scrollInfo, root->id(), scrollDelta);

    gfx::Vector2d scrollDelta2(-5, 27);
    root->ScrollBy(scrollDelta2);
    scrollInfo = m_hostImpl->ProcessScrollDeltas();
    ASSERT_EQ(scrollInfo->scrolls.size(), 1u);
    EXPECT_VECTOR_EQ(root->sent_scroll_delta(), scrollDelta + scrollDelta2);
    expectContains(*scrollInfo, root->id(), scrollDelta + scrollDelta2);

    root->ScrollBy(gfx::Vector2d());
    scrollInfo = m_hostImpl->ProcessScrollDeltas();
    EXPECT_EQ(root->sent_scroll_delta(), scrollDelta + scrollDelta2);
}

TEST_F(LayerTreeHostImplTest, scrollRootCallsCommitAndRedraw)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->ScrollEnd();
    EXPECT_TRUE(m_didRequestRedraw);
    EXPECT_TRUE(m_didRequestCommit);
}

TEST_F(LayerTreeHostImplTest, scrollWithoutRootLayer)
{
    // We should not crash when trying to scroll an empty layer tree.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
}

TEST_F(LayerTreeHostImplTest, scrollWithoutRenderer)
{
    LayerTreeSettings settings;
    m_hostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

    // Initialization will fail here.
    m_hostImpl->InitializeRenderer(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new TestWebGraphicsContext3DMakeCurrentFails)).PassAs<OutputSurface>());
    m_hostImpl->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));

    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // We should not crash when trying to scroll after the renderer initialization fails.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
}

TEST_F(LayerTreeHostImplTest, replaceTreeWhileScrolling)
{
    const int scrollLayerId = 1;

    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    // We should not crash if the tree is replaced while we are scrolling.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->active_tree()->DetachLayerTree();

    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // We should still be scrolling, because the scrolled layer also exists in the new tree.
    gfx::Vector2d scrollDelta(0, 10);
    m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->ScrollEnd();
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo, scrollLayerId, scrollDelta);
}

TEST_F(LayerTreeHostImplTest, clearRootRenderSurfaceAndScroll)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    // We should be able to scroll even if the root layer loses its render surface after the most
    // recent render.
    m_hostImpl->active_tree()->root_layer()->ClearRenderSurface();
    m_hostImpl->active_tree()->set_needs_update_draw_properties();

    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
}

TEST_F(LayerTreeHostImplTest, wheelEventHandlers)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();
    LayerImpl* root = m_hostImpl->active_tree()->root_layer();

    root->SetHaveWheelEventHandlers(true);

    // With registered event handlers, wheel scrolls have to go to the main thread.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);

    // But gesture scrolls can still be handled.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
}

TEST_F(LayerTreeHostImplTest, shouldScrollOnMainThread)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();
    LayerImpl* root = m_hostImpl->active_tree()->root_layer();

    root->SetShouldScrollOnMainThread(true);

    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollOnMainThread);
}

TEST_F(LayerTreeHostImplTest, nonFastScrollableRegionBasic)
{
    setupScrollAndContentsLayers(gfx::Size(200, 200));
    m_hostImpl->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

    LayerImpl* root = m_hostImpl->active_tree()->root_layer();
    root->SetContentsScale(2, 2);
    root->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));

    initializeRendererAndDrawFrame();

    // All scroll types inside the non-fast scrollable region should fail.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(25, 25), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(25, 25), InputHandlerClient::Gesture), InputHandlerClient::ScrollOnMainThread);

    // All scroll types outside this region should succeed.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(75, 75), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->ScrollEnd();
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(75, 75), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->ScrollEnd();
}

TEST_F(LayerTreeHostImplTest, nonFastScrollableRegionWithOffset)
{
    setupScrollAndContentsLayers(gfx::Size(200, 200));
    m_hostImpl->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

    LayerImpl* root = m_hostImpl->active_tree()->root_layer();
    root->SetContentsScale(2, 2);
    root->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));
    root->SetPosition(gfx::PointF(-25, 0));

    initializeRendererAndDrawFrame();

    // This point would fall into the non-fast scrollable region except that we've moved the layer down by 25 pixels.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(40, 10), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 1));
    m_hostImpl->ScrollEnd();

    // This point is still inside the non-fast region.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(10, 10), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
}

TEST_F(LayerTreeHostImplTest, scrollByReturnsCorrectValue)
{
    setupScrollAndContentsLayers(gfx::Size(200, 200));
    m_hostImpl->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

    initializeRendererAndDrawFrame();

    EXPECT_EQ(InputHandlerClient::ScrollStarted,
        m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));

    // Trying to scroll to the left/top will not succeed.
    EXPECT_FALSE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
    EXPECT_FALSE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, -10)));
    EXPECT_FALSE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(-10, -10)));

    // Scrolling to the right/bottom will succeed.
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(10, 0)));
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10)));
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(10, 10)));

    // Scrolling to left/top will now succeed.
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, -10)));
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(-10, -10)));

    // Scrolling diagonally against an edge will succeed.
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(10, -10)));
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 10)));

    // Trying to scroll more than the available space will also succeed.
    EXPECT_TRUE(m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(5000, 5000)));
}

TEST_F(LayerTreeHostImplTest, clearRootRenderSurfaceAndHitTestTouchHandlerRegion)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    // We should be able to hit test for touch event handlers even if the root layer loses
    // its render surface after the most recent render.
    m_hostImpl->active_tree()->root_layer()->ClearRenderSurface();
    m_hostImpl->active_tree()->set_needs_update_draw_properties();

    EXPECT_EQ(m_hostImpl->HaveTouchEventHandlersAt(gfx::Point(0, 0)), false);
}

TEST_F(LayerTreeHostImplTest, implPinchZoom)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->RootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = 1, maxPageScale = 4;
    const gfx::Transform identityScaleTransform;

    // The impl-based pinch zoom should adjust the max scroll position.
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        m_hostImpl->active_tree()->SetPageScaleDelta(1);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollDelta(gfx::Vector2d());

        float pageScaleDelta = 2;
        m_hostImpl->PinchGestureBegin();
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(50, 50));
        m_hostImpl->PinchGestureEnd();
        EXPECT_TRUE(m_didRequestRedraw);
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);

        EXPECT_EQ(gfx::Vector2d(75, 75), m_hostImpl->active_tree()->root_layer()->max_scroll_offset());
    }

    // Scrolling after a pinch gesture should always be in local space.  The scroll deltas do not
    // have the page scale factor applied.
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        m_hostImpl->active_tree()->SetPageScaleDelta(1);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollDelta(gfx::Vector2d());

        float pageScaleDelta = 2;
        m_hostImpl->PinchGestureBegin();
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(0, 0));
        m_hostImpl->PinchGestureEnd();

        gfx::Vector2d scrollDelta(0, 10);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
        m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->ScrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), scrollDelta);
    }
}

TEST_F(LayerTreeHostImplTest, pinchGesture)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->RootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = 1;
    const float maxPageScale = 4;
    const gfx::Transform identityScaleTransform;

    // Basic pinch zoom in gesture
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollDelta(gfx::Vector2d());

        float pageScaleDelta = 2;
        m_hostImpl->PinchGestureBegin();
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(50, 50));
        m_hostImpl->PinchGestureEnd();
        EXPECT_TRUE(m_didRequestRedraw);
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
    }

    // Zoom-in clamping
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollDelta(gfx::Vector2d());
        float pageScaleDelta = 10;

        m_hostImpl->PinchGestureBegin();
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(50, 50));
        m_hostImpl->PinchGestureEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, maxPageScale);
    }

    // Zoom-out clamping
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollDelta(gfx::Vector2d());
        scrollLayer->SetScrollOffset(gfx::Vector2d(50, 50));

        float pageScaleDelta = 0.1f;
        m_hostImpl->PinchGestureBegin();
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(0, 0));
        m_hostImpl->PinchGestureEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, minPageScale);

        EXPECT_TRUE(scrollInfo->scrolls.empty());
    }

    // Two-finger panning should not happen based on pinch events only
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollDelta(gfx::Vector2d());
        scrollLayer->SetScrollOffset(gfx::Vector2d(20, 20));

        float pageScaleDelta = 1;
        m_hostImpl->PinchGestureBegin();
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(10, 10));
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(20, 20));
        m_hostImpl->PinchGestureEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
        EXPECT_TRUE(scrollInfo->scrolls.empty());
    }

    // Two-finger panning should work with interleaved scroll events
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollDelta(gfx::Vector2d());
        scrollLayer->SetScrollOffset(gfx::Vector2d(20, 20));

        float pageScaleDelta = 1;
        m_hostImpl->ScrollBegin(gfx::Point(10, 10), InputHandlerClient::Wheel);
        m_hostImpl->PinchGestureBegin();
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(10, 10));
        m_hostImpl->ScrollBy(gfx::Point(10, 10), gfx::Vector2d(-10, -10));
        m_hostImpl->PinchGestureUpdate(pageScaleDelta, gfx::Point(20, 20));
        m_hostImpl->PinchGestureEnd();
        m_hostImpl->ScrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, pageScaleDelta);
        expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(-10, -10));
    }
}

TEST_F(LayerTreeHostImplTest, pageScaleAnimation)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->RootScrollLayer();
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
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollOffset(gfx::Vector2d(50, 50));

        m_hostImpl->StartPageScaleAnimation(gfx::Vector2d(0, 0), false, 2, startTime, duration);
        m_hostImpl->Animate(halfwayThroughAnimation, base::Time());
        EXPECT_TRUE(m_didRequestRedraw);
        m_hostImpl->Animate(endTime, base::Time());
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, 2);
        expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(-50, -50));
    }

    // Anchor zoom-out
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollOffset(gfx::Vector2d(50, 50));

        m_hostImpl->StartPageScaleAnimation(gfx::Vector2d(25, 25), true, minPageScale, startTime, duration);
        m_hostImpl->Animate(endTime, base::Time());
        EXPECT_TRUE(m_didRequestRedraw);
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, minPageScale);
        // Pushed to (0,0) via clamping against contents layer size.
        expectContains(*scrollInfo, scrollLayer->id(), gfx::Vector2d(-50, -50));
    }
}

TEST_F(LayerTreeHostImplTest, pageScaleAnimationNoOp)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    initializeRendererAndDrawFrame();

    LayerImpl* scrollLayer = m_hostImpl->RootScrollLayer();
    DCHECK(scrollLayer);

    const float minPageScale = 0.5;
    const float maxPageScale = 4;
    const base::TimeTicks startTime = base::TimeTicks() + base::TimeDelta::FromSeconds(1);
    const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
    const base::TimeTicks halfwayThroughAnimation = startTime + duration / 2;
    const base::TimeTicks endTime = startTime + duration;
    const gfx::Transform identityScaleTransform;

    // Anchor zoom with unchanged page scale should not change scroll or scale.
    {
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, minPageScale, maxPageScale);
        scrollLayer->SetImplTransform(identityScaleTransform);
        scrollLayer->SetScrollOffset(gfx::Vector2d(50, 50));

        m_hostImpl->StartPageScaleAnimation(gfx::Vector2d(0, 0), true, 1, startTime, duration);
        m_hostImpl->Animate(halfwayThroughAnimation, base::Time());
        EXPECT_TRUE(m_didRequestRedraw);
        m_hostImpl->Animate(endTime, base::Time());
        EXPECT_TRUE(m_didRequestCommit);

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        EXPECT_EQ(scrollInfo->pageScaleDelta, 1);
        expectNone(*scrollInfo, scrollLayer->id());
    }
}

TEST_F(LayerTreeHostImplTest, compositorFrameMetadata)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    m_hostImpl->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
    m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1.0f, 0.5f, 4.0f);
    initializeRendererAndDrawFrame();

    {
        CompositorFrameMetadata metadata = m_hostImpl->MakeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 0.0f), metadata.root_scroll_offset);
        EXPECT_EQ(1.0f, metadata.page_scale_factor);
        EXPECT_EQ(gfx::SizeF(50.0f, 50.0f), metadata.viewport_size);
        EXPECT_EQ(gfx::SizeF(100.0f, 100.0f), metadata.root_layer_size);
        EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
        EXPECT_EQ(4.0f, metadata.max_page_scale_factor);
    }

    // Scrolling should update metadata immediately.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    {
        CompositorFrameMetadata metadata = m_hostImpl->MakeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 10.0f), metadata.root_scroll_offset);
    }
    m_hostImpl->ScrollEnd();

    {
        CompositorFrameMetadata metadata = m_hostImpl->MakeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 10.0f), metadata.root_scroll_offset);
    }

    // Page scale should update metadata correctly (shrinking only the viewport).
    m_hostImpl->PinchGestureBegin();
    m_hostImpl->PinchGestureUpdate(2.0f, gfx::Point(0, 0));
    m_hostImpl->PinchGestureEnd();

    {
        CompositorFrameMetadata metadata = m_hostImpl->MakeCompositorFrameMetadata();
        EXPECT_EQ(gfx::Vector2dF(0.0f, 10.0f), metadata.root_scroll_offset);
        EXPECT_EQ(2, metadata.page_scale_factor);
        EXPECT_EQ(gfx::SizeF(25.0f, 25.0f), metadata.viewport_size);
        EXPECT_EQ(gfx::SizeF(100.0f, 100.0f), metadata.root_layer_size);
        EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
        EXPECT_EQ(4.0f, metadata.max_page_scale_factor);
    }

    // Likewise if set from the main thread.
    m_hostImpl->ProcessScrollDeltas();
    m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(4.0f, 0.5f, 4.0f);
    m_hostImpl->active_tree()->SetPageScaleDelta(1.0f);
    {
        CompositorFrameMetadata metadata = m_hostImpl->MakeCompositorFrameMetadata();
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
    static scoped_ptr<LayerImpl> Create(LayerTreeImpl* treeImpl, int id) { return scoped_ptr<LayerImpl>(new DidDrawCheckLayer(treeImpl, id)); }

    virtual void DidDraw(ResourceProvider*) OVERRIDE
    {
        m_didDrawCalled = true;
    }

    virtual void WillDraw(ResourceProvider*) OVERRIDE
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
        SetAnchorPoint(gfx::PointF(0, 0));
        SetBounds(gfx::Size(10, 10));
        SetContentBounds(gfx::Size(10, 10));
        SetDrawsContent(true);
        set_skips_draw(false);
        draw_properties().visible_content_rect = gfx::Rect(0, 0, 10, 10);

        scoped_ptr<LayerTilingData> tiler = LayerTilingData::Create(gfx::Size(100, 100), LayerTilingData::HAS_BORDER_TEXELS);
        tiler->SetBounds(content_bounds());
        SetTilingData(*tiler.get());
    }

private:
    bool m_didDrawCalled;
    bool m_willDrawCalled;
};

TEST_F(LayerTreeHostImplTest, didDrawNotCalledOnHiddenLayer)
{
    // The root layer is always drawn, so run this test on a child layer that
    // will be masked out by the root layer's bounds.
    m_hostImpl->active_tree()->SetRootLayer(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->active_tree()->root_layer());
    root->SetMasksToBounds(true);

    root->AddChild(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 2));
    DidDrawCheckLayer* layer = static_cast<DidDrawCheckLayer*>(root->children()[0]);
    // Ensure visibleContentRect for layer is empty
    layer->SetPosition(gfx::PointF(100, 100));
    layer->SetBounds(gfx::Size(10, 10));
    layer->SetContentBounds(gfx::Size(10, 10));

    LayerTreeHostImpl::FrameData frame;

    EXPECT_FALSE(layer->willDrawCalled());
    EXPECT_FALSE(layer->didDrawCalled());

    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    EXPECT_FALSE(layer->willDrawCalled());
    EXPECT_FALSE(layer->didDrawCalled());

    EXPECT_TRUE(layer->visible_content_rect().IsEmpty());

    // Ensure visibleContentRect for layer layer is not empty
    layer->SetPosition(gfx::PointF(0, 0));

    EXPECT_FALSE(layer->willDrawCalled());
    EXPECT_FALSE(layer->didDrawCalled());

    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    EXPECT_TRUE(layer->willDrawCalled());
    EXPECT_TRUE(layer->didDrawCalled());

    EXPECT_FALSE(layer->visible_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostImplTest, willDrawNotCalledOnOccludedLayer)
{
    gfx::Size bigSize(1000, 1000);
    m_hostImpl->SetViewportSize(bigSize, bigSize);

    m_hostImpl->active_tree()->SetRootLayer(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->active_tree()->root_layer());

    root->AddChild(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 2));
    DidDrawCheckLayer* occludedLayer = static_cast<DidDrawCheckLayer*>(root->children()[0]);

    root->AddChild(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 3));
    DidDrawCheckLayer* topLayer = static_cast<DidDrawCheckLayer*>(root->children()[1]);
    // This layer covers the occludedLayer above. Make this layer large so it can occlude.
    topLayer->SetBounds(bigSize);
    topLayer->SetContentBounds(bigSize);
    topLayer->SetContentsOpaque(true);

    LayerTreeHostImpl::FrameData frame;

    EXPECT_FALSE(occludedLayer->willDrawCalled());
    EXPECT_FALSE(occludedLayer->didDrawCalled());
    EXPECT_FALSE(topLayer->willDrawCalled());
    EXPECT_FALSE(topLayer->didDrawCalled());

    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    EXPECT_FALSE(occludedLayer->willDrawCalled());
    EXPECT_FALSE(occludedLayer->didDrawCalled());
    EXPECT_TRUE(topLayer->willDrawCalled());
    EXPECT_TRUE(topLayer->didDrawCalled());
}

TEST_F(LayerTreeHostImplTest, didDrawCalledOnAllLayers)
{
    m_hostImpl->active_tree()->SetRootLayer(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->active_tree()->root_layer());

    root->AddChild(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 2));
    DidDrawCheckLayer* layer1 = static_cast<DidDrawCheckLayer*>(root->children()[0]);

    layer1->AddChild(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 3));
    DidDrawCheckLayer* layer2 = static_cast<DidDrawCheckLayer*>(layer1->children()[0]);

    layer1->SetOpacity(0.3f);
    layer1->SetPreserves3d(false);

    EXPECT_FALSE(root->didDrawCalled());
    EXPECT_FALSE(layer1->didDrawCalled());
    EXPECT_FALSE(layer2->didDrawCalled());

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    EXPECT_TRUE(root->didDrawCalled());
    EXPECT_TRUE(layer1->didDrawCalled());
    EXPECT_TRUE(layer2->didDrawCalled());

    EXPECT_NE(root->render_surface(), layer1->render_surface());
    EXPECT_TRUE(!!layer1->render_surface());
}

class MissingTextureAnimatingLayer : public DidDrawCheckLayer {
public:
    static scoped_ptr<LayerImpl> Create(LayerTreeImpl* treeImpl, int id, bool tileMissing, bool skipsDraw, bool animating, ResourceProvider* resourceProvider)
    {
        return scoped_ptr<LayerImpl>(new MissingTextureAnimatingLayer(treeImpl, id, tileMissing, skipsDraw, animating, resourceProvider));
    }

private:
    MissingTextureAnimatingLayer(LayerTreeImpl* treeImpl, int id, bool tileMissing, bool skipsDraw, bool animating, ResourceProvider* resourceProvider)
        : DidDrawCheckLayer(treeImpl, id)
    {
        scoped_ptr<LayerTilingData> tilingData = LayerTilingData::Create(gfx::Size(10, 10), LayerTilingData::NO_BORDER_TEXELS);
        tilingData->SetBounds(bounds());
        SetTilingData(*tilingData.get());
        set_skips_draw(skipsDraw);
        if (!tileMissing) {
            ResourceProvider::ResourceId resource = resourceProvider->CreateResource(gfx::Size(), GL_RGBA, ResourceProvider::TextureUsageAny);
            resourceProvider->AllocateForTesting(resource);
            PushTileProperties(0, 0, resource, gfx::Rect(), false);
        }
        if (animating)
            AddAnimatedTransformToLayer(this, 10, 3, 0);
    }
};

TEST_F(LayerTreeHostImplTest, prepareToDrawFailsWhenAnimationUsesCheckerboard)
{
    // When the texture is not missing, we draw as usual.
    m_hostImpl->active_tree()->SetRootLayer(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 1));
    DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(m_hostImpl->active_tree()->root_layer());
    root->AddChild(MissingTextureAnimatingLayer::Create(m_hostImpl->active_tree(), 2, false, false, true, m_hostImpl->resource_provider()));

    LayerTreeHostImpl::FrameData frame;

    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    // When a texture is missing and we're not animating, we draw as usual with checkerboarding.
    m_hostImpl->active_tree()->SetRootLayer(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 3));
    root = static_cast<DidDrawCheckLayer*>(m_hostImpl->active_tree()->root_layer());
    root->AddChild(MissingTextureAnimatingLayer::Create(m_hostImpl->active_tree(), 4, true, false, false, m_hostImpl->resource_provider()));

    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    // When a texture is missing and we're animating, we don't want to draw anything.
    m_hostImpl->active_tree()->SetRootLayer(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 5));
    root = static_cast<DidDrawCheckLayer*>(m_hostImpl->active_tree()->root_layer());
    root->AddChild(MissingTextureAnimatingLayer::Create(m_hostImpl->active_tree(), 6, true, false, true, m_hostImpl->resource_provider()));

    EXPECT_FALSE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    // When the layer skips draw and we're animating, we still draw the frame.
    m_hostImpl->active_tree()->SetRootLayer(DidDrawCheckLayer::Create(m_hostImpl->active_tree(), 7));
    root = static_cast<DidDrawCheckLayer*>(m_hostImpl->active_tree()->root_layer());
    root->AddChild(MissingTextureAnimatingLayer::Create(m_hostImpl->active_tree(), 8, false, true, true, m_hostImpl->resource_provider()));

    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);
}

TEST_F(LayerTreeHostImplTest, scrollRootIgnored)
{
    scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    root->SetScrollable(false);
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    initializeRendererAndDrawFrame();

    // Scroll event is ignored because layer is not scrollable.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
    EXPECT_FALSE(m_didRequestRedraw);
    EXPECT_FALSE(m_didRequestCommit);
}

TEST_F(LayerTreeHostImplTest, scrollNonScrollableRootWithTopControls)
{
    LayerTreeSettings settings;
    settings.calculate_top_controls_position = true;
    settings.top_controls_height = 50;

    m_hostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);
    m_hostImpl->InitializeRenderer(createOutputSurface());
    m_hostImpl->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));

    gfx::Size layerSize(5, 5);
    scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    root->SetScrollable(true);
    root->SetMaxScrollOffset(gfx::Vector2d(layerSize.width(), layerSize.height()));
    root->SetBounds(layerSize);
    root->SetContentBounds(layerSize);
    root->SetPosition(gfx::PointF(0, 0));
    root->SetAnchorPoint(gfx::PointF(0, 0));
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->active_tree()->FindRootScrollLayer();
    initializeRendererAndDrawFrame();

    EXPECT_EQ(InputHandlerClient::ScrollIgnored, m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));

    m_hostImpl->top_controls_manager()->ScrollBegin();
    m_hostImpl->top_controls_manager()->ScrollBy(gfx::Vector2dF(0, 50));
    m_hostImpl->top_controls_manager()->ScrollEnd();
    EXPECT_EQ(m_hostImpl->top_controls_manager()->content_top_offset(), 0.f);

    EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture));
}

TEST_F(LayerTreeHostImplTest, scrollNonCompositedRoot)
{
    // Test the configuration where a non-composited root layer is embedded in a
    // scrollable outer layer.
    gfx::Size surfaceSize(10, 10);

    scoped_ptr<LayerImpl> contentLayer = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    contentLayer->SetDrawsContent(true);
    contentLayer->SetPosition(gfx::PointF(0, 0));
    contentLayer->SetAnchorPoint(gfx::PointF(0, 0));
    contentLayer->SetBounds(surfaceSize);
    contentLayer->SetContentBounds(gfx::Size(surfaceSize.width() * 2, surfaceSize.height() * 2));
    contentLayer->SetContentsScale(2, 2);

    scoped_ptr<LayerImpl> scrollLayer = LayerImpl::Create(m_hostImpl->active_tree(), 2);
    scrollLayer->SetScrollable(true);
    scrollLayer->SetMaxScrollOffset(gfx::Vector2d(surfaceSize.width(), surfaceSize.height()));
    scrollLayer->SetBounds(surfaceSize);
    scrollLayer->SetContentBounds(surfaceSize);
    scrollLayer->SetPosition(gfx::PointF(0, 0));
    scrollLayer->SetAnchorPoint(gfx::PointF(0, 0));
    scrollLayer->AddChild(contentLayer.Pass());

    m_hostImpl->active_tree()->SetRootLayer(scrollLayer.Pass());
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->ScrollEnd();
    EXPECT_TRUE(m_didRequestRedraw);
    EXPECT_TRUE(m_didRequestCommit);
}

TEST_F(LayerTreeHostImplTest, scrollChildCallsCommitAndRedraw)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    root->SetBounds(surfaceSize);
    root->SetContentBounds(surfaceSize);
    root->AddChild(createScrollableLayer(2, surfaceSize));
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
    m_hostImpl->ScrollEnd();
    EXPECT_TRUE(m_didRequestRedraw);
    EXPECT_TRUE(m_didRequestCommit);
}

TEST_F(LayerTreeHostImplTest, scrollMissesChild)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    root->AddChild(createScrollableLayer(2, surfaceSize));
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scroll event is ignored because the input coordinate is outside the layer boundaries.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(15, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
    EXPECT_FALSE(m_didRequestRedraw);
    EXPECT_FALSE(m_didRequestCommit);
}

TEST_F(LayerTreeHostImplTest, scrollMissesBackfacingChild)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    scoped_ptr<LayerImpl> child = createScrollableLayer(2, surfaceSize);
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);

    gfx::Transform matrix;
    matrix.RotateAboutXAxis(180);
    child->SetTransform(matrix);
    child->SetDoubleSided(false);

    root->AddChild(child.Pass());
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    initializeRendererAndDrawFrame();

    // Scroll event is ignored because the scrollable layer is not facing the viewer and there is
    // nothing scrollable behind it.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollIgnored);
    EXPECT_FALSE(m_didRequestRedraw);
    EXPECT_FALSE(m_didRequestCommit);
}

TEST_F(LayerTreeHostImplTest, scrollBlockedByContentLayer)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> contentLayer = createScrollableLayer(1, surfaceSize);
    contentLayer->SetShouldScrollOnMainThread(true);
    contentLayer->SetScrollable(false);

    scoped_ptr<LayerImpl> scrollLayer = createScrollableLayer(2, surfaceSize);
    scrollLayer->AddChild(contentLayer.Pass());

    m_hostImpl->active_tree()->SetRootLayer(scrollLayer.Pass());
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scrolling fails because the content layer is asking to be scrolled on the main thread.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollOnMainThread);
}

TEST_F(LayerTreeHostImplTest, scrollRootAndChangePageScaleOnMainThread)
{
    gfx::Size surfaceSize(10, 10);
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->active_tree()->DidBecomeActive();
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    gfx::Vector2d scrollDelta(0, 10);
    gfx::Vector2d expectedScrollDelta(scrollDelta);
    gfx::Vector2d expectedMaxScroll(m_hostImpl->active_tree()->root_layer()->max_scroll_offset());
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->ScrollEnd();

    // Set new page scale from main thread.
    m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(pageScale, pageScale, pageScale);

    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), expectedScrollDelta);

    // The scroll range should also have been updated.
    EXPECT_EQ(m_hostImpl->active_tree()->root_layer()->max_scroll_offset(), expectedMaxScroll);

    // The page scale delta remains constant because the impl thread did not scale.
    // TODO: If possible, use gfx::Transform() or Skia equality functions. At
    //       the moment we avoid that because skia does exact bit-wise equality
    //       checking that does not consider -0 == +0.
    //       http://code.google.com/p/chromium/issues/detail?id=162747
    EXPECT_EQ(1.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(0, 0));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(0, 1));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(0, 2));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(0, 3));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(1, 0));
    EXPECT_EQ(1.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(1, 1));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(1, 2));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(1, 3));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(2, 0));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(2, 1));
    EXPECT_EQ(1.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(2, 2));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(2, 3));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(3, 0));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(3, 1));
    EXPECT_EQ(0.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(3, 2));
    EXPECT_EQ(1.0, m_hostImpl->active_tree()->root_layer()->impl_transform().matrix().getDouble(3, 3));
}

TEST_F(LayerTreeHostImplTest, scrollRootAndChangePageScaleOnImplThread)
{
    gfx::Size surfaceSize(10, 10);
    float pageScale = 2;
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->active_tree()->DidBecomeActive();
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(1, 1, pageScale);
    initializeRendererAndDrawFrame();

    gfx::Vector2d scrollDelta(0, 10);
    gfx::Vector2d expectedScrollDelta(scrollDelta);
    gfx::Vector2d expectedMaxScroll(m_hostImpl->active_tree()->root_layer()->max_scroll_offset());
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->ScrollEnd();

    // Set new page scale on impl thread by pinching.
    m_hostImpl->PinchGestureBegin();
    m_hostImpl->PinchGestureUpdate(pageScale, gfx::Point());
    m_hostImpl->PinchGestureEnd();
    drawOneFrame();

    // The scroll delta is not scaled because the main thread did not scale.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), expectedScrollDelta);

    // The scroll range should also have been updated.
    EXPECT_EQ(m_hostImpl->active_tree()->root_layer()->max_scroll_offset(), expectedMaxScroll);

    // The page scale delta should match the new scale on the impl side.
    gfx::Transform expectedScale;
    expectedScale.Scale(pageScale, pageScale);
    EXPECT_EQ(m_hostImpl->active_tree()->root_layer()->impl_transform(), expectedScale);
}

TEST_F(LayerTreeHostImplTest, pageScaleDeltaAppliedToRootScrollLayerOnly)
{
    gfx::Size surfaceSize(10, 10);
    float defaultPageScale = 1;
    gfx::Transform defaultPageScaleMatrix;

    float newPageScale = 2;
    gfx::Transform newPageScaleMatrix;
    newPageScaleMatrix.Scale(newPageScale, newPageScale);

    // Create a normal scrollable root layer and another scrollable child layer.
    setupScrollAndContentsLayers(surfaceSize);
    LayerImpl* root = m_hostImpl->active_tree()->root_layer();
    LayerImpl* child = root->children()[0];

    scoped_ptr<LayerImpl> scrollableChild = createScrollableLayer(3, surfaceSize);
    child->AddChild(scrollableChild.Pass());
    LayerImpl* grandChild = child->children()[0];

    // Set new page scale on impl thread by pinching.
    m_hostImpl->PinchGestureBegin();
    m_hostImpl->PinchGestureUpdate(newPageScale, gfx::Point());
    m_hostImpl->PinchGestureEnd();
    drawOneFrame();

    // The page scale delta should only be applied to the scrollable root layer.
    EXPECT_EQ(root->impl_transform(), newPageScaleMatrix);
    EXPECT_EQ(child->impl_transform(), defaultPageScaleMatrix);
    EXPECT_EQ(grandChild->impl_transform(), defaultPageScaleMatrix);

    // Make sure all the layers are drawn with the page scale delta applied, i.e., the page scale
    // delta on the root layer is applied hierarchically.
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);

    EXPECT_EQ(root->draw_transform().matrix().getDouble(0, 0), newPageScale);
    EXPECT_EQ(root->draw_transform().matrix().getDouble(1, 1), newPageScale);
    EXPECT_EQ(child->draw_transform().matrix().getDouble(0, 0), newPageScale);
    EXPECT_EQ(child->draw_transform().matrix().getDouble(1, 1), newPageScale);
    EXPECT_EQ(grandChild->draw_transform().matrix().getDouble(0, 0), newPageScale);
    EXPECT_EQ(grandChild->draw_transform().matrix().getDouble(1, 1), newPageScale);
}

TEST_F(LayerTreeHostImplTest, scrollChildAndChangePageScaleOnMainThread)
{
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    root->SetBounds(surfaceSize);
    root->SetContentBounds(surfaceSize);
    // Also mark the root scrollable so it becomes the root scroll layer.
    root->SetScrollable(true);
    int scrollLayerId = 2;
    root->AddChild(createScrollableLayer(scrollLayerId, surfaceSize));
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->active_tree()->DidBecomeActive();
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    LayerImpl* child = m_hostImpl->active_tree()->root_layer()->children()[0];

    gfx::Vector2d scrollDelta(0, 10);
    gfx::Vector2d expectedScrollDelta(scrollDelta);
    gfx::Vector2d expectedMaxScroll(child->max_scroll_offset());
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->ScrollEnd();

    float pageScale = 2;
    m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(pageScale, 1, pageScale);

    drawOneFrame();

    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo.get(), scrollLayerId, expectedScrollDelta);

    // The scroll range should not have changed.
    EXPECT_EQ(child->max_scroll_offset(), expectedMaxScroll);

    // The page scale delta remains constant because the impl thread did not scale.
    gfx::Transform identityTransform;
    EXPECT_EQ(child->impl_transform(), gfx::Transform());
}

TEST_F(LayerTreeHostImplTest, scrollChildBeyondLimit)
{
    // Scroll a child layer beyond its maximum scroll range and make sure the
    // parent layer is scrolled on the axis on which the child was unable to
    // scroll.
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);

    scoped_ptr<LayerImpl> grandChild = createScrollableLayer(3, surfaceSize);
    grandChild->SetScrollOffset(gfx::Vector2d(0, 5));

    scoped_ptr<LayerImpl> child = createScrollableLayer(2, surfaceSize);
    child->SetScrollOffset(gfx::Vector2d(3, 0));
    child->AddChild(grandChild.Pass());

    root->AddChild(child.Pass());
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->active_tree()->DidBecomeActive();
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();
    {
        gfx::Vector2d scrollDelta(-8, -7);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
        m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->ScrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();

        // The grand child should have scrolled up to its limit.
        LayerImpl* child = m_hostImpl->active_tree()->root_layer()->children()[0];
        LayerImpl* grandChild = child->children()[0];
        expectContains(*scrollInfo.get(), grandChild->id(), gfx::Vector2d(0, -5));

        // The child should have only scrolled on the other axis.
        expectContains(*scrollInfo.get(), child->id(), gfx::Vector2d(-3, 0));
    }
}

TEST_F(LayerTreeHostImplTest, scrollWithoutBubbling)
{
    // Scroll a child layer beyond its maximum scroll range and make sure the
    // the scroll doesn't bubble up to the parent layer.
    gfx::Size surfaceSize(10, 10);
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, surfaceSize);

    scoped_ptr<LayerImpl> grandChild = createScrollableLayer(3, surfaceSize);
    grandChild->SetScrollOffset(gfx::Vector2d(0, 2));

    scoped_ptr<LayerImpl> child = createScrollableLayer(2, surfaceSize);
    child->SetScrollOffset(gfx::Vector2d(0, 3));
    child->AddChild(grandChild.Pass());

    root->AddChild(child.Pass());
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->active_tree()->DidBecomeActive();
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();
    {
        gfx::Vector2d scrollDelta(0, -10);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::NonBubblingGesture), InputHandlerClient::ScrollStarted);
        m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->ScrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();

        // The grand child should have scrolled up to its limit.
        LayerImpl* child = m_hostImpl->active_tree()->root_layer()->children()[0];
        LayerImpl* grandChild = child->children()[0];
        expectContains(*scrollInfo.get(), grandChild->id(), gfx::Vector2d(0, -2));

        // The child should not have scrolled.
        expectNone(*scrollInfo.get(), child->id());

        // The next time we scroll we should only scroll the parent.
        scrollDelta = gfx::Vector2d(0, -3);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::NonBubblingGesture), InputHandlerClient::ScrollStarted);
        EXPECT_EQ(m_hostImpl->CurrentlyScrollingLayer(), grandChild);
        m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
        EXPECT_EQ(m_hostImpl->CurrentlyScrollingLayer(), child);
        m_hostImpl->ScrollEnd();

        scrollInfo = m_hostImpl->ProcessScrollDeltas();

        // The child should have scrolled up to its limit.
        expectContains(*scrollInfo.get(), child->id(), gfx::Vector2d(0, -3));

        // The grand child should not have scrolled.
        expectContains(*scrollInfo.get(), grandChild->id(), gfx::Vector2d(0, -2));

        // After scrolling the parent, another scroll on the opposite direction
        // should still scroll the child.
        scrollDelta = gfx::Vector2d(0, 7);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::NonBubblingGesture), InputHandlerClient::ScrollStarted);
        EXPECT_EQ(m_hostImpl->CurrentlyScrollingLayer(), grandChild);
        m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
        EXPECT_EQ(m_hostImpl->CurrentlyScrollingLayer(), grandChild);
        m_hostImpl->ScrollEnd();

        scrollInfo = m_hostImpl->ProcessScrollDeltas();

        // The grand child should have scrolled.
        expectContains(*scrollInfo.get(), grandChild->id(), gfx::Vector2d(0, 5));

        // The child should not have scrolled.
        expectContains(*scrollInfo.get(), child->id(), gfx::Vector2d(0, -3));


        // Scrolling should be adjusted from viewport space.
        m_hostImpl->active_tree()->SetPageScaleFactorAndLimits(2, 2, 2);
        m_hostImpl->active_tree()->SetPageScaleDelta(1);
        gfx::Transform scaleTransform;
        scaleTransform.Scale(2, 2);
        m_hostImpl->active_tree()->root_layer()->SetImplTransform(scaleTransform);

        scrollDelta = gfx::Vector2d(0, -2);
        EXPECT_EQ(InputHandlerClient::ScrollStarted, m_hostImpl->ScrollBegin(gfx::Point(1, 1), InputHandlerClient::NonBubblingGesture));
        EXPECT_EQ(grandChild, m_hostImpl->CurrentlyScrollingLayer());
        m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->ScrollEnd();

        scrollInfo = m_hostImpl->ProcessScrollDeltas();

        // Should have scrolled by half the amount in layer space (5 - 2/2)
        expectContains(*scrollInfo.get(), grandChild->id(), gfx::Vector2d(0, 4));
    }
}

TEST_F(LayerTreeHostImplTest, scrollEventBubbling)
{
    // When we try to scroll a non-scrollable child layer, the scroll delta
    // should be applied to one of its ancestors if possible.
    gfx::Size surfaceSize(10, 10);
    gfx::Size contentSize(20, 20);
    scoped_ptr<LayerImpl> root = createScrollableLayer(1, contentSize);
    scoped_ptr<LayerImpl> child = createScrollableLayer(2, contentSize);

    child->SetScrollable(false);
    root->AddChild(child.Pass());

    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    m_hostImpl->active_tree()->DidBecomeActive();
    initializeRendererAndDrawFrame();
    {
        gfx::Vector2d scrollDelta(0, 4);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
        m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
        m_hostImpl->ScrollEnd();

        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();

        // Only the root should have scrolled.
        ASSERT_EQ(scrollInfo->scrolls.size(), 1u);
        expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), scrollDelta);
    }
}

TEST_F(LayerTreeHostImplTest, scrollBeforeRedraw)
{
    gfx::Size surfaceSize(10, 10);
    m_hostImpl->active_tree()->SetRootLayer(createScrollableLayer(1, surfaceSize));
    m_hostImpl->active_tree()->DidBecomeActive();
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);

    // Draw one frame and then immediately rebuild the layer tree to mimic a tree synchronization.
    initializeRendererAndDrawFrame();
    m_hostImpl->active_tree()->DetachLayerTree();
    m_hostImpl->active_tree()->SetRootLayer(createScrollableLayer(2, surfaceSize));
    m_hostImpl->active_tree()->DidBecomeActive();

    // Scrolling should still work even though we did not draw yet.
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
}

TEST_F(LayerTreeHostImplTest, scrollAxisAlignedRotatedLayer)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // Rotate the root layer 90 degrees counter-clockwise about its center.
    gfx::Transform rotateTransform;
    rotateTransform.Rotate(-90);
    m_hostImpl->active_tree()->root_layer()->SetTransform(rotateTransform);

    gfx::Size surfaceSize(50, 50);
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scroll to the right in screen coordinates with a gesture.
    gfx::Vector2d gestureScrollDelta(10, 0);
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), gestureScrollDelta);
    m_hostImpl->ScrollEnd();

    // The layer should have scrolled down in its local coordinates.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), gfx::Vector2d(0, gestureScrollDelta.x()));

    // Reset and scroll down with the wheel.
    m_hostImpl->active_tree()->root_layer()->SetScrollDelta(gfx::Vector2dF());
    gfx::Vector2d wheelScrollDelta(0, 10);
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), wheelScrollDelta);
    m_hostImpl->ScrollEnd();

    // The layer should have scrolled down in its local coordinates.
    scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), wheelScrollDelta);
}

TEST_F(LayerTreeHostImplTest, scrollNonAxisAlignedRotatedLayer)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));
    int childLayerId = 3;
    float childLayerAngle = -20;

    // Create a child layer that is rotated to a non-axis-aligned angle.
    scoped_ptr<LayerImpl> child = createScrollableLayer(childLayerId, m_hostImpl->active_tree()->root_layer()->content_bounds());
    gfx::Transform rotateTransform;
    rotateTransform.Translate(-50, -50);
    rotateTransform.Rotate(childLayerAngle);
    rotateTransform.Translate(50, 50);
    child->SetTransform(rotateTransform);

    // Only allow vertical scrolling.
    child->SetMaxScrollOffset(gfx::Vector2d(0, child->content_bounds().height()));
    m_hostImpl->active_tree()->root_layer()->AddChild(child.Pass());

    gfx::Size surfaceSize(50, 50);
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    {
        // Scroll down in screen coordinates with a gesture.
        gfx::Vector2d gestureScrollDelta(0, 10);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
        m_hostImpl->ScrollBy(gfx::Point(), gestureScrollDelta);
        m_hostImpl->ScrollEnd();

        // The child layer should have scrolled down in its local coordinates an amount proportional to
        // the angle between it and the input scroll delta.
        gfx::Vector2d expectedScrollDelta(0, gestureScrollDelta.y() * std::cos(MathUtil::Deg2Rad(childLayerAngle)));
        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        expectContains(*scrollInfo.get(), childLayerId, expectedScrollDelta);

        // The root layer should not have scrolled, because the input delta was close to the layer's
        // axis of movement.
        EXPECT_EQ(scrollInfo->scrolls.size(), 1u);
    }

    {
        // Now reset and scroll the same amount horizontally.
        m_hostImpl->active_tree()->root_layer()->children()[1]->SetScrollDelta(gfx::Vector2dF());
        gfx::Vector2d gestureScrollDelta(10, 0);
        EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
        m_hostImpl->ScrollBy(gfx::Point(), gestureScrollDelta);
        m_hostImpl->ScrollEnd();

        // The child layer should have scrolled down in its local coordinates an amount proportional to
        // the angle between it and the input scroll delta.
        gfx::Vector2d expectedScrollDelta(0, -gestureScrollDelta.x() * std::sin(MathUtil::Deg2Rad(childLayerAngle)));
        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
        expectContains(*scrollInfo.get(), childLayerId, expectedScrollDelta);

        // The root layer should have scrolled more, since the input scroll delta was mostly
        // orthogonal to the child layer's vertical scroll axis.
        gfx::Vector2d expectedRootScrollDelta(gestureScrollDelta.x() * std::pow(std::cos(MathUtil::Deg2Rad(childLayerAngle)), 2), 0);
        expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), expectedRootScrollDelta);
    }
}

TEST_F(LayerTreeHostImplTest, scrollScaledLayer)
{
    setupScrollAndContentsLayers(gfx::Size(100, 100));

    // Scale the layer to twice its normal size.
    int scale = 2;
    gfx::Transform scaleTransform;
    scaleTransform.Scale(scale, scale);
    m_hostImpl->active_tree()->root_layer()->SetTransform(scaleTransform);

    gfx::Size surfaceSize(50, 50);
    m_hostImpl->SetViewportSize(surfaceSize, surfaceSize);
    initializeRendererAndDrawFrame();

    // Scroll down in screen coordinates with a gesture.
    gfx::Vector2d scrollDelta(0, 10);
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), scrollDelta);
    m_hostImpl->ScrollEnd();

    // The layer should have scrolled down in its local coordinates, but half he amount.
    scoped_ptr<ScrollAndScaleSet> scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), gfx::Vector2d(0, scrollDelta.y() / scale));

    // Reset and scroll down with the wheel.
    m_hostImpl->active_tree()->root_layer()->SetScrollDelta(gfx::Vector2dF());
    gfx::Vector2d wheelScrollDelta(0, 10);
    EXPECT_EQ(m_hostImpl->ScrollBegin(gfx::Point(0, 0), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
    m_hostImpl->ScrollBy(gfx::Point(), wheelScrollDelta);
    m_hostImpl->ScrollEnd();

    // The scale should not have been applied to the scroll delta.
    scrollInfo = m_hostImpl->ProcessScrollDeltas();
    expectContains(*scrollInfo.get(), m_hostImpl->active_tree()->root_layer()->id(), wheelScrollDelta);
}

class BlendStateTrackerContext: public TestWebGraphicsContext3D {
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
    static scoped_ptr<LayerImpl> Create(LayerTreeImpl* treeImpl, int id, ResourceProvider* resourceProvider) { return scoped_ptr<LayerImpl>(new BlendStateCheckLayer(treeImpl, id, resourceProvider)); }

    virtual void AppendQuads(QuadSink* quadSink, AppendQuadsData* appendQuadsData) OVERRIDE
    {
        m_quadsAppended = true;

        gfx::Rect opaqueRect;
        if (contents_opaque())
            opaqueRect = m_quadRect;
        else
            opaqueRect = m_opaqueContentRect;

        SharedQuadState* sharedQuadState = quadSink->UseSharedQuadState(CreateSharedQuadState());
        scoped_ptr<TileDrawQuad> testBlendingDrawQuad = TileDrawQuad::Create();
        testBlendingDrawQuad->SetNew(sharedQuadState, m_quadRect, opaqueRect, m_resourceId, gfx::RectF(0, 0, 1, 1), gfx::Size(1, 1), false);
        testBlendingDrawQuad->visible_rect = m_quadVisibleRect;
        EXPECT_EQ(m_blend, testBlendingDrawQuad->ShouldDrawWithBlending());
        EXPECT_EQ(m_hasRenderSurface, !!render_surface());
        quadSink->Append(testBlendingDrawQuad.PassAs<DrawQuad>(), appendQuadsData);
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
        , m_resourceId(resourceProvider->CreateResource(gfx::Size(1, 1), GL_RGBA, ResourceProvider::TextureUsageAny))
    {
        resourceProvider->AllocateForTesting(m_resourceId);
        SetAnchorPoint(gfx::PointF(0, 0));
        SetBounds(gfx::Size(10, 10));
        SetContentBounds(gfx::Size(10, 10));
        SetDrawsContent(true);
    }

    bool m_blend;
    bool m_hasRenderSurface;
    bool m_quadsAppended;
    gfx::Rect m_quadRect;
    gfx::Rect m_opaqueContentRect;
    gfx::Rect m_quadVisibleRect;
    ResourceProvider::ResourceId m_resourceId;
};

TEST_F(LayerTreeHostImplTest, blendingOffWhenDrawingOpaqueLayers)
{
    {
        scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl->active_tree(), 1);
        root->SetAnchorPoint(gfx::PointF(0, 0));
        root->SetBounds(gfx::Size(10, 10));
        root->SetContentBounds(root->bounds());
        root->SetDrawsContent(false);
        m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    }
    LayerImpl* root = m_hostImpl->active_tree()->root_layer();

    root->AddChild(BlendStateCheckLayer::Create(m_hostImpl->active_tree(), 2, m_hostImpl->resource_provider()));
    BlendStateCheckLayer* layer1 = static_cast<BlendStateCheckLayer*>(root->children()[0]);
    layer1->SetPosition(gfx::PointF(2, 2));

    LayerTreeHostImpl::FrameData frame;

    // Opaque layer, drawn without blending.
    layer1->SetContentsOpaque(true);
    layer1->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Layer with translucent content and painting, so drawn with blending.
    layer1->SetContentsOpaque(false);
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Layer with translucent opacity, drawn with blending.
    layer1->SetContentsOpaque(true);
    layer1->SetOpacity(0.5);
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Layer with translucent opacity and painting, drawn with blending.
    layer1->SetContentsOpaque(true);
    layer1->SetOpacity(0.5);
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    layer1->AddChild(BlendStateCheckLayer::Create(m_hostImpl->active_tree(), 3, m_hostImpl->resource_provider()));
    BlendStateCheckLayer* layer2 = static_cast<BlendStateCheckLayer*>(layer1->children()[0]);
    layer2->SetPosition(gfx::PointF(4, 4));

    // 2 opaque layers, drawn without blending.
    layer1->SetContentsOpaque(true);
    layer1->SetOpacity(1);
    layer1->setExpectation(false, false);
    layer2->SetContentsOpaque(true);
    layer2->SetOpacity(1);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Parent layer with translucent content, drawn with blending.
    // Child layer with opaque content, drawn without blending.
    layer1->SetContentsOpaque(false);
    layer1->setExpectation(true, false);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Parent layer with translucent content but opaque painting, drawn without blending.
    // Child layer with opaque content, drawn without blending.
    layer1->SetContentsOpaque(true);
    layer1->setExpectation(false, false);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Parent layer with translucent opacity and opaque content. Since it has a
    // drawing child, it's drawn to a render surface which carries the opacity,
    // so it's itself drawn without blending.
    // Child layer with opaque content, drawn without blending (parent surface
    // carries the inherited opacity).
    layer1->SetContentsOpaque(true);
    layer1->SetOpacity(0.5);
    layer1->setExpectation(false, true);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Draw again, but with child non-opaque, to make sure
    // layer1 not culled.
    layer1->SetContentsOpaque(true);
    layer1->SetOpacity(1);
    layer1->setExpectation(false, false);
    layer2->SetContentsOpaque(true);
    layer2->SetOpacity(0.5);
    layer2->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // A second way of making the child non-opaque.
    layer1->SetContentsOpaque(true);
    layer1->SetOpacity(1);
    layer1->setExpectation(false, false);
    layer2->SetContentsOpaque(false);
    layer2->SetOpacity(1);
    layer2->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // And when the layer says its not opaque but is painted opaque, it is not blended.
    layer1->SetContentsOpaque(true);
    layer1->SetOpacity(1);
    layer1->setExpectation(false, false);
    layer2->SetContentsOpaque(true);
    layer2->SetOpacity(1);
    layer2->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    EXPECT_TRUE(layer2->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Layer with partially opaque contents, drawn with blending.
    layer1->SetContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(5, 5, 5, 5));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Layer with partially opaque contents partially culled, drawn with blending.
    layer1->SetContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(5, 5, 5, 2));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Layer with partially opaque contents culled, drawn with blending.
    layer1->SetContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(7, 5, 3, 5));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(true, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

    // Layer with partially opaque contents and translucent contents culled, drawn without blending.
    layer1->SetContentsOpaque(false);
    layer1->setQuadRect(gfx::Rect(5, 5, 5, 5));
    layer1->setQuadVisibleRect(gfx::Rect(5, 5, 2, 5));
    layer1->setOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
    layer1->setExpectation(false, false);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(layer1->quadsAppended());
    m_hostImpl->DidDrawAllLayers(frame);

}

TEST_F(LayerTreeHostImplTest, viewportCovered)
{
    m_hostImpl->InitializeRenderer(createOutputSurface());
    m_hostImpl->active_tree()->set_background_color(SK_ColorGRAY);

    gfx::Size viewportSize(1000, 1000);
    m_hostImpl->SetViewportSize(viewportSize, viewportSize);

    m_hostImpl->active_tree()->SetRootLayer(LayerImpl::Create(m_hostImpl->active_tree(), 1));
    m_hostImpl->active_tree()->root_layer()->AddChild(BlendStateCheckLayer::Create(m_hostImpl->active_tree(), 2, m_hostImpl->resource_provider()));
    BlendStateCheckLayer* child = static_cast<BlendStateCheckLayer*>(m_hostImpl->active_tree()->root_layer()->children()[0]);
    child->setExpectation(false, false);
    child->SetContentsOpaque(true);

    // No gutter rects
    {
        gfx::Rect layerRect(0, 0, 1000, 1000);
        child->SetPosition(layerRect.origin());
        child->SetBounds(layerRect.size());
        child->SetContentBounds(layerRect.size());
        child->setQuadRect(gfx::Rect(gfx::Point(), layerRect.size()));
        child->setQuadVisibleRect(gfx::Rect(gfx::Point(), layerRect.size()));

        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        ASSERT_EQ(1u, frame.render_passes.size());

        size_t numGutterQuads = 0;
        for (size_t i = 0; i < frame.render_passes[0]->quad_list.size(); ++i)
            numGutterQuads += (frame.render_passes[0]->quad_list[i]->material == DrawQuad::SOLID_COLOR) ? 1 : 0;
        EXPECT_EQ(0u, numGutterQuads);
        EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());

        LayerTestCommon::VerifyQuadsExactlyCoverRect(frame.render_passes[0]->quad_list, gfx::Rect(gfx::Point(), viewportSize));
        m_hostImpl->DidDrawAllLayers(frame);
    }

    // Empty visible content area (fullscreen gutter rect)
    {
        gfx::Rect layerRect(0, 0, 0, 0);
        child->SetPosition(layerRect.origin());
        child->SetBounds(layerRect.size());
        child->SetContentBounds(layerRect.size());
        child->setQuadRect(gfx::Rect(gfx::Point(), layerRect.size()));
        child->setQuadVisibleRect(gfx::Rect(gfx::Point(), layerRect.size()));

        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        ASSERT_EQ(1u, frame.render_passes.size());

        size_t numGutterQuads = 0;
        for (size_t i = 0; i < frame.render_passes[0]->quad_list.size(); ++i)
            numGutterQuads += (frame.render_passes[0]->quad_list[i]->material == DrawQuad::SOLID_COLOR) ? 1 : 0;
        EXPECT_EQ(1u, numGutterQuads);
        EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());

        LayerTestCommon::VerifyQuadsExactlyCoverRect(frame.render_passes[0]->quad_list, gfx::Rect(gfx::Point(), viewportSize));
        m_hostImpl->DidDrawAllLayers(frame);
    }

    // Content area in middle of clip rect (four surrounding gutter rects)
    {
        gfx::Rect layerRect(500, 500, 200, 200);
        child->SetPosition(layerRect.origin());
        child->SetBounds(layerRect.size());
        child->SetContentBounds(layerRect.size());
        child->setQuadRect(gfx::Rect(gfx::Point(), layerRect.size()));
        child->setQuadVisibleRect(gfx::Rect(gfx::Point(), layerRect.size()));

        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        ASSERT_EQ(1u, frame.render_passes.size());

        size_t numGutterQuads = 0;
        for (size_t i = 0; i < frame.render_passes[0]->quad_list.size(); ++i)
            numGutterQuads += (frame.render_passes[0]->quad_list[i]->material == DrawQuad::SOLID_COLOR) ? 1 : 0;
        EXPECT_EQ(4u, numGutterQuads);
        EXPECT_EQ(5u, frame.render_passes[0]->quad_list.size());

        LayerTestCommon::VerifyQuadsExactlyCoverRect(frame.render_passes[0]->quad_list, gfx::Rect(gfx::Point(), viewportSize));
        m_hostImpl->DidDrawAllLayers(frame);
    }

}


class ReshapeTrackerContext: public TestWebGraphicsContext3D {
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
    static scoped_ptr<LayerImpl> Create(LayerTreeImpl* treeImpl, int id) { return scoped_ptr<LayerImpl>(new FakeDrawableLayerImpl(treeImpl, id)); }
protected:
    FakeDrawableLayerImpl(LayerTreeImpl* treeImpl, int id) : LayerImpl(treeImpl, id) { }
};

// Only reshape when we know we are going to draw. Otherwise, the reshape
// can leave the window at the wrong size if we never draw and the proper
// viewport size is never set.
TEST_F(LayerTreeHostImplTest, reshapeNotCalledUntilDraw)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new ReshapeTrackerContext)).PassAs<OutputSurface>();
    ReshapeTrackerContext* reshapeTracker = static_cast<ReshapeTrackerContext*>(outputSurface->context3d());
    m_hostImpl->InitializeRenderer(outputSurface.Pass());

    scoped_ptr<LayerImpl> root = FakeDrawableLayerImpl::Create(m_hostImpl->active_tree(), 1);
    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetBounds(gfx::Size(10, 10));
    root->SetDrawsContent(true);
    m_hostImpl->active_tree()->SetRootLayer(root.Pass());
    EXPECT_FALSE(reshapeTracker->reshapeCalled());

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    EXPECT_TRUE(reshapeTracker->reshapeCalled());
    m_hostImpl->DidDrawAllLayers(frame);
}

class PartialSwapTrackerContext : public TestWebGraphicsContext3D {
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
TEST_F(LayerTreeHostImplTest, partialSwapReceivesDamageRect)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapTrackerContext)).PassAs<OutputSurface>();
    PartialSwapTrackerContext* partialSwapTracker = static_cast<PartialSwapTrackerContext*>(outputSurface->context3d());

    // This test creates its own LayerTreeHostImpl, so
    // that we can force partial swap enabled.
    LayerTreeSettings settings;
    settings.partial_swap_enabled = true;
    scoped_ptr<LayerTreeHostImpl> layerTreeHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);
    layerTreeHostImpl->InitializeRenderer(outputSurface.Pass());
    layerTreeHostImpl->SetViewportSize(gfx::Size(500, 500), gfx::Size(500, 500));

    scoped_ptr<LayerImpl> root = FakeDrawableLayerImpl::Create(layerTreeHostImpl->active_tree(), 1);
    scoped_ptr<LayerImpl> child = FakeDrawableLayerImpl::Create(layerTreeHostImpl->active_tree(), 2);
    child->SetPosition(gfx::PointF(12, 13));
    child->SetAnchorPoint(gfx::PointF(0, 0));
    child->SetBounds(gfx::Size(14, 15));
    child->SetContentBounds(gfx::Size(14, 15));
    child->SetDrawsContent(true);
    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetBounds(gfx::Size(500, 500));
    root->SetContentBounds(gfx::Size(500, 500));
    root->SetDrawsContent(true);
    root->AddChild(child.Pass());
    layerTreeHostImpl->active_tree()->SetRootLayer(root.Pass());

    LayerTreeHostImpl::FrameData frame;

    // First frame, the entire screen should get swapped.
    EXPECT_TRUE(layerTreeHostImpl->PrepareToDraw(&frame));
    layerTreeHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    layerTreeHostImpl->DidDrawAllLayers(frame);
    layerTreeHostImpl->SwapBuffers();
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
    layerTreeHostImpl->active_tree()->root_layer()->children()[0]->SetPosition(gfx::PointF(0, 0));
    EXPECT_TRUE(layerTreeHostImpl->PrepareToDraw(&frame));
    layerTreeHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);
    layerTreeHostImpl->SwapBuffers();
    actualSwapRect = partialSwapTracker->partialSwapRect();
    expectedSwapRect = gfx::Rect(gfx::Point(0, 500-28), gfx::Size(26, 28));
    EXPECT_EQ(expectedSwapRect.x(), actualSwapRect.x());
    EXPECT_EQ(expectedSwapRect.y(), actualSwapRect.y());
    EXPECT_EQ(expectedSwapRect.width(), actualSwapRect.width());
    EXPECT_EQ(expectedSwapRect.height(), actualSwapRect.height());

    // Make sure that partial swap is constrained to the viewport dimensions
    // expected damage rect: gfx::Rect(gfx::Point(), gfx::Size(500, 500));
    // expected swap rect: flipped damage rect, but also clamped to viewport
    layerTreeHostImpl->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    layerTreeHostImpl->active_tree()->root_layer()->SetOpacity(0.7f); // this will damage everything
    EXPECT_TRUE(layerTreeHostImpl->PrepareToDraw(&frame));
    layerTreeHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);
    layerTreeHostImpl->SwapBuffers();
    actualSwapRect = partialSwapTracker->partialSwapRect();
    expectedSwapRect = gfx::Rect(gfx::Point(), gfx::Size(10, 10));
    EXPECT_EQ(expectedSwapRect.x(), actualSwapRect.x());
    EXPECT_EQ(expectedSwapRect.y(), actualSwapRect.y());
    EXPECT_EQ(expectedSwapRect.width(), actualSwapRect.width());
    EXPECT_EQ(expectedSwapRect.height(), actualSwapRect.height());
}

TEST_F(LayerTreeHostImplTest, rootLayerDoesntCreateExtraSurface)
{
    scoped_ptr<LayerImpl> root = FakeDrawableLayerImpl::Create(m_hostImpl->active_tree(), 1);
    scoped_ptr<LayerImpl> child = FakeDrawableLayerImpl::Create(m_hostImpl->active_tree(), 2);
    child->SetAnchorPoint(gfx::PointF(0, 0));
    child->SetBounds(gfx::Size(10, 10));
    child->SetContentBounds(gfx::Size(10, 10));
    child->SetDrawsContent(true);
    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetBounds(gfx::Size(10, 10));
    root->SetContentBounds(gfx::Size(10, 10));
    root->SetDrawsContent(true);
    root->SetOpacity(0.7f);
    root->AddChild(child.Pass());

    m_hostImpl->active_tree()->SetRootLayer(root.Pass());

    LayerTreeHostImpl::FrameData frame;

    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    EXPECT_EQ(1u, frame.render_surface_layer_list->size());
    EXPECT_EQ(1u, frame.render_passes.size());
    m_hostImpl->DidDrawAllLayers(frame);
}

class FakeLayerWithQuads : public LayerImpl {
public:
    static scoped_ptr<LayerImpl> Create(LayerTreeImpl* treeImpl, int id) { return scoped_ptr<LayerImpl>(new FakeLayerWithQuads(treeImpl, id)); }

    virtual void AppendQuads(QuadSink* quadSink, AppendQuadsData* appendQuadsData) OVERRIDE
    {
        SharedQuadState* sharedQuadState = quadSink->UseSharedQuadState(CreateSharedQuadState());

        SkColor gray = SkColorSetRGB(100, 100, 100);
        gfx::Rect quadRect(gfx::Point(0, 0), content_bounds());
        scoped_ptr<SolidColorDrawQuad> myQuad = SolidColorDrawQuad::Create();
        myQuad->SetNew(sharedQuadState, quadRect, gray);
        quadSink->Append(myQuad.PassAs<DrawQuad>(), appendQuadsData);
    }

private:
    FakeLayerWithQuads(LayerTreeImpl* treeImpl, int id)
        : LayerImpl(treeImpl, id)
    {
    }
};

class MockContext : public TestWebGraphicsContext3D {
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

TEST_F(LayerTreeHostImplTest, noPartialSwap)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new MockContext)).PassAs<OutputSurface>();
    MockContext* mockContext = static_cast<MockContext*>(outputSurface->context3d());
    MockContextHarness harness(mockContext);

    // Run test case
    createLayerTreeHost(false, outputSurface.Pass());
    setupRootLayerImpl(FakeLayerWithQuads::Create(m_hostImpl->active_tree(), 1));

    // without partial swap, and no clipping, no scissor is set.
    harness.mustDrawSolidQuad();
    harness.mustSetNoScissor();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);

    // without partial swap, but a layer does clip its subtree, one scissor is set.
    m_hostImpl->active_tree()->root_layer()->SetMasksToBounds(true);
    harness.mustDrawSolidQuad();
    harness.mustSetScissor(0, 0, 10, 10);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);
}

TEST_F(LayerTreeHostImplTest, partialSwap)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new MockContext)).PassAs<OutputSurface>();
    MockContext* mockContext = static_cast<MockContext*>(outputSurface->context3d());
    MockContextHarness harness(mockContext);

    createLayerTreeHost(true, outputSurface.Pass());
    setupRootLayerImpl(FakeLayerWithQuads::Create(m_hostImpl->active_tree(), 1));

    // The first frame is not a partially-swapped one.
    harness.mustSetScissor(0, 0, 10, 10);
    harness.mustDrawSolidQuad();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);

    // Damage a portion of the frame.
    m_hostImpl->active_tree()->root_layer()->set_update_rect(gfx::Rect(0, 0, 2, 3));

    // The second frame will be partially-swapped (the y coordinates are flipped).
    harness.mustSetScissor(0, 7, 2, 3);
    harness.mustDrawSolidQuad();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }
    Mock::VerifyAndClearExpectations(&mockContext);
}

class PartialSwapContext : public TestWebGraphicsContext3D {
public:
    virtual WebKit::WebString getString(WebKit::WGC3Denum name)
    {
        if (name == GL_EXTENSIONS)
            return WebKit::WebString("GL_CHROMIUM_post_sub_buffer");
        return WebKit::WebString();
    }

    virtual WebKit::WebString getRequestableExtensionsCHROMIUM()
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

static scoped_ptr<LayerTreeHostImpl> setupLayersForOpacity(bool partialSwap, LayerTreeHostImplClient* client, Proxy* proxy, RenderingStatsInstrumentation* statsInstrumentation)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    LayerTreeSettings settings;
    settings.partial_swap_enabled = partialSwap;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, client, proxy, statsInstrumentation);
    myHostImpl->InitializeRenderer(outputSurface.Pass());
    myHostImpl->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

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
    scoped_ptr<LayerImpl> root = LayerImpl::Create(myHostImpl->active_tree(), 1);
    scoped_ptr<LayerImpl> child = LayerImpl::Create(myHostImpl->active_tree(), 2);
    scoped_ptr<LayerImpl> grandChild = FakeLayerWithQuads::Create(myHostImpl->active_tree(), 3);

    gfx::Rect rootRect(0, 0, 100, 100);
    gfx::Rect childRect(10, 10, 50, 50);
    gfx::Rect grandChildRect(5, 5, 150, 150);

    root->CreateRenderSurface();
    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(rootRect.x(), rootRect.y()));
    root->SetBounds(gfx::Size(rootRect.width(), rootRect.height()));
    root->SetContentBounds(root->bounds());
    root->draw_properties().visible_content_rect = rootRect;
    root->SetDrawsContent(false);
    root->render_surface()->SetContentRect(gfx::Rect(gfx::Point(), gfx::Size(rootRect.width(), rootRect.height())));

    child->SetAnchorPoint(gfx::PointF(0, 0));
    child->SetPosition(gfx::PointF(childRect.x(), childRect.y()));
    child->SetOpacity(0.5f);
    child->SetBounds(gfx::Size(childRect.width(), childRect.height()));
    child->SetContentBounds(child->bounds());
    child->draw_properties().visible_content_rect = childRect;
    child->SetDrawsContent(false);
    child->SetForceRenderSurface(true);

    grandChild->SetAnchorPoint(gfx::PointF(0, 0));
    grandChild->SetPosition(gfx::Point(grandChildRect.x(), grandChildRect.y()));
    grandChild->SetBounds(gfx::Size(grandChildRect.width(), grandChildRect.height()));
    grandChild->SetContentBounds(grandChild->bounds());
    grandChild->draw_properties().visible_content_rect = grandChildRect;
    grandChild->SetDrawsContent(true);

    child->AddChild(grandChild.Pass());
    root->AddChild(child.Pass());

    myHostImpl->active_tree()->SetRootLayer(root.Pass());
    return myHostImpl.Pass();
}

TEST_F(LayerTreeHostImplTest, contributingLayerEmptyScissorPartialSwap)
{
    scoped_ptr<LayerTreeHostImpl> myHostImpl = setupLayersForOpacity(true, this, &m_proxy, &m_statsInstrumentation);

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Verify all quads have been computed
        ASSERT_EQ(2U, frame.render_passes.size());
        ASSERT_EQ(1U, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(1U, frame.render_passes[1]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.render_passes[0]->quad_list[0]->material);
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, contributingLayerEmptyScissorNoPartialSwap)
{
    scoped_ptr<LayerTreeHostImpl> myHostImpl = setupLayersForOpacity(false, this, &m_proxy, &m_statsInstrumentation);

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Verify all quads have been computed
        ASSERT_EQ(2U, frame.render_passes.size());
        ASSERT_EQ(1U, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(1U, frame.render_passes[1]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.render_passes[0]->quad_list[0]->material);
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

// Fake WebKit::WebGraphicsContext3D that tracks the number of textures in use.
class TrackingWebGraphicsContext3D : public TestWebGraphicsContext3D {
public:
    TrackingWebGraphicsContext3D()
        : TestWebGraphicsContext3D()
        , m_numTextures(0)
    { }

    virtual WebKit::WebGLId createTexture() OVERRIDE
    {
        WebKit::WebGLId id = TestWebGraphicsContext3D::createTexture();

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
    return resourceProvider->CreateResource(
        gfx::Size(20, 12),
        resourceProvider->best_texture_format(),
        ResourceProvider::TextureUsageAny);
}

static unsigned createTextureId(ResourceProvider* resourceProvider)
{
    return ResourceProvider::ScopedReadLockGL(
        resourceProvider, createResourceId(resourceProvider)).texture_id();
}

TEST_F(LayerTreeHostImplTest, layersFreeTextures)
{
    scoped_ptr<TestWebGraphicsContext3D> context =
            TestWebGraphicsContext3D::Create();
    TestWebGraphicsContext3D* context3d = context.get();
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(
        context.PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
    m_hostImpl->InitializeRenderer(outputSurface.Pass());

    scoped_ptr<LayerImpl> rootLayer(LayerImpl::Create(m_hostImpl->active_tree(), 1));
    rootLayer->SetBounds(gfx::Size(10, 10));
    rootLayer->SetAnchorPoint(gfx::PointF());

    scoped_refptr<VideoFrame> softwareFrame(media::VideoFrame::CreateColorFrame(
        gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta()));
    FakeVideoFrameProvider provider;
    provider.set_frame(softwareFrame);
    scoped_ptr<VideoLayerImpl> videoLayer = VideoLayerImpl::Create(m_hostImpl->active_tree(), 4, &provider);
    videoLayer->SetBounds(gfx::Size(10, 10));
    videoLayer->SetAnchorPoint(gfx::PointF(0, 0));
    videoLayer->SetContentBounds(gfx::Size(10, 10));
    videoLayer->SetDrawsContent(true);
    rootLayer->AddChild(videoLayer.PassAs<LayerImpl>());

    scoped_ptr<IOSurfaceLayerImpl> ioSurfaceLayer = IOSurfaceLayerImpl::Create(m_hostImpl->active_tree(), 5);
    ioSurfaceLayer->SetBounds(gfx::Size(10, 10));
    ioSurfaceLayer->SetAnchorPoint(gfx::PointF(0, 0));
    ioSurfaceLayer->SetContentBounds(gfx::Size(10, 10));
    ioSurfaceLayer->SetDrawsContent(true);
    ioSurfaceLayer->SetIOSurfaceProperties(1, gfx::Size(10, 10));
    rootLayer->AddChild(ioSurfaceLayer.PassAs<LayerImpl>());

    m_hostImpl->active_tree()->SetRootLayer(rootLayer.Pass());

    EXPECT_EQ(0u, context3d->NumTextures());

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);
    m_hostImpl->SwapBuffers();

    EXPECT_GT(context3d->NumTextures(), 0u);

    // Kill the layer tree.
    m_hostImpl->active_tree()->SetRootLayer(LayerImpl::Create(m_hostImpl->active_tree(), 100));
    // There should be no textures left in use after.
    EXPECT_EQ(0u, context3d->NumTextures());
}

class MockDrawQuadsToFillScreenContext : public TestWebGraphicsContext3D {
public:
    MOCK_METHOD1(useProgram, void(WebKit::WebGLId program));
    MOCK_METHOD4(drawElements, void(WebKit::WGC3Denum mode, WebKit::WGC3Dsizei count, WebKit::WGC3Denum type, WebKit::WGC3Dintptr offset));
};

TEST_F(LayerTreeHostImplTest, hasTransparentBackground)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new MockDrawQuadsToFillScreenContext)).PassAs<OutputSurface>();
    MockDrawQuadsToFillScreenContext* mockContext = static_cast<MockDrawQuadsToFillScreenContext*>(outputSurface->context3d());

    // Run test case
    createLayerTreeHost(false, outputSurface.Pass());
    setupRootLayerImpl(LayerImpl::Create(m_hostImpl->active_tree(), 1));
    m_hostImpl->active_tree()->set_background_color(SK_ColorWHITE);

    // Verify one quad is drawn when transparent background set is not set.
    m_hostImpl->active_tree()->set_has_transparent_background(false);
    EXPECT_CALL(*mockContext, useProgram(_))
        .Times(1);
    EXPECT_CALL(*mockContext, drawElements(_, _, _, _))
        .Times(1);
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);
    Mock::VerifyAndClearExpectations(&mockContext);

    // Verify no quads are drawn when transparent background is set.
    m_hostImpl->active_tree()->set_has_transparent_background(true);
    EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
    m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
    m_hostImpl->DidDrawAllLayers(frame);
    Mock::VerifyAndClearExpectations(&mockContext);
}

static void addDrawingLayerTo(LayerImpl* parent, int id, const gfx::Rect& layerRect, LayerImpl** result)
{
    scoped_ptr<LayerImpl> layer = FakeLayerWithQuads::Create(parent->layer_tree_impl(), id);
    LayerImpl* layerPtr = layer.get();
    layerPtr->SetAnchorPoint(gfx::PointF(0, 0));
    layerPtr->SetPosition(gfx::PointF(layerRect.origin()));
    layerPtr->SetBounds(layerRect.size());
    layerPtr->SetContentBounds(layerRect.size());
    layerPtr->SetDrawsContent(true); // only children draw content
    layerPtr->SetContentsOpaque(true);
    parent->AddChild(layer.Pass());
    if (result)
        *result = layerPtr;
}

static void setupLayersForTextureCaching(LayerTreeHostImpl* layerTreeHostImpl, LayerImpl*& rootPtr, LayerImpl*& intermediateLayerPtr, LayerImpl*& surfaceLayerPtr, LayerImpl*& childPtr, const gfx::Size& rootSize)
{
    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    layerTreeHostImpl->InitializeRenderer(outputSurface.Pass());
    layerTreeHostImpl->SetViewportSize(rootSize, rootSize);

    scoped_ptr<LayerImpl> root = LayerImpl::Create(layerTreeHostImpl->active_tree(), 1);
    rootPtr = root.get();

    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(0, 0));
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetDrawsContent(true);
    layerTreeHostImpl->active_tree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(10, 10, rootSize.width(), rootSize.height()), &intermediateLayerPtr);
    intermediateLayerPtr->SetDrawsContent(false); // only children draw content

    // Surface layer is the layer that changes its opacity
    // It will contain other layers that draw content.
    addDrawingLayerTo(intermediateLayerPtr, 3, gfx::Rect(10, 10, rootSize.width(), rootSize.height()), &surfaceLayerPtr);
    surfaceLayerPtr->SetDrawsContent(false); // only children draw content
    surfaceLayerPtr->SetOpacity(0.5f);
    surfaceLayerPtr->SetForceRenderSurface(true); // This will cause it to have a surface

    // Child of the surface layer will produce some quads
    addDrawingLayerTo(surfaceLayerPtr, 4, gfx::Rect(5, 5, rootSize.width() - 25, rootSize.height() - 25), &childPtr);
}

class GLRendererWithReleaseTextures : public GLRenderer {
public:
    using GLRenderer::ReleaseRenderPassTextures;
};

TEST_F(LayerTreeHostImplTest, textureCachingWithOcclusion)
{
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

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

    myHostImpl->InitializeRenderer(outputSurface.Pass());
    myHostImpl->SetViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::Create(myHostImpl->active_tree(), 1);
    rootPtr = root.get();

    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(0, 0));
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetDrawsContent(true);
    root->SetMasksToBounds(true);
    myHostImpl->active_tree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(300, 300, 300, 300), &layerS1Ptr);
    layerS1Ptr->SetForceRenderSurface(true);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(10, 10, 10, 10), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(0, 0, 30, 30), 0); // L12

    addDrawingLayerTo(rootPtr, 5, gfx::Rect(550, 250, 300, 400), &layerS2Ptr);
    layerS2Ptr->SetForceRenderSurface(true);

    addDrawingLayerTo(layerS2Ptr, 6, gfx::Rect(20, 20, 5, 5), 0); // L21

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 2 quads (1 is occluded); for S2, there is 2 quads.
        ASSERT_EQ(3U, frame.render_passes.size());

        EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());
        EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(150, 150);
    layerS2Ptr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads
        // For S1, the number of quads depends on what got unoccluded, so not asserted beyond being positive.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.render_passes.size());

        EXPECT_GT(frame.render_passes[0]->quad_list.size(), 0U);
        EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // "Re-occlude" surface S1 and repeat draw.
    // Must remove S1's render pass since it is now available in full.
    // S2 has no change so must also be removed.
    transform = layerS2Ptr->transform();
    transform.Translate(-15, -15);
    layerS2Ptr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 1 render pass - for the root.
        ASSERT_EQ(1U, frame.render_passes.size());

        EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

}

TEST_F(LayerTreeHostImplTest, textureCachingWithOcclusionEarlyOut)
{
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

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

    myHostImpl->InitializeRenderer(outputSurface.Pass());
    myHostImpl->SetViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::Create(myHostImpl->active_tree(), 1);
    rootPtr = root.get();

    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(0, 0));
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetDrawsContent(true);
    root->SetMasksToBounds(true);
    myHostImpl->active_tree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(0, 0, 800, 800), &layerS1Ptr);
    layerS1Ptr->SetForceRenderSurface(true);
    layerS1Ptr->SetDrawsContent(false);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(0, 0, 300, 300), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(0, 500, 300, 300), 0); // L12
    addDrawingLayerTo(layerS1Ptr, 5, gfx::Rect(500, 0, 300, 300), 0); // L13
    addDrawingLayerTo(layerS1Ptr, 6, gfx::Rect(500, 500, 300, 300), 0); // L14
    addDrawingLayerTo(layerS1Ptr, 9, gfx::Rect(500, 500, 300, 300), 0); // L14

    addDrawingLayerTo(rootPtr, 7, gfx::Rect(450, 450, 450, 450), &layerS2Ptr);
    layerS2Ptr->SetForceRenderSurface(true);

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 3 quads; for S2, there is 1 quad.
        ASSERT_EQ(3U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

        // L14 is culled, so only 3 quads.
        EXPECT_EQ(3U, frame.render_passes[1]->quad_list.size());
        EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(100, 100);
    layerS2Ptr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads
        // For S1, the number of quads depends on what got unoccluded, so not asserted beyond being positive.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.render_passes.size());

        EXPECT_GT(frame.render_passes[0]->quad_list.size(), 0U);
        EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // "Re-occlude" surface S1 and repeat draw.
    // Must remove S1's render pass since it is now available in full.
    // S2 has no change so must also be removed.
    transform = layerS2Ptr->transform();
    transform.Translate(-15, -15);
    layerS2Ptr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 1 render pass - for the root.
        ASSERT_EQ(1U, frame.render_passes.size());

        EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, textureCachingWithOcclusionExternalOverInternal)
{
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

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

    myHostImpl->InitializeRenderer(outputSurface.Pass());
    myHostImpl->SetViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::Create(myHostImpl->active_tree(), 1);
    rootPtr = root.get();

    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(0, 0));
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetDrawsContent(true);
    root->SetMasksToBounds(true);
    myHostImpl->active_tree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(0, 0, 400, 400), &layerS1Ptr);
    layerS1Ptr->SetForceRenderSurface(true);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(0, 0, 300, 300), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(100, 0, 300, 300), 0); // L12

    addDrawingLayerTo(rootPtr, 7, gfx::Rect(200, 0, 300, 300), &layerS2Ptr);
    layerS2Ptr->SetForceRenderSurface(true);

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 3 quads; for S2, there is 1 quad.
        ASSERT_EQ(3U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(3U, frame.render_passes[1]->quad_list.size());
        EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(300, 0);
    layerS2Ptr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads
        // For S1, the number of quads depends on what got unoccluded, so not asserted beyond being positive.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.render_passes.size());

        EXPECT_GT(frame.render_passes[0]->quad_list.size(), 0U);
        EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, textureCachingWithOcclusionExternalNotAligned)
{
    LayerTreeSettings settings;
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

    // Layers are structured as follows:
    //
    //  R +-- S1 +- L10 (rotated, drawing)
    //           +- L11 (occupies half surface)

    LayerImpl* rootPtr;
    LayerImpl* layerS1Ptr;

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();

    gfx::Size rootSize(1000, 1000);

    myHostImpl->InitializeRenderer(outputSurface.Pass());
    myHostImpl->SetViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::Create(myHostImpl->active_tree(), 1);
    rootPtr = root.get();

    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(0, 0));
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetDrawsContent(true);
    root->SetMasksToBounds(true);
    myHostImpl->active_tree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(0, 0, 400, 400), &layerS1Ptr);
    layerS1Ptr->SetForceRenderSurface(true);
    gfx::Transform transform = layerS1Ptr->transform();
    transform.Translate(200, 200);
    transform.Rotate(45);
    transform.Translate(-200, -200);
    layerS1Ptr->SetTransform(transform);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(200, 0, 200, 400), 0); // L11

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 2 render passes.
        ASSERT_EQ(2U, frame.render_passes.size());

        EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change opacity and draw. Verify we used cached texture.
    layerS1Ptr->SetOpacity(0.2f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // One render pass must be gone due to cached texture.
        ASSERT_EQ(1U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, textureCachingWithOcclusionPartialSwap)
{
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.partial_swap_enabled = true;
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

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

    myHostImpl->InitializeRenderer(outputSurface.Pass());
    myHostImpl->SetViewportSize(gfx::Size(rootSize.width(), rootSize.height()), gfx::Size(rootSize.width(), rootSize.height()));

    scoped_ptr<LayerImpl> root = LayerImpl::Create(myHostImpl->active_tree(), 1);
    rootPtr = root.get();

    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(0, 0));
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetDrawsContent(true);
    root->SetMasksToBounds(true);
    myHostImpl->active_tree()->SetRootLayer(root.Pass());

    addDrawingLayerTo(rootPtr, 2, gfx::Rect(300, 300, 300, 300), &layerS1Ptr);
    layerS1Ptr->SetForceRenderSurface(true);

    addDrawingLayerTo(layerS1Ptr, 3, gfx::Rect(10, 10, 10, 10), 0); // L11
    addDrawingLayerTo(layerS1Ptr, 4, gfx::Rect(0, 0, 30, 30), 0); // L12

    addDrawingLayerTo(rootPtr, 5, gfx::Rect(550, 250, 300, 400), &layerS2Ptr);
    layerS2Ptr->SetForceRenderSurface(true);

    addDrawingLayerTo(layerS2Ptr, 6, gfx::Rect(20, 20, 5, 5), 0); // L21

    // Initial draw - must receive all quads
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 3 render passes.
        // For Root, there are 2 quads; for S1, there are 2 quads (one is occluded); for S2, there is 2 quads.
        ASSERT_EQ(3U, frame.render_passes.size());

        EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());
        EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // "Unocclude" surface S1 and repeat draw.
    // Must remove S2's render pass since it's cached;
    // Must keep S1 quads because texture contained external occlusion.
    gfx::Transform transform = layerS2Ptr->transform();
    transform.Translate(150, 150);
    layerS2Ptr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive 2 render passes.
        // For Root, there are 2 quads.
        // For S1, there are 2 quads.
        // For S2, there is no render pass
        ASSERT_EQ(2U, frame.render_passes.size());

        EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // "Re-occlude" surface S1 and repeat draw.
    // Must remove S1's render pass since it is now available in full.
    // S2 has no change so must also be removed.
    transform = layerS2Ptr->transform();
    transform.Translate(-15, -15);
    layerS2Ptr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Root render pass only.
        ASSERT_EQ(1U, frame.render_passes.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, textureCachingWithScissor)
{
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

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
    scoped_ptr<LayerImpl> root = LayerImpl::Create(myHostImpl->active_tree(), 1);
    scoped_ptr<TiledLayerImpl> child = TiledLayerImpl::Create(myHostImpl->active_tree(), 2);
    scoped_ptr<LayerImpl> grandChild = LayerImpl::Create(myHostImpl->active_tree(), 3);

    gfx::Rect rootRect(0, 0, 100, 100);
    gfx::Rect childRect(10, 10, 50, 50);
    gfx::Rect grandChildRect(5, 5, 150, 150);

    scoped_ptr<OutputSurface> outputSurface = FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new PartialSwapContext)).PassAs<OutputSurface>();
    myHostImpl->InitializeRenderer(outputSurface.Pass());

    root->SetAnchorPoint(gfx::PointF(0, 0));
    root->SetPosition(gfx::PointF(rootRect.x(), rootRect.y()));
    root->SetBounds(gfx::Size(rootRect.width(), rootRect.height()));
    root->SetContentBounds(root->bounds());
    root->SetDrawsContent(true);
    root->SetMasksToBounds(true);

    child->SetAnchorPoint(gfx::PointF(0, 0));
    child->SetPosition(gfx::PointF(childRect.x(), childRect.y()));
    child->SetOpacity(0.5);
    child->SetBounds(gfx::Size(childRect.width(), childRect.height()));
    child->SetContentBounds(child->bounds());
    child->SetDrawsContent(true);
    child->set_skips_draw(false);

    // child layer has 10x10 tiles.
    scoped_ptr<LayerTilingData> tiler = LayerTilingData::Create(gfx::Size(10, 10), LayerTilingData::HAS_BORDER_TEXELS);
    tiler->SetBounds(child->content_bounds());
    child->SetTilingData(*tiler.get());

    grandChild->SetAnchorPoint(gfx::PointF(0, 0));
    grandChild->SetPosition(gfx::Point(grandChildRect.x(), grandChildRect.y()));
    grandChild->SetBounds(gfx::Size(grandChildRect.width(), grandChildRect.height()));
    grandChild->SetContentBounds(grandChild->bounds());
    grandChild->SetDrawsContent(true);

    TiledLayerImpl* childPtr = child.get();
    RenderPass::Id childPassId(childPtr->id(), 0);

    child->AddChild(grandChild.Pass());
    root->AddChild(child.PassAs<LayerImpl>());
    myHostImpl->active_tree()->SetRootLayer(root.Pass());
    myHostImpl->SetViewportSize(rootRect.size(), rootRect.size());

    EXPECT_FALSE(myHostImpl->renderer()->HaveCachedResourcesForRenderPassId(childPassId));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));
        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // We should have cached textures for surface 2.
    EXPECT_TRUE(myHostImpl->renderer()->HaveCachedResourcesForRenderPassId(childPassId));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));
        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // We should still have cached textures for surface 2 after drawing with no damage.
    EXPECT_TRUE(myHostImpl->renderer()->HaveCachedResourcesForRenderPassId(childPassId));

    // Damage a single tile of surface 2.
    childPtr->set_update_rect(gfx::Rect(10, 10, 10, 10));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));
        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // We should have a cached texture for surface 2 again even though it was damaged.
    EXPECT_TRUE(myHostImpl->renderer()->HaveCachedResourcesForRenderPassId(childPassId));
}

TEST_F(LayerTreeHostImplTest, surfaceTextureCaching)
{
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.partial_swap_enabled = true;
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

    LayerImpl* rootPtr;
    LayerImpl* intermediateLayerPtr;
    LayerImpl* surfaceLayerPtr;
    LayerImpl* childPtr;

    setupLayersForTextureCaching(myHostImpl.get(), rootPtr, intermediateLayerPtr, surfaceLayerPtr, childPtr, gfx::Size(100, 100));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.render_passes.size());
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
        RenderPass* targetPass = frame.render_passes_by_id[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Draw without any change
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) == frame.render_passes_by_id.end());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change opacity and draw
    surfaceLayerPtr->SetOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) == frame.render_passes_by_id.end());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change less benign property and draw - should have contents changed flag
    surfaceLayerPtr->SetStackingOrderChanged(true);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.render_passes[0]->quad_list[0]->material);

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
        RenderPass* targetPass = frame.render_passes_by_id[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change opacity again, and evict the cached surface texture.
    surfaceLayerPtr->SetOpacity(0.5f);
    static_cast<GLRendererWithReleaseTextures*>(myHostImpl->renderer())->ReleaseRenderPassTextures();

    // Change opacity and draw
    surfaceLayerPtr->SetOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive two render passes
        ASSERT_EQ(2U, frame.render_passes.size());

        // Even though not enough properties changed, the entire thing must be
        // redrawn as we don't have cached textures
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
        RenderPass* targetPass = frame.render_passes_by_id[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_TRUE(targetPass->damage_rect.IsEmpty());

        // Was our surface evicted?
        EXPECT_FALSE(myHostImpl->renderer()->HaveCachedResourcesForRenderPassId(targetPass->id));

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Draw without any change, to make sure the state is clear
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) == frame.render_passes_by_id.end());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change location of the intermediate layer
    gfx::Transform transform = intermediateLayerPtr->transform();
    transform.matrix().setDouble(0, 3, 1.0001);
    intermediateLayerPtr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive one render pass, as the other one should be culled.
        ASSERT_EQ(1U, frame.render_passes.size());
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) == frame.render_passes_by_id.end());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, surfaceTextureCachingNoPartialSwap)
{
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.cache_render_pass_contents = true;
    scoped_ptr<LayerTreeHostImpl> myHostImpl = LayerTreeHostImpl::Create(settings, this, &m_proxy, &m_statsInstrumentation);

    LayerImpl* rootPtr;
    LayerImpl* intermediateLayerPtr;
    LayerImpl* surfaceLayerPtr;
    LayerImpl* childPtr;

    setupLayersForTextureCaching(myHostImpl.get(), rootPtr, intermediateLayerPtr, surfaceLayerPtr, childPtr, gfx::Size(100, 100));

    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.render_passes.size());
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
        RenderPass* targetPass = frame.render_passes_by_id[quad->render_pass_id];
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        EXPECT_FALSE(frame.render_passes[0]->damage_rect.IsEmpty());
        EXPECT_FALSE(frame.render_passes[1]->damage_rect.IsEmpty());

        EXPECT_FALSE(frame.render_passes[0]->has_occlusion_from_outside_target_surface);
        EXPECT_FALSE(frame.render_passes[1]->has_occlusion_from_outside_target_surface);

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Draw without any change
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Even though there was no change, we set the damage to entire viewport.
        // One of the passes should be culled as a result, since contents didn't change
        // and we have cached texture.
        ASSERT_EQ(1U, frame.render_passes.size());
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

        EXPECT_TRUE(frame.render_passes[0]->damage_rect.IsEmpty());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change opacity and draw
    surfaceLayerPtr->SetOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive one render pass, as the other one should be culled
        ASSERT_EQ(1U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) == frame.render_passes_by_id.end());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change less benign property and draw - should have contents changed flag
    surfaceLayerPtr->SetStackingOrderChanged(true);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive two render passes, each with one quad
        ASSERT_EQ(2U, frame.render_passes.size());

        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, frame.render_passes[0]->quad_list[0]->material);

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
        RenderPass* targetPass = frame.render_passes_by_id[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_FALSE(targetPass->damage_rect.IsEmpty());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change opacity again, and evict the cached surface texture.
    surfaceLayerPtr->SetOpacity(0.5f);
    static_cast<GLRendererWithReleaseTextures*>(myHostImpl->renderer())->ReleaseRenderPassTextures();

    // Change opacity and draw
    surfaceLayerPtr->SetOpacity(0.6f);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive two render passes
        ASSERT_EQ(2U, frame.render_passes.size());

        // Even though not enough properties changed, the entire thing must be
        // redrawn as we don't have cached textures
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
        EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[1]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
        RenderPass* targetPass = frame.render_passes_by_id[quad->render_pass_id];
        ASSERT_TRUE(targetPass);
        EXPECT_TRUE(targetPass->damage_rect.IsEmpty());

        // Was our surface evicted?
        EXPECT_FALSE(myHostImpl->renderer()->HaveCachedResourcesForRenderPassId(targetPass->id));

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Draw without any change, to make sure the state is clear
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Even though there was no change, we set the damage to entire viewport.
        // One of the passes should be culled as a result, since contents didn't change
        // and we have cached texture.
        ASSERT_EQ(1U, frame.render_passes.size());
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }

    // Change location of the intermediate layer
    gfx::Transform transform = intermediateLayerPtr->transform();
    transform.matrix().setDouble(0, 3, 1.0001);
    intermediateLayerPtr->SetTransform(transform);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(myHostImpl->PrepareToDraw(&frame));

        // Must receive one render pass, as the other one should be culled.
        ASSERT_EQ(1U, frame.render_passes.size());
        EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

        EXPECT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) == frame.render_passes_by_id.end());

        myHostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        myHostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, releaseContentsTextureShouldTriggerCommit)
{
    setReduceMemoryResult(false);

    // Even if changing the memory limit didn't result in anything being
    // evicted, we need to re-commit because the new value may result in us
    // drawing something different than before.
    setReduceMemoryResult(false);
    m_hostImpl->SetManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memory_allocation_limit_bytes() - 1));
    EXPECT_TRUE(m_didRequestCommit);
    m_didRequestCommit = false;

    // Especially if changing the memory limit caused evictions, we need
    // to re-commit.
    setReduceMemoryResult(true);
    m_hostImpl->SetManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memory_allocation_limit_bytes() - 1));
    EXPECT_TRUE(m_didRequestCommit);
    m_didRequestCommit = false;

    // But if we set it to the same value that it was before, we shouldn't
    // re-commit.
    m_hostImpl->SetManagedMemoryPolicy(ManagedMemoryPolicy(
        m_hostImpl->memory_allocation_limit_bytes()));
    EXPECT_FALSE(m_didRequestCommit);
}

struct RenderPassRemovalTestData : public LayerTreeHostImpl::FrameData {
    ScopedPtrHashMap<RenderPass::Id, TestRenderPass> renderPassCache;
    scoped_ptr<SharedQuadState> sharedQuadState;
};

class TestRenderer : public GLRenderer, public RendererClient {
public:
    static scoped_ptr<TestRenderer> Create(ResourceProvider* resourceProvider, OutputSurface* outputSurface, Proxy* proxy)
    {
        scoped_ptr<TestRenderer> renderer(new TestRenderer(resourceProvider, outputSurface, proxy));
        if (!renderer->Initialize())
            return scoped_ptr<TestRenderer>();

        return renderer.Pass();
    }

    void clearCachedTextures() { m_textures.clear(); }
    void setHaveCachedResourcesForRenderPassId(RenderPass::Id id) { m_textures.insert(id); }

    virtual bool HaveCachedResourcesForRenderPassId(RenderPass::Id id) const OVERRIDE { return m_textures.count(id); }

    // RendererClient implementation.
    virtual gfx::Size DeviceViewportSize() const OVERRIDE { return m_viewportSize; }
    virtual const LayerTreeSettings& Settings() const OVERRIDE { return m_settings; }
    virtual void DidLoseOutputSurface() OVERRIDE { }
    virtual void OnSwapBuffersComplete() OVERRIDE { }
    virtual void SetFullRootLayerDamage() OVERRIDE { }
    virtual void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { }
    virtual void EnforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { }
    virtual bool HasImplThread() const OVERRIDE { return false; }
    virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
    virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
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
    testData.sharedQuadState->SetAll(gfx::Transform(), gfx::Size(), gfx::Rect(), gfx::Rect(), false, 1.0);

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
        testData.render_passes_by_id[renderPassId] = renderPass.get();
        testData.render_passes.insert(testData.render_passes.begin(), renderPass.PassAs<RenderPass>());
        if (*currentChar)
            currentChar++;
    }
}

void dumpRenderPassTestData(const RenderPassRemovalTestData& testData, char* buffer)
{
    char* pos = buffer;
    for (RenderPassList::const_reverse_iterator it = testData.render_passes.rbegin(); it != testData.render_passes.rend(); ++it) {
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

TEST_F(LayerTreeHostImplTest, testRemoveRenderPasses)
{
    scoped_ptr<OutputSurface> outputSurface(createOutputSurface());
    ASSERT_TRUE(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::Create(outputSurface.get()));

    scoped_ptr<TestRenderer> renderer(TestRenderer::Create(resourceProvider.get(), outputSurface.get(), &m_proxy));

    int testCaseIndex = 0;
    while (removeRenderPassesCases[testCaseIndex].name) {
        RenderPassRemovalTestData testData;
        configureRenderPassTestData(removeRenderPassesCases[testCaseIndex].initScript, testData, renderer.get());
        LayerTreeHostImpl::RemoveRenderPasses(LayerTreeHostImpl::CullRenderPassesWithCachedTextures(*renderer), &testData);
        verifyRenderPassTestData(removeRenderPassesCases[testCaseIndex], testData);
        testCaseIndex++;
    }
}

class LayerTreeHostImplTestWithDelegatingRenderer : public LayerTreeHostImplTest {
protected:
    virtual scoped_ptr<OutputSurface> createOutputSurface() OVERRIDE
    {
        // Creates an output surface with a parent to use a delegating renderer.
        WebKit::WebGraphicsContext3D::Attributes attrs;
        return FakeOutputSurface::CreateDelegating3d(TestWebGraphicsContext3D::Create(attrs).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
    }

    void drawFrameAndTestDamage(const gfx::RectF& expectedDamage) {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));
        ASSERT_EQ(1u, frame.render_passes.size());

        // Verify the damage rect for the root render pass.
        const RenderPass* rootRenderPass = frame.render_passes.back();
        EXPECT_RECT_EQ(expectedDamage, rootRenderPass->damage_rect);

        // Verify the root layer's quad is generated and not being culled.
        ASSERT_EQ(1u, rootRenderPass->quad_list.size());
        gfx::Rect expectedVisibleRect(m_hostImpl->active_tree()->root_layer()->content_bounds());
        EXPECT_RECT_EQ(expectedVisibleRect, rootRenderPass->quad_list[0]->visible_rect);

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }
};

TEST_F(LayerTreeHostImplTestWithDelegatingRenderer, FrameIncludesDamageRect)
{
    scoped_ptr<SolidColorLayerImpl> root = SolidColorLayerImpl::Create(m_hostImpl->active_tree(), 1);
    root->SetAnchorPoint(gfx::PointF(0.f, 0.f));
    root->SetPosition(gfx::PointF(0.f, 0.f));
    root->SetBounds(gfx::Size(10, 10));
    root->SetContentBounds(gfx::Size(10, 10));
    root->SetDrawsContent(true);
    m_hostImpl->active_tree()->SetRootLayer(root.PassAs<LayerImpl>());

    // Draw a frame. In the first frame, the entire viewport should be damaged.
    gfx::Rect fullFrameDamage = gfx::Rect(m_hostImpl->device_viewport_size());
    drawFrameAndTestDamage(fullFrameDamage);

    // The second frame should have no damage, but the quads should still be generated.
    gfx::Rect noDamage = gfx::Rect();
    drawFrameAndTestDamage(noDamage);
}

class FakeMaskLayerImpl : public LayerImpl {
public:
    static scoped_ptr<FakeMaskLayerImpl> Create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new FakeMaskLayerImpl(treeImpl, id));
    }

    virtual ResourceProvider::ResourceId ContentsResourceId() const OVERRIDE { return 0; }

private:
    FakeMaskLayerImpl(LayerTreeImpl* treeImpl, int id) : LayerImpl(treeImpl, id) { }
};

TEST_F(LayerTreeHostImplTest, maskLayerWithScaling)
{
    // Root
    //  |
    //  +-- Scaling Layer (adds a 2x scale)
    //       |
    //       +-- Content Layer
    //             +--Mask
    scoped_ptr<LayerImpl> scopedRoot = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    LayerImpl* root = scopedRoot.get();
    m_hostImpl->active_tree()->SetRootLayer(scopedRoot.Pass());

    scoped_ptr<LayerImpl> scopedScalingLayer = LayerImpl::Create(m_hostImpl->active_tree(), 2);
    LayerImpl* scalingLayer = scopedScalingLayer.get();
    root->AddChild(scopedScalingLayer.Pass());

    scoped_ptr<LayerImpl> scopedContentLayer = LayerImpl::Create(m_hostImpl->active_tree(), 3);
    LayerImpl* contentLayer = scopedContentLayer.get();
    scalingLayer->AddChild(scopedContentLayer.Pass());

    scoped_ptr<FakeMaskLayerImpl> scopedMaskLayer = FakeMaskLayerImpl::Create(m_hostImpl->active_tree(), 4);
    FakeMaskLayerImpl* maskLayer = scopedMaskLayer.get();
    contentLayer->SetMaskLayer(scopedMaskLayer.PassAs<LayerImpl>());

    gfx::Size rootSize(100, 100);
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetPosition(gfx::PointF());
    root->SetAnchorPoint(gfx::PointF());

    gfx::Size scalingLayerSize(50, 50);
    scalingLayer->SetBounds(scalingLayerSize);
    scalingLayer->SetContentBounds(scalingLayerSize);
    scalingLayer->SetPosition(gfx::PointF());
    scalingLayer->SetAnchorPoint(gfx::PointF());
    gfx::Transform scale;
    scale.Scale(2.0, 2.0);
    scalingLayer->SetTransform(scale);

    contentLayer->SetBounds(scalingLayerSize);
    contentLayer->SetContentBounds(scalingLayerSize);
    contentLayer->SetPosition(gfx::PointF());
    contentLayer->SetAnchorPoint(gfx::PointF());
    contentLayer->SetDrawsContent(true);

    maskLayer->SetBounds(scalingLayerSize);
    maskLayer->SetContentBounds(scalingLayerSize);
    maskLayer->SetPosition(gfx::PointF());
    maskLayer->SetAnchorPoint(gfx::PointF());
    maskLayer->SetDrawsContent(true);


    // Check that the tree scaling is correctly taken into account for the mask,
    // that should fully map onto the quad.
    float deviceScaleFactor = 1.f;
    m_hostImpl->SetViewportSize(rootSize, rootSize);
    m_hostImpl->SetDeviceScaleFactor(deviceScaleFactor);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));

        ASSERT_EQ(1u, frame.render_passes.size());
        ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }


    // Applying a DSF should change the render surface size, but won't affect
    // which part of the mask is used.
    deviceScaleFactor = 2.f;
    gfx::Size deviceViewport(gfx::ToFlooredSize(gfx::ScaleSize(rootSize, deviceScaleFactor)));
    m_hostImpl->SetViewportSize(rootSize, deviceViewport);
    m_hostImpl->SetDeviceScaleFactor(deviceScaleFactor);
    m_hostImpl->active_tree()->set_needs_update_draw_properties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));

        ASSERT_EQ(1u, frame.render_passes.size());
        ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }


    // Applying an equivalent content scale on the content layer and the mask
    // should still result in the same part of the mask being used.
    gfx::Size contentsBounds(gfx::ToRoundedSize(gfx::ScaleSize(scalingLayerSize, deviceScaleFactor)));
    contentLayer->SetContentBounds(contentsBounds);
    contentLayer->SetContentsScale(deviceScaleFactor, deviceScaleFactor);
    maskLayer->SetContentBounds(contentsBounds);
    maskLayer->SetContentsScale(deviceScaleFactor, deviceScaleFactor);
    m_hostImpl->active_tree()->set_needs_update_draw_properties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));

        ASSERT_EQ(1u, frame.render_passes.size());
        ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }
}

TEST_F(LayerTreeHostImplTest, maskLayerWithDifferentBounds)
{
    // The mask layer has bounds 100x100 but is attached to a layer with bounds 50x50.

    scoped_ptr<LayerImpl> scopedRoot = LayerImpl::Create(m_hostImpl->active_tree(), 1);
    LayerImpl* root = scopedRoot.get();
    m_hostImpl->active_tree()->SetRootLayer(scopedRoot.Pass());

    scoped_ptr<LayerImpl> scopedContentLayer = LayerImpl::Create(m_hostImpl->active_tree(), 3);
    LayerImpl* contentLayer = scopedContentLayer.get();
    root->AddChild(scopedContentLayer.Pass());

    scoped_ptr<FakeMaskLayerImpl> scopedMaskLayer = FakeMaskLayerImpl::Create(m_hostImpl->active_tree(), 4);
    FakeMaskLayerImpl* maskLayer = scopedMaskLayer.get();
    contentLayer->SetMaskLayer(scopedMaskLayer.PassAs<LayerImpl>());

    gfx::Size rootSize(100, 100);
    root->SetBounds(rootSize);
    root->SetContentBounds(rootSize);
    root->SetPosition(gfx::PointF());
    root->SetAnchorPoint(gfx::PointF());

    gfx::Size layerSize(50, 50);
    contentLayer->SetBounds(layerSize);
    contentLayer->SetContentBounds(layerSize);
    contentLayer->SetPosition(gfx::PointF());
    contentLayer->SetAnchorPoint(gfx::PointF());
    contentLayer->SetDrawsContent(true);

    gfx::Size maskSize(100, 100);
    maskLayer->SetBounds(maskSize);
    maskLayer->SetContentBounds(maskSize);
    maskLayer->SetPosition(gfx::PointF());
    maskLayer->SetAnchorPoint(gfx::PointF());
    maskLayer->SetDrawsContent(true);


    // Check that the mask fills the surface.
    float deviceScaleFactor = 1.f;
    m_hostImpl->SetViewportSize(rootSize, rootSize);
    m_hostImpl->SetDeviceScaleFactor(deviceScaleFactor);
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));

        ASSERT_EQ(1u, frame.render_passes.size());
        ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }


    // Applying a DSF should change the render surface size, but won't affect
    // which part of the mask is used.
    deviceScaleFactor = 2.f;
    gfx::Size deviceViewport(gfx::ToFlooredSize(gfx::ScaleSize(rootSize, deviceScaleFactor)));
    m_hostImpl->SetViewportSize(rootSize, deviceViewport);
    m_hostImpl->SetDeviceScaleFactor(deviceScaleFactor);
    m_hostImpl->active_tree()->set_needs_update_draw_properties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));

        ASSERT_EQ(1u, frame.render_passes.size());
        ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }


    // Applying an equivalent content scale on the content layer and the mask
    // should still result in the same part of the mask being used.
    gfx::Size layerSizeLarge(gfx::ToRoundedSize(gfx::ScaleSize(layerSize, deviceScaleFactor)));
    contentLayer->SetContentBounds(layerSizeLarge);
    contentLayer->SetContentsScale(deviceScaleFactor, deviceScaleFactor);
    gfx::Size maskSizeLarge(gfx::ToRoundedSize(gfx::ScaleSize(maskSize, deviceScaleFactor)));
    maskLayer->SetContentBounds(maskSizeLarge);
    maskLayer->SetContentsScale(deviceScaleFactor, deviceScaleFactor);
    m_hostImpl->active_tree()->set_needs_update_draw_properties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));

        ASSERT_EQ(1u, frame.render_passes.size());
        ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }

    // Applying a different contents scale to the mask layer will still result
    // in the mask covering the owning layer.
    maskLayer->SetContentBounds(maskSize);
    maskLayer->SetContentsScale(deviceScaleFactor, deviceScaleFactor);
    m_hostImpl->active_tree()->set_needs_update_draw_properties();
    {
        LayerTreeHostImpl::FrameData frame;
        EXPECT_TRUE(m_hostImpl->PrepareToDraw(&frame));

        ASSERT_EQ(1u, frame.render_passes.size());
        ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
        ASSERT_EQ(DrawQuad::RENDER_PASS, frame.render_passes[0]->quad_list[0]->material);
        const RenderPassDrawQuad* renderPassQuad = RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), renderPassQuad->rect.ToString());
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(), renderPassQuad->mask_uv_rect.ToString());

        m_hostImpl->DrawLayers(&frame, base::TimeTicks::Now());
        m_hostImpl->DidDrawAllLayers(frame);
    }
}

}  // namespace
}  // namespace cc
