// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCLayerIterator.h"

#include "CCLayerTreeHostCommon.h"
#include "LayerChromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebTransformationMatrix.h>

using namespace cc;
using WebKit::WebTransformationMatrix;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace {

class TestLayerChromium : public LayerChromium {
public:
    static scoped_refptr<TestLayerChromium> create() { return make_scoped_refptr(new TestLayerChromium()); }

    int m_countRepresentingTargetSurface;
    int m_countRepresentingContributingSurface;
    int m_countRepresentingItself;

    virtual bool drawsContent() const OVERRIDE { return m_drawsContent; }
    void setDrawsContent(bool drawsContent) { m_drawsContent = drawsContent; }

private:
    TestLayerChromium()
        : LayerChromium()
        , m_drawsContent(true)
    {
        setBounds(IntSize(100, 100));
        setPosition(IntPoint());
        setAnchorPoint(IntPoint());
    }
    virtual ~TestLayerChromium()
    {
    }

    bool m_drawsContent;
};

#define EXPECT_COUNT(layer, target, contrib, itself) \
    EXPECT_EQ(target, layer->m_countRepresentingTargetSurface);          \
    EXPECT_EQ(contrib, layer->m_countRepresentingContributingSurface);   \
    EXPECT_EQ(itself, layer->m_countRepresentingItself);

typedef CCLayerIterator<LayerChromium, std::vector<scoped_refptr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::FrontToBack> FrontToBack;
typedef CCLayerIterator<LayerChromium, std::vector<scoped_refptr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::BackToFront> BackToFront;

void resetCounts(std::vector<scoped_refptr<LayerChromium> >& renderSurfaceLayerList)
{
    for (unsigned surfaceIndex = 0; surfaceIndex < renderSurfaceLayerList.size(); ++surfaceIndex) {
        TestLayerChromium* renderSurfaceLayer = static_cast<TestLayerChromium*>(renderSurfaceLayerList[surfaceIndex].get());
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();

        renderSurfaceLayer->m_countRepresentingTargetSurface = -1;
        renderSurfaceLayer->m_countRepresentingContributingSurface = -1;
        renderSurfaceLayer->m_countRepresentingItself = -1;

        for (unsigned layerIndex = 0; layerIndex < renderSurface->layerList().size(); ++layerIndex) {
            TestLayerChromium* layer = static_cast<TestLayerChromium*>(renderSurface->layerList()[layerIndex].get());

            layer->m_countRepresentingTargetSurface = -1;
            layer->m_countRepresentingContributingSurface = -1;
            layer->m_countRepresentingItself = -1;
        }
    }
}

void iterateFrontToBack(std::vector<scoped_refptr<LayerChromium> >* renderSurfaceLayerList)
{
    resetCounts(*renderSurfaceLayerList);
    int count = 0;
    for (FrontToBack it = FrontToBack::begin(renderSurfaceLayerList); it != FrontToBack::end(renderSurfaceLayerList); ++it, ++count) {
        TestLayerChromium* layer = static_cast<TestLayerChromium*>(*it);
        if (it.representsTargetRenderSurface())
            layer->m_countRepresentingTargetSurface = count;
        if (it.representsContributingRenderSurface())
            layer->m_countRepresentingContributingSurface = count;
        if (it.representsItself())
            layer->m_countRepresentingItself = count;
    }
}

void iterateBackToFront(std::vector<scoped_refptr<LayerChromium> >* renderSurfaceLayerList)
{
    resetCounts(*renderSurfaceLayerList);
    int count = 0;
    for (BackToFront it = BackToFront::begin(renderSurfaceLayerList); it != BackToFront::end(renderSurfaceLayerList); ++it, ++count) {
        TestLayerChromium* layer = static_cast<TestLayerChromium*>(*it);
        if (it.representsTargetRenderSurface())
            layer->m_countRepresentingTargetSurface = count;
        if (it.representsContributingRenderSurface())
            layer->m_countRepresentingContributingSurface = count;
        if (it.representsItself())
            layer->m_countRepresentingItself = count;
    }
}

TEST(CCLayerIteratorTest, emptyTree)
{
    std::vector<scoped_refptr<LayerChromium> > renderSurfaceLayerList;

    iterateBackToFront(&renderSurfaceLayerList);
    iterateFrontToBack(&renderSurfaceLayerList);
}

TEST(CCLayerIteratorTest, simpleTree)
{
    scoped_refptr<TestLayerChromium> rootLayer = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> first = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> second = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> third = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> fourth = TestLayerChromium::create();

    rootLayer->createRenderSurface();

    rootLayer->addChild(first);
    rootLayer->addChild(second);
    rootLayer->addChild(third);
    rootLayer->addChild(fourth);

    std::vector<scoped_refptr<LayerChromium> > renderSurfaceLayerList;
    CCLayerTreeHostCommon::calculateDrawTransforms(rootLayer.get(), rootLayer->bounds(), 1, 1, 256, renderSurfaceLayerList);

    iterateBackToFront(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 0, -1, 1);
    EXPECT_COUNT(first, -1, -1, 2);
    EXPECT_COUNT(second, -1, -1, 3);
    EXPECT_COUNT(third, -1, -1, 4);
    EXPECT_COUNT(fourth, -1, -1, 5);

    iterateFrontToBack(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 5, -1, 4);
    EXPECT_COUNT(first, -1, -1, 3);
    EXPECT_COUNT(second, -1, -1, 2);
    EXPECT_COUNT(third, -1, -1, 1);
    EXPECT_COUNT(fourth, -1, -1, 0);

}

TEST(CCLayerIteratorTest, complexTree)
{
    scoped_refptr<TestLayerChromium> rootLayer = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root1 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root2 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root3 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root21 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root22 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root23 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root221 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root231 = TestLayerChromium::create();

    rootLayer->createRenderSurface();

    rootLayer->addChild(root1);
    rootLayer->addChild(root2);
    rootLayer->addChild(root3);
    root2->addChild(root21);
    root2->addChild(root22);
    root2->addChild(root23);
    root22->addChild(root221);
    root23->addChild(root231);

    std::vector<scoped_refptr<LayerChromium> > renderSurfaceLayerList;
    CCLayerTreeHostCommon::calculateDrawTransforms(rootLayer.get(), rootLayer->bounds(), 1, 1, 256, renderSurfaceLayerList);

    iterateBackToFront(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 0, -1, 1);
    EXPECT_COUNT(root1, -1, -1, 2);
    EXPECT_COUNT(root2, -1, -1, 3);
    EXPECT_COUNT(root21, -1, -1, 4);
    EXPECT_COUNT(root22, -1, -1, 5);
    EXPECT_COUNT(root221, -1, -1, 6);
    EXPECT_COUNT(root23, -1, -1, 7);
    EXPECT_COUNT(root231, -1, -1, 8);
    EXPECT_COUNT(root3, -1, -1, 9);

    iterateFrontToBack(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 9, -1, 8);
    EXPECT_COUNT(root1, -1, -1, 7);
    EXPECT_COUNT(root2, -1, -1, 6);
    EXPECT_COUNT(root21, -1, -1, 5);
    EXPECT_COUNT(root22, -1, -1, 4);
    EXPECT_COUNT(root221, -1, -1, 3);
    EXPECT_COUNT(root23, -1, -1, 2);
    EXPECT_COUNT(root231, -1, -1, 1);
    EXPECT_COUNT(root3, -1, -1, 0);

}

TEST(CCLayerIteratorTest, complexTreeMultiSurface)
{
    scoped_refptr<TestLayerChromium> rootLayer = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root1 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root2 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root3 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root21 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root22 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root23 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root221 = TestLayerChromium::create();
    scoped_refptr<TestLayerChromium> root231 = TestLayerChromium::create();

    rootLayer->createRenderSurface();
    rootLayer->renderSurface()->setContentRect(IntRect(IntPoint(), rootLayer->bounds()));

    rootLayer->addChild(root1);
    rootLayer->addChild(root2);
    rootLayer->addChild(root3);
    root2->setDrawsContent(false);
    root2->setOpacity(0.5); // Force the layer to own a new surface.
    root2->addChild(root21);
    root2->addChild(root22);
    root2->addChild(root23);
    root22->setOpacity(0.5);
    root22->addChild(root221);
    root23->setOpacity(0.5);
    root23->addChild(root231);

    std::vector<scoped_refptr<LayerChromium> > renderSurfaceLayerList;
    CCLayerTreeHostCommon::calculateDrawTransforms(rootLayer.get(), rootLayer->bounds(), 1, 1, 256, renderSurfaceLayerList);

    iterateBackToFront(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 0, -1, 1);
    EXPECT_COUNT(root1, -1, -1, 2);
    EXPECT_COUNT(root2, 4, 3, -1);
    EXPECT_COUNT(root21, -1, -1, 5);
    EXPECT_COUNT(root22, 7, 6, 8);
    EXPECT_COUNT(root221, -1, -1, 9);
    EXPECT_COUNT(root23, 11, 10, 12);
    EXPECT_COUNT(root231, -1, -1, 13);
    EXPECT_COUNT(root3, -1, -1, 14);

    iterateFrontToBack(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 14, -1, 13);
    EXPECT_COUNT(root1, -1, -1, 12);
    EXPECT_COUNT(root2, 10, 11, -1);
    EXPECT_COUNT(root21, -1, -1, 9);
    EXPECT_COUNT(root22, 7, 8, 6);
    EXPECT_COUNT(root221, -1, -1, 5);
    EXPECT_COUNT(root23, 3, 4, 2);
    EXPECT_COUNT(root231, -1, -1, 1);
    EXPECT_COUNT(root3, -1, -1, 0);
}

} // namespace
