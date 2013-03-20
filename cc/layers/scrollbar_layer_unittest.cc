// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer.h"

#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/priority_calculator.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_scrollbar_theme_painter.h"
#include "cc/test/fake_web_scrollbar.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {
namespace {

scoped_ptr<LayerImpl> layerImplForScrollAreaAndScrollbar(
    FakeLayerTreeHostImpl* host_impl,
    scoped_ptr<WebKit::WebScrollbar> scrollbar,
    bool reverse_order)
{
    scoped_refptr<Layer> layerTreeRoot = Layer::Create();
    scoped_refptr<Layer> child1 = Layer::Create();
    scoped_refptr<Layer> child2 = ScrollbarLayer::Create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), child1->id());
    layerTreeRoot->AddChild(child1);
    layerTreeRoot->InsertChild(child2, reverse_order ? 0 : 1);
    scoped_ptr<LayerImpl> layerImpl = TreeSynchronizer::SynchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), host_impl->active_tree());
    TreeSynchronizer::PushProperties(layerTreeRoot.get(), layerImpl.get());
    return layerImpl.Pass();
}

TEST(ScrollbarLayerTest, resolveScrollLayerPointer)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    {
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::Create());
        scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);

        LayerImpl* ccChild1 = layerImplTreeRoot->children()[0];
        ScrollbarLayerImpl* ccChild2 = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

        EXPECT_EQ(ccChild1->horizontal_scrollbar_layer(), ccChild2);
    }

    { // another traverse order
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::Create());
        scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), true);

        ScrollbarLayerImpl* ccChild1 = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[0]);
        LayerImpl* ccChild2 = layerImplTreeRoot->children()[1];

        EXPECT_EQ(ccChild2->horizontal_scrollbar_layer(), ccChild1);
    }
}

TEST(ScrollbarLayerTest, shouldScrollNonOverlayOnMainThread)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    // Create and attach a non-overlay scrollbar.
    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::Create());
    static_cast<FakeWebScrollbar*>(scrollbar.get())->setOverlay(false);
    scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);
    ScrollbarLayerImpl* scrollbarLayerImpl = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

    // When the scrollbar is not an overlay scrollbar, the scroll should be
    // responded to on the main thread as the compositor does not yet implement
    // scrollbar scrolling.
    EXPECT_EQ(InputHandlerClient::ScrollOnMainThread, scrollbarLayerImpl->TryScroll(gfx::Point(0, 0), InputHandlerClient::Gesture));

    // Create and attach an overlay scrollbar.
    scrollbar = FakeWebScrollbar::Create();
    static_cast<FakeWebScrollbar*>(scrollbar.get())->setOverlay(true);

    layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);
    scrollbarLayerImpl = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

    // The user shouldn't be able to drag an overlay scrollbar and the scroll
    // may be handled in the compositor.
    EXPECT_EQ(InputHandlerClient::ScrollIgnored, scrollbarLayerImpl->TryScroll(gfx::Point(0, 0), InputHandlerClient::Gesture));
}

