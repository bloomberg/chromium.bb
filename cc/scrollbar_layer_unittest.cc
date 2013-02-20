// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_layer.h"

#include "cc/append_quads_data.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/priority_calculator.h"
#include "cc/resource_update_queue.h"
#include "cc/scrollbar_animation_controller.h"
#include "cc/scrollbar_layer_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_scrollbar_theme_painter.h"
#include "cc/test/fake_web_scrollbar.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/tree_synchronizer.h"
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
    scoped_refptr<Layer> layerTreeRoot = Layer::create();
    scoped_refptr<Layer> child1 = Layer::create();
    scoped_refptr<Layer> child2 = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), child1->id());
    layerTreeRoot->addChild(child1);
    layerTreeRoot->insertChild(child2, reverse_order ? 0 : 1);
    scoped_ptr<LayerImpl> layerImpl = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), host_impl->activeTree());
    TreeSynchronizer::pushProperties(layerTreeRoot.get(), layerImpl.get());
    return layerImpl.Pass();
}

TEST(ScrollbarLayerTest, resolveScrollLayerPointer)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    {
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);

        LayerImpl* ccChild1 = layerImplTreeRoot->children()[0];
        ScrollbarLayerImpl* ccChild2 = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

        EXPECT_EQ(ccChild1->horizontalScrollbarLayer(), ccChild2);
    }

    { // another traverse order
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), true);

        ScrollbarLayerImpl* ccChild1 = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[0]);
        LayerImpl* ccChild2 = layerImplTreeRoot->children()[1];

        EXPECT_EQ(ccChild2->horizontalScrollbarLayer(), ccChild1);
    }
}

TEST(ScrollbarLayerTest, shouldScrollNonOverlayOnMainThread)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    // Create and attach a non-overlay scrollbar.
    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
    static_cast<FakeWebScrollbar*>(scrollbar.get())->setOverlay(false);
    scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);
    ScrollbarLayerImpl* scrollbarLayerImpl = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

    // When the scrollbar is not an overlay scrollbar, the scroll should be
    // responded to on the main thread as the compositor does not yet implement
    // scrollbar scrolling.
    EXPECT_EQ(InputHandlerClient::ScrollOnMainThread, scrollbarLayerImpl->tryScroll(gfx::Point(0, 0), InputHandlerClient::Gesture));

    // Create and attach an overlay scrollbar.
    scrollbar = FakeWebScrollbar::create();
    static_cast<FakeWebScrollbar*>(scrollbar.get())->setOverlay(true);

    layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);
    scrollbarLayerImpl = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

    // The user shouldn't be able to drag an overlay scrollbar and the scroll
    // may be handled in the compositor.
    EXPECT_EQ(InputHandlerClient::ScrollIgnored, scrollbarLayerImpl->tryScroll(gfx::Point(0, 0), InputHandlerClient::Gesture));
}

TEST(ScrollbarLayerTest, scrollOffsetSynchronization)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
    scoped_refptr<Layer> layerTreeRoot = Layer::create();
    scoped_refptr<Layer> contentLayer = Layer::create();
    scoped_refptr<Layer> scrollbarLayer = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), layerTreeRoot->id());
    layerTreeRoot->addChild(contentLayer);
    layerTreeRoot->addChild(scrollbarLayer);

    layerTreeRoot->setScrollOffset(gfx::Vector2d(10, 20));
    layerTreeRoot->setMaxScrollOffset(gfx::Vector2d(30, 50));
    layerTreeRoot->setBounds(gfx::Size(100, 200));
    contentLayer->setBounds(gfx::Size(100, 200));

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.activeTree());
    TreeSynchronizer::pushProperties(layerTreeRoot.get(), layerImplTreeRoot.get());

    ScrollbarLayerImpl* ccScrollbarLayer = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

    EXPECT_EQ(10, ccScrollbarLayer->currentPos());
    EXPECT_EQ(100, ccScrollbarLayer->totalSize());
    EXPECT_EQ(30, ccScrollbarLayer->maximum());

    layerTreeRoot->setScrollOffset(gfx::Vector2d(100, 200));
    layerTreeRoot->setMaxScrollOffset(gfx::Vector2d(300, 500));
    layerTreeRoot->setBounds(gfx::Size(1000, 2000));
    contentLayer->setBounds(gfx::Size(1000, 2000));

    ScrollbarAnimationController* scrollbarController = layerImplTreeRoot->scrollbarAnimationController();
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.activeTree());
    TreeSynchronizer::pushProperties(layerTreeRoot.get(), layerImplTreeRoot.get());
    EXPECT_EQ(scrollbarController, layerImplTreeRoot->scrollbarAnimationController());

    EXPECT_EQ(100, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());

    layerImplTreeRoot->scrollBy(gfx::Vector2d(12, 34));

    EXPECT_EQ(112, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());
}

