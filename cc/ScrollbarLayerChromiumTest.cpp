// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ScrollbarLayerChromium.h"

#include "CCScrollbarAnimationController.h"
#include "CCScrollbarLayerImpl.h"
#include "CCSingleThreadProxy.h"
#include "FakeWebScrollbarThemeGeometry.h"
#include "TreeSynchronizer.h"
#include <gtest/gtest.h>
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

using namespace cc;

namespace {

class FakeWebScrollbar : public WebKit::WebScrollbar {
public:
    static PassOwnPtr<FakeWebScrollbar> create() { return adoptPtr(new FakeWebScrollbar()); }

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

TEST(ScrollbarLayerChromiumTest, resolveScrollLayerPointer)
{
    DebugScopedSetImplThread impl;

    WebKit::WebScrollbarThemePainter painter;

    {
        OwnPtr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
        RefPtr<LayerChromium> child1 = LayerChromium::create();
        RefPtr<LayerChromium> child2 = ScrollbarLayerChromium::create(scrollbar.release(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), child1->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr, 0);

        CCLayerImpl* ccChild1 = ccLayerTreeRoot->children()[0].get();
        CCScrollbarLayerImpl* ccChild2 = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[1].get());

        EXPECT_TRUE(ccChild1->scrollbarAnimationController());
        EXPECT_EQ(ccChild1->horizontalScrollbarLayer(), ccChild2);
    }

    { // another traverse order
        OwnPtr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
        RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
        RefPtr<LayerChromium> child2 = LayerChromium::create();
        RefPtr<LayerChromium> child1 = ScrollbarLayerChromium::create(scrollbar.release(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), child2->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr, 0);

        CCScrollbarLayerImpl* ccChild1 = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[0].get());
        CCLayerImpl* ccChild2 = ccLayerTreeRoot->children()[1].get();

        EXPECT_TRUE(ccChild2->scrollbarAnimationController());
        EXPECT_EQ(ccChild2->horizontalScrollbarLayer(), ccChild1);
    }
}

TEST(ScrollbarLayerChromiumTest, scrollOffsetSynchronization)
{
    DebugScopedSetImplThread impl;

    WebKit::WebScrollbarThemePainter painter;

    OwnPtr<WebKit::WebScrollbar> scrollbar(FakeWebScrollbar::create());
    RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
    RefPtr<LayerChromium> contentLayer = LayerChromium::create();
    RefPtr<LayerChromium> scrollbarLayer = ScrollbarLayerChromium::create(scrollbar.release(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), layerTreeRoot->id());
    layerTreeRoot->addChild(contentLayer);
    layerTreeRoot->addChild(scrollbarLayer);

    layerTreeRoot->setScrollPosition(IntPoint(10, 20));
    layerTreeRoot->setMaxScrollPosition(IntSize(30, 50));
    contentLayer->setBounds(IntSize(100, 200));

    OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr, 0);

    CCScrollbarLayerImpl* ccScrollbarLayer = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[1].get());

    EXPECT_EQ(10, ccScrollbarLayer->currentPos());
    EXPECT_EQ(100, ccScrollbarLayer->totalSize());
    EXPECT_EQ(30, ccScrollbarLayer->maximum());

    layerTreeRoot->setScrollPosition(IntPoint(100, 200));
    layerTreeRoot->setMaxScrollPosition(IntSize(300, 500));
    contentLayer->setBounds(IntSize(1000, 2000));

    CCScrollbarAnimationController* scrollbarController = ccLayerTreeRoot->scrollbarAnimationController();
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.release(), 0);
    EXPECT_EQ(scrollbarController, ccLayerTreeRoot->scrollbarAnimationController());

    EXPECT_EQ(100, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());

    ccLayerTreeRoot->scrollBy(FloatSize(12, 34));

    EXPECT_EQ(112, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());
}

}
