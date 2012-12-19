// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_layer.h"

#include "cc/scrollbar_animation_controller.h"
#include "cc/scrollbar_layer_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_scrollbar_theme_painter.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_scrollbar.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/tree_synchronizer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {
namespace {

TEST(ScrollbarLayerTest, resolveScrollLayerPointer)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    {
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        scoped_refptr<Layer> layerTreeRoot = Layer::create();
        scoped_refptr<Layer> child1 = Layer::create();
        scoped_refptr<Layer> child2 = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(), child1->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.activeTree());

        LayerImpl* ccChild1 = layerImplTreeRoot->children()[0];
        ScrollbarLayerImpl* ccChild2 = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

        EXPECT_TRUE(ccChild1->scrollbarAnimationController());
        EXPECT_EQ(ccChild1->horizontalScrollbarLayer(), ccChild2);
    }

    { // another traverse order
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        scoped_refptr<Layer> layerTreeRoot = Layer::create();
        scoped_refptr<Layer> child2 = Layer::create();
        scoped_refptr<Layer> child1 = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(), child2->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.activeTree());

        ScrollbarLayerImpl* ccChild1 = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[0]);
        LayerImpl* ccChild2 = layerImplTreeRoot->children()[1];

        EXPECT_TRUE(ccChild2->scrollbarAnimationController());
        EXPECT_EQ(ccChild2->horizontalScrollbarLayer(), ccChild1);
    }
}

TEST(ScrollbarLayerTest, scrollOffsetSynchronization)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
    scoped_refptr<Layer> layerTreeRoot = Layer::create();
    scoped_refptr<Layer> contentLayer = Layer::create();
    scoped_refptr<Layer> scrollbarLayer = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(), layerTreeRoot->id());
    layerTreeRoot->addChild(contentLayer);
    layerTreeRoot->addChild(scrollbarLayer);

    layerTreeRoot->setScrollOffset(gfx::Vector2d(10, 20));
    layerTreeRoot->setMaxScrollOffset(gfx::Vector2d(30, 50));
    contentLayer->setBounds(gfx::Size(100, 200));

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.activeTree());

    ScrollbarLayerImpl* ccScrollbarLayer = static_cast<ScrollbarLayerImpl*>(layerImplTreeRoot->children()[1]);

    EXPECT_EQ(10, ccScrollbarLayer->currentPos());
    EXPECT_EQ(100, ccScrollbarLayer->totalSize());
    EXPECT_EQ(30, ccScrollbarLayer->maximum());

    layerTreeRoot->setScrollOffset(gfx::Vector2d(100, 200));
    layerTreeRoot->setMaxScrollOffset(gfx::Vector2d(300, 500));
    contentLayer->setBounds(gfx::Size(1000, 2000));

    ScrollbarAnimationController* scrollbarController = layerImplTreeRoot->scrollbarAnimationController();
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.activeTree());
    EXPECT_EQ(scrollbarController, layerImplTreeRoot->scrollbarAnimationController());

    EXPECT_EQ(100, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());

    layerImplTreeRoot->scrollBy(gfx::Vector2d(12, 34));

    EXPECT_EQ(112, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());
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
        m_scrollbarLayer = ScrollbarLayer::create(scrollbar.Pass(), FakeScrollbarThemePainter::Create(false).PassAs<ScrollbarThemePainter>(), FakeWebScrollbarThemeGeometry::create(), 1);
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
        const int kMaxTextureSize = m_layerTreeHost->rendererCapabilities().maxTextureSize;

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
    scoped_ptr<FakeWebGraphicsContext3D> context = FakeWebGraphicsContext3D::create();
    int max_size = 0;
    context->getIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    setScrollbarBounds(gfx::Size(max_size + 100, max_size + 100));
    runTest(true);
}

}  // namespace
}  // namespace cc