TEST(ScrollbarLayerTest, solidColorThicknessOverride)
{
    LayerTreeSettings layerTreeSettings;
    layerTreeSettings.solidColorScrollbars = true;
    layerTreeSettings.solidColorScrollbarThicknessDIP = 3;
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(layerTreeSettings, &proxy);

    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
    static_cast<FakeWebScrollbar*>(scrollbar.get())->setOverlay(true);
    scoped_ptr<LayerImpl> layerImplTreeRoot = layerImplForScrollAreaAndScrollbar(&hostImpl, scrollbar.Pass(), false);
    ScrollbarLayerImpl* scrollbarLayerImpl = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);
    scrollbarLayerImpl->setThumbSize(gfx::Size(4, 4));

    // Thickness should be overridden to 3.
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        scrollbarLayerImpl->appendQuads(quadCuller, data);

        const QuadList& quads = quadCuller.quadList();
        ASSERT_EQ(1, quads.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
        EXPECT_RECT_EQ(gfx::Rect(1, 0, 4, 3), quads[0]->rect);
    }

    // Contents scale should scale the draw quad.
    scrollbarLayerImpl->drawProperties().contents_scale_x = 2;
    scrollbarLayerImpl->drawProperties().contents_scale_y = 2;
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        scrollbarLayerImpl->appendQuads(quadCuller, data);

        const QuadList& quads = quadCuller.quadList();
        ASSERT_EQ(1, quads.size());
        EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
        EXPECT_RECT_EQ(gfx::Rect(2, 0, 8, 6), quads[0]->rect);
    }

}

class ScrollbarLayerTestMaxTextureSize : public ThreadedTest {
public:
    ScrollbarLayerTestMaxTextureSize() {}

    void setScrollbarBounds(gfx::Size bounds) {
        m_bounds = bounds;
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->initializeRendererIfNeeded();

        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        m_scrollbarLayer = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), 1);
        m_scrollbarLayer->setLayerTreeHost(m_layerTreeHost.get());
        m_scrollbarLayer->setBounds(m_bounds);
        m_layerTreeHost->rootLayer()->addChild(m_scrollbarLayer);

        m_scrollLayer = Layer::create();
        m_scrollbarLayer->setScrollLayerId(m_scrollLayer->id());
        m_layerTreeHost->rootLayer()->addChild(m_scrollLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        const int kMaxTextureSize = impl->rendererCapabilities().maxTextureSize;

        // Check first that we're actually testing something.
        EXPECT_GT(m_scrollbarLayer->bounds().width(), kMaxTextureSize);

        EXPECT_EQ(m_scrollbarLayer->contentBounds().width(), kMaxTextureSize - 1);
        EXPECT_EQ(m_scrollbarLayer->contentBounds().height(), kMaxTextureSize - 1);

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    scoped_refptr<ScrollbarLayer> m_scrollbarLayer;
    scoped_refptr<Layer> m_scrollLayer;
    gfx::Size m_bounds;
};

TEST_F(ScrollbarLayerTestMaxTextureSize, runTest) {
    scoped_ptr<TestWebGraphicsContext3D> context = TestWebGraphicsContext3D::Create();
    int max_size = 0;
    context->getIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    setScrollbarBounds(gfx::Size(max_size + 100, max_size + 100));
    runTest(true);
}

class MockLayerTreeHost : public LayerTreeHost {
public:
    MockLayerTreeHost(const LayerTreeSettings& settings)
        : LayerTreeHost(&m_fakeClient, settings)
    {
        initialize(scoped_ptr<Thread>(NULL));
    }

private:
    FakeLayerImplTreeHostClient m_fakeClient;
};


class ScrollbarLayerTestResourceCreation : public testing::Test {
public:
    ScrollbarLayerTestResourceCreation()
    {
    }

    void testResourceUpload(int expectedResources)
    {
        m_layerTreeHost.reset(new MockLayerTreeHost(m_layerTreeSettings));

        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        scoped_refptr<Layer> layerTreeRoot = Layer::create();
        scoped_refptr<Layer> contentLayer = Layer::create();
        scoped_refptr<Layer> scrollbarLayer = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(true), layerTreeRoot->id());
        layerTreeRoot->addChild(contentLayer);
        layerTreeRoot->addChild(scrollbarLayer);

        m_layerTreeHost->initializeRendererIfNeeded();
        m_layerTreeHost->contentsTextureManager()->setMaxMemoryLimitBytes(1024 * 1024);
        m_layerTreeHost->setRootLayer(layerTreeRoot);

        scrollbarLayer->setIsDrawable(true);
        scrollbarLayer->setBounds(gfx::Size(100, 100));
        layerTreeRoot->setScrollOffset(gfx::Vector2d(10, 20));
        layerTreeRoot->setMaxScrollOffset(gfx::Vector2d(30, 50));
        layerTreeRoot->setBounds(gfx::Size(100, 200));
        contentLayer->setBounds(gfx::Size(100, 200));
        scrollbarLayer->drawProperties().content_bounds = gfx::Size(100, 200);
        scrollbarLayer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
        scrollbarLayer->createRenderSurface();
        scrollbarLayer->drawProperties().render_target = scrollbarLayer;

        testing::Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_EQ(scrollbarLayer->layerTreeHost(), m_layerTreeHost.get());

        PriorityCalculator calculator;
        ResourceUpdateQueue queue;
        OcclusionTracker occlusionTracker(gfx::Rect(), false);

        scrollbarLayer->setTexturePriorities(calculator);
        m_layerTreeHost->contentsTextureManager()->prioritizeTextures();
        scrollbarLayer->update(queue, &occlusionTracker, NULL);
        EXPECT_EQ(0, queue.fullUploadSize());
        EXPECT_EQ(expectedResources, queue.partialUploadSize());

        testing::Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    }

protected:
    scoped_ptr<MockLayerTreeHost> m_layerTreeHost;
    LayerTreeSettings m_layerTreeSettings;
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

}  // namespace
}  // namespace cc