TEST(ScrollbarLayerTest, scrollOffsetSynchronization)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::Create());
    scoped_refptr<Layer> layerTreeRoot = Layer::Create();
    scoped_refptr<Layer> contentLayer = Layer::Create();
    scoped_refptr<Layer> scrollbarLayer = ScrollbarLayer::Create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), layerTreeRoot->id());
    layerTreeRoot->AddChild(contentLayer);
    layerTreeRoot->AddChild(scrollbarLayer);

    layerTreeRoot->SetScrollOffset(gfx::Vector2d(10, 20));
    layerTreeRoot->SetMaxScrollOffset(gfx::Vector2d(30, 50));
    layerTreeRoot->SetBounds(gfx::Size(100, 200));
    contentLayer->SetBounds(gfx::Size(100, 200));

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::SynchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.active_tree());
    TreeSynchronizer::PushProperties(layerTreeRoot.get(), layerImplTreeRoot.get());

    ScrollbarLayerImpl* ccScrollbarLayer = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

    EXPECT_EQ(10, ccScrollbarLayer->CurrentPos());
    EXPECT_EQ(100, ccScrollbarLayer->TotalSize());
    EXPECT_EQ(30, ccScrollbarLayer->Maximum());

    layerTreeRoot->SetScrollOffset(gfx::Vector2d(100, 200));
    layerTreeRoot->SetMaxScrollOffset(gfx::Vector2d(300, 500));
    layerTreeRoot->SetBounds(gfx::Size(1000, 2000));
    contentLayer->SetBounds(gfx::Size(1000, 2000));

    ScrollbarAnimationController* scrollbarController = layerImplTreeRoot->scrollbar_animation_controller();
    layerImplTreeRoot = TreeSynchronizer::SynchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.active_tree());
    TreeSynchronizer::PushProperties(layerTreeRoot.get(), layerImplTreeRoot.get());
    EXPECT_EQ(scrollbarController, layerImplTreeRoot->scrollbar_animation_controller());

    EXPECT_EQ(100, ccScrollbarLayer->CurrentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->TotalSize());
    EXPECT_EQ(300, ccScrollbarLayer->Maximum());

    layerImplTreeRoot->ScrollBy(gfx::Vector2d(12, 34));

    EXPECT_EQ(112, ccScrollbarLayer->CurrentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->TotalSize());
    EXPECT_EQ(300, ccScrollbarLayer->Maximum());
}

TEST(ScrollbarLayerTest, solidColorDrawQuads)
{
    LayerTreeSettings layerTreeSettings;
    layerTreeSettings.solidColorScrollbars = true;
    layerTreeSettings.solidColorScrollbarThicknessDIP = 3;
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(layerTreeSettings, &proxy);

    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::Create());
    static_cast<FakeWebScrollbar*>(scrollbar.get())->setOverlay(true);
    scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);
    ScrollbarLayerImpl* scrollbarLayerImpl = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);
    scrollbarLayerImpl->SetThumbSize(gfx::Size(4, 4));
    scrollbarLayerImpl->SetViewportWithinScrollableArea(
        gfx::RectF(10.f, 0.f, 40.f, 0.f), gfx::SizeF(100.f, 100.f));

    // Thickness should be overridden to 3.
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        scrollbarLayerImpl->AppendQuads(&quadCuller, &data);

        const QuadList& quads = quadCuller.quadList();
        ASSERT_EQ(1, quads.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
        EXPECT_RECT_EQ(gfx::Rect(1, 0, 4, 3), quads[0]->rect);
    }

    // Contents scale should scale the draw quad.
    scrollbarLayerImpl->draw_properties().contents_scale_x = 2;
    scrollbarLayerImpl->draw_properties().contents_scale_y = 2;
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        scrollbarLayerImpl->AppendQuads(&quadCuller, &data);

        const QuadList& quads = quadCuller.quadList();
        ASSERT_EQ(1, quads.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
        EXPECT_RECT_EQ(gfx::Rect(2, 0, 8, 6), quads[0]->rect);
    }
    scrollbarLayerImpl->draw_properties().contents_scale_x = 1;
    scrollbarLayerImpl->draw_properties().contents_scale_y = 1;

    // For solid color scrollbars, position and size should reflect the
    // viewport, not the geometry object.
    scrollbarLayerImpl->SetViewportWithinScrollableArea(
        gfx::RectF(40.f, 0.f, 20.f, 0.f), gfx::SizeF(100.f, 100.f));
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        scrollbarLayerImpl->AppendQuads(&quadCuller, &data);

        const QuadList& quads = quadCuller.quadList();
        ASSERT_EQ(1, quads.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
        EXPECT_RECT_EQ(gfx::Rect(4, 0, 2, 3), quads[0]->rect);
    }

}

class ScrollbarLayerTestMaxTextureSize : public LayerTreeTest {
public:
    ScrollbarLayerTestMaxTextureSize() {}

