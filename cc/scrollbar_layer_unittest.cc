// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_layer.h"

#include "cc/scrollbar_animation_controller.h"
#include "cc/scrollbar_layer_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/tree_synchronizer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

namespace cc {
namespace {

class FakeWebScrollbar : public WebKit::WebScrollbar {
public:
    static scoped_ptr<FakeWebScrollbar> create() { return make_scoped_ptr(new FakeWebScrollbar()); }

    // WebScrollbar implementation
    virtual bool isOverlay() const OVERRIDE { return false; }
    virtual int value() const OVERRIDE { return 0; }
    virtual WebKit::WebPoint location() const OVERRIDE { return WebKit::WebPoint(); }
    virtual WebKit::WebSize size() const OVERRIDE { return WebKit::WebSize(); }
    virtual bool enabled() const OVERRIDE { return true; }
    virtual int maximum() const OVERRIDE { return 0; }
    virtual int totalSize() const OVERRIDE { return 0; }
    virtual bool isScrollViewScrollbar() const OVERRIDE { return false; }
    virtual bool isScrollableAreaActive() const OVERRIDE { return true; }
    virtual void getTickmarks(WebKit::WebVector<WebKit::WebRect>&) const OVERRIDE { }
    virtual ScrollbarControlSize controlSize() const OVERRIDE { return WebScrollbar::RegularScrollbar; }
    virtual ScrollbarPart pressedPart() const OVERRIDE { return WebScrollbar::NoPart; }
    virtual ScrollbarPart hoveredPart() const OVERRIDE { return WebScrollbar::NoPart; }
    virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const OVERRIDE { return WebScrollbar::ScrollbarOverlayStyleDefault; }
    virtual bool isCustomScrollbar() const OVERRIDE { return false; }
    virtual Orientation orientation() const OVERRIDE { return WebScrollbar::Horizontal; }
};

TEST(ScrollbarLayerTest, resolveScrollLayerPointer)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    WebKit::WebScrollbarThemePainter painter;

    {
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        scoped_refptr<Layer> layerTreeRoot = Layer::create();
        scoped_refptr<Layer> child1 = Layer::create();
        scoped_refptr<Layer> child2 = ScrollbarLayer::create(scrollbar.Pass(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), child1->id());
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
        scoped_refptr<Layer> child1 = ScrollbarLayer::create(scrollbar.Pass(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), child2->id());
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
    WebKit::WebScrollbarThemePainter painter;

    scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
    scoped_refptr<Layer> layerTreeRoot = Layer::create();
    scoped_refptr<Layer> contentLayer = Layer::create();
    scoped_refptr<Layer> scrollbarLayer = ScrollbarLayer::create(scrollbar.Pass(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), layerTreeRoot->id());
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
        scoped_ptr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        m_scrollbarLayer = ScrollbarLayer::create(scrollbar.Pass(), m_painter, WebKit::FakeWebScrollbarThemeGeometry::create(), 1);
        m_scrollbarLayer->setBounds(m_bounds);
        m_layerTreeHost->rootLayer()->addChild(m_scrollbarLayer);

        m_scrollLayer = Layer::create();
        m_scrollbarLayer->setScrollLayerId(m_scrollLayer->id());
        m_layerTreeHost->rootLayer()->addChild(m_scrollLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_layerTreeHost->initializeRendererIfNeeded();

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
    WebKit::WebScrollbarThemePainter m_painter;
    gfx::Size m_bounds;
};

TEST_F(ScrollbarLayerTestMaxTextureSize, runTest) {
    WebKit::FakeWebGraphicsContext3D context;
    int max_size = 0;
    context.getIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    setScrollbarBounds(gfx::Size(max_size + 100, max_size + 100));
    runTest(true);
}

}  // namespace
}  // namespace cc