    void setScrollbarBounds(gfx::Size bounds) {
        bounds_ = bounds;
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->InitializeRendererIfNeeded();

        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::Create());
        m_scrollbarLayer = ScrollbarLayer::Create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), 1);
        m_scrollbarLayer->SetLayerTreeHost(layer_tree_host());
        m_scrollbarLayer->SetBounds(bounds_);
        layer_tree_host()->root_layer()->AddChild(m_scrollbarLayer);

        m_scrollLayer = Layer::Create();
        m_scrollbarLayer->SetScrollLayerId(m_scrollLayer->id());
        layer_tree_host()->root_layer()->AddChild(m_scrollLayer);

        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        const int kMaxTextureSize = impl->GetRendererCapabilities().max_texture_size;

        // Check first that we're actually testing something.
        EXPECT_GT(m_scrollbarLayer->bounds().width(), kMaxTextureSize);

        EXPECT_EQ(m_scrollbarLayer->content_bounds().width(), kMaxTextureSize - 1);
        EXPECT_EQ(m_scrollbarLayer->content_bounds().height(), kMaxTextureSize - 1);

        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    scoped_refptr<ScrollbarLayer> m_scrollbarLayer;
    scoped_refptr<Layer> m_scrollLayer;
    gfx::Size bounds_;
};

TEST_F(ScrollbarLayerTestMaxTextureSize, runTest) {
    scoped_ptr<TestWebGraphicsContext3D> context = TestWebGraphicsContext3D::Create();
    int max_size = 0;
    context->getIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    setScrollbarBounds(gfx::Size(max_size + 100, max_size + 100));
    RunTest(true);
}

class MockLayerTreeHost : public LayerTreeHost {
public:
    MockLayerTreeHost(LayerTreeHostClient* client, const LayerTreeSettings& settings)
        : LayerTreeHost(client, settings)
    {
        Initialize(scoped_ptr<Thread>(NULL));
    }
};


class ScrollbarLayerTestResourceCreation : public testing::Test {
public:
    ScrollbarLayerTestResourceCreation()
        : m_fakeClient(FakeLayerTreeHostClient::DIRECT_3D)
    {
    }

    void testResourceUpload(int expectedResources)
    {
        m_layerTreeHost.reset(new MockLayerTreeHost(&m_fakeClient, m_layerTreeSettings));

        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::Create());
        scoped_refptr<Layer> layerTreeRoot = Layer::Create();
        scoped_refptr<Layer> contentLayer = Layer::Create();
        scoped_refptr<Layer> scrollbarLayer = ScrollbarLayer::Create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), layerTreeRoot->id());
        layerTreeRoot->AddChild(contentLayer);
        layerTreeRoot->AddChild(scrollbarLayer);

        m_layerTreeHost->InitializeRendererIfNeeded();
        m_layerTreeHost->contents_texture_manager()->SetMaxMemoryLimitBytes(1024 * 1024);
        m_layerTreeHost->SetRootLayer(layerTreeRoot);

        scrollbarLayer->SetIsDrawable(true);
        scrollbarLayer->SetBounds(gfx::Size(100, 100));
        layerTreeRoot->SetScrollOffset(gfx::Vector2d(10, 20));
        layerTreeRoot->SetMaxScrollOffset(gfx::Vector2d(30, 50));
        layerTreeRoot->SetBounds(gfx::Size(100, 200));
        contentLayer->SetBounds(gfx::Size(100, 200));
        scrollbarLayer->draw_properties().content_bounds = gfx::Size(100, 200);
        scrollbarLayer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
        scrollbarLayer->CreateRenderSurface();
        scrollbarLayer->draw_properties().render_target = scrollbarLayer;

        testing::Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_EQ(scrollbarLayer->layer_tree_host(), m_layerTreeHost.get());

        PriorityCalculator calculator;
        ResourceUpdateQueue queue;
        OcclusionTracker occlusionTracker(gfx::Rect(), false);

        scrollbarLayer->SetTexturePriorities(calculator);
        m_layerTreeHost->contents_texture_manager()->PrioritizeTextures();
        scrollbarLayer->Update(&queue, &occlusionTracker, NULL);
        EXPECT_EQ(0, queue.fullUploadSize());
        EXPECT_EQ(expectedResources, queue.partialUploadSize());

        testing::Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    }

protected:
    FakeLayerTreeHostClient m_fakeClient;
    LayerTreeSettings m_layerTreeSettings;
    scoped_ptr<MockLayerTreeHost> m_layerTreeHost;
};

TEST_F(ScrollbarLayerTestResourceCreation, resourceUpload)
{
    m_layerTreeSettings.solidColorScrollbars = false;
    testResourceUpload(2);
}

TEST_F(ScrollbarLayerTestResourceCreation, solidColorNoResourceUpload)
{
    m_layerTreeSettings.solidColorScrollbars = true;
    testResourceUpload(0);
}

TEST(ScrollbarLayerTest, pinchZoomScrollbarUpdates)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    scoped_refptr<Layer> layerTreeRoot = Layer::Create();
    layerTreeRoot->SetScrollable(true);

    scoped_refptr<Layer> contentLayer = Layer::Create();
    scoped_ptr<WebKit::WebScrollbar> scrollbar1(FakeWebScrollbar::Create());
    scoped_refptr<Layer> scrollbarLayerHorizontal =
        ScrollbarLayer::Create(scrollbar1.Pass(),
        FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(),
        FakeWebScrollbarThemeGeometry::create(true),
        Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID);
    scoped_ptr<WebKit::WebScrollbar> scrollbar2(FakeWebScrollbar::Create());
    scoped_refptr<Layer> scrollbarLayerVertical =
        ScrollbarLayer::Create(scrollbar2.Pass(),
        FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(),
        FakeWebScrollbarThemeGeometry::create(true),
        Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID);

    layerTreeRoot->AddChild(contentLayer);
    layerTreeRoot->AddChild(scrollbarLayerHorizontal);
    layerTreeRoot->AddChild(scrollbarLayerVertical);

    layerTreeRoot->SetScrollOffset(gfx::Vector2d(10, 20));
    layerTreeRoot->SetMaxScrollOffset(gfx::Vector2d(30, 50));
    layerTreeRoot->SetBounds(gfx::Size(100, 200));
    contentLayer->SetBounds(gfx::Size(100, 200));

    scoped_ptr<LayerImpl> layerImplTreeRoot =
        TreeSynchronizer::SynchronizeTrees(layerTreeRoot.get(),
            scoped_ptr<LayerImpl>(), hostImpl.active_tree());
    TreeSynchronizer::PushProperties(layerTreeRoot.get(),
        layerImplTreeRoot.get());

    ScrollbarLayerImpl* pinchZoomHorizontal = static_cast<ScrollbarLayerImpl*>(
        layerImplTreeRoot->children()[1]);
    ScrollbarLayerImpl* pinchZoomVertical = static_cast<ScrollbarLayerImpl*>(
        layerImplTreeRoot->children()[2]);

    // Need a root layer in the active tree in order for DidUpdateScroll()
    // to work.
    hostImpl.active_tree()->SetRootLayer(layerImplTreeRoot.Pass());
    hostImpl.active_tree()->FindRootScrollLayer();

    // Manually set the pinch-zoom layers: normally this is done by
    // LayerTreeHost.
    hostImpl.active_tree()->SetPinchZoomHorizontalLayerId(
        pinchZoomHorizontal->id());
    hostImpl.active_tree()->SetPinchZoomVerticalLayerId(
        pinchZoomVertical->id());

    hostImpl.active_tree()->DidUpdateScroll();

    EXPECT_EQ(10, pinchZoomHorizontal->CurrentPos());
    EXPECT_EQ(100, pinchZoomHorizontal->TotalSize());
    EXPECT_EQ(30, pinchZoomHorizontal->Maximum());
    EXPECT_EQ(20, pinchZoomVertical->CurrentPos());
    EXPECT_EQ(200, pinchZoomVertical->TotalSize());
    EXPECT_EQ(50, pinchZoomVertical->Maximum());
}

}  // namespace
}  // namespace cc
