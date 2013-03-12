// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_iterator.h"

#include "cc/layer.h"
#include "cc/layer_tree_host_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class TestLayer : public Layer {
public:
    static scoped_refptr<TestLayer> Create() { return make_scoped_refptr(new TestLayer()); }

    int m_countRepresentingTargetSurface;
    int m_countRepresentingContributingSurface;
    int m_countRepresentingItself;

    virtual bool DrawsContent() const OVERRIDE { return m_drawsContent; }
    void setDrawsContent(bool drawsContent) { m_drawsContent = drawsContent; }

private:
    TestLayer()
        : Layer()
        , m_drawsContent(true)
    {
        SetBounds(gfx::Size(100, 100));
        SetPosition(gfx::Point());
        SetAnchorPoint(gfx::Point());
    }
    virtual ~TestLayer()
    {
    }

    bool m_drawsContent;
};

#define EXPECT_COUNT(layer, target, contrib, itself) \
    EXPECT_EQ(target, layer->m_countRepresentingTargetSurface);          \
    EXPECT_EQ(contrib, layer->m_countRepresentingContributingSurface);   \
    EXPECT_EQ(itself, layer->m_countRepresentingItself);

typedef LayerIterator<Layer, std::vector<scoped_refptr<Layer> >, RenderSurface, LayerIteratorActions::FrontToBack> FrontToBack;
typedef LayerIterator<Layer, std::vector<scoped_refptr<Layer> >, RenderSurface, LayerIteratorActions::BackToFront> BackToFront;

void resetCounts(std::vector<scoped_refptr<Layer> >& renderSurfaceLayerList)
{
    for (unsigned surfaceIndex = 0; surfaceIndex < renderSurfaceLayerList.size(); ++surfaceIndex) {
        TestLayer* renderSurfaceLayer = static_cast<TestLayer*>(renderSurfaceLayerList[surfaceIndex].get());
        RenderSurface* renderSurface = renderSurfaceLayer->render_surface();

        renderSurfaceLayer->m_countRepresentingTargetSurface = -1;
        renderSurfaceLayer->m_countRepresentingContributingSurface = -1;
        renderSurfaceLayer->m_countRepresentingItself = -1;

        for (unsigned layerIndex = 0; layerIndex < renderSurface->layer_list().size(); ++layerIndex) {
            TestLayer* layer = static_cast<TestLayer*>(renderSurface->layer_list()[layerIndex].get());

            layer->m_countRepresentingTargetSurface = -1;
            layer->m_countRepresentingContributingSurface = -1;
            layer->m_countRepresentingItself = -1;
        }
    }
}

void iterateFrontToBack(std::vector<scoped_refptr<Layer> >* renderSurfaceLayerList)
{
    resetCounts(*renderSurfaceLayerList);
    int count = 0;
    for (FrontToBack it = FrontToBack::begin(renderSurfaceLayerList); it != FrontToBack::end(renderSurfaceLayerList); ++it, ++count) {
        TestLayer* layer = static_cast<TestLayer*>(*it);
        if (it.representsTargetRenderSurface())
            layer->m_countRepresentingTargetSurface = count;
        if (it.representsContributingRenderSurface())
            layer->m_countRepresentingContributingSurface = count;
        if (it.representsItself())
            layer->m_countRepresentingItself = count;
    }
}

void iterateBackToFront(std::vector<scoped_refptr<Layer> >* renderSurfaceLayerList)
{
    resetCounts(*renderSurfaceLayerList);
    int count = 0;
    for (BackToFront it = BackToFront::begin(renderSurfaceLayerList); it != BackToFront::end(renderSurfaceLayerList); ++it, ++count) {
        TestLayer* layer = static_cast<TestLayer*>(*it);
        if (it.representsTargetRenderSurface())
            layer->m_countRepresentingTargetSurface = count;
        if (it.representsContributingRenderSurface())
            layer->m_countRepresentingContributingSurface = count;
        if (it.representsItself())
            layer->m_countRepresentingItself = count;
    }
}

TEST(LayerIteratorTest, emptyTree)
{
    std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;

    iterateBackToFront(&renderSurfaceLayerList);
    iterateFrontToBack(&renderSurfaceLayerList);
}

TEST(LayerIteratorTest, simpleTree)
{
    scoped_refptr<TestLayer> rootLayer = TestLayer::Create();
    scoped_refptr<TestLayer> first = TestLayer::Create();
    scoped_refptr<TestLayer> second = TestLayer::Create();
    scoped_refptr<TestLayer> third = TestLayer::Create();
    scoped_refptr<TestLayer> fourth = TestLayer::Create();

    rootLayer->CreateRenderSurface();

    rootLayer->AddChild(first);
    rootLayer->AddChild(second);
    rootLayer->AddChild(third);
    rootLayer->AddChild(fourth);

    std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
    LayerTreeHostCommon::calculateDrawProperties(rootLayer.get(), rootLayer->bounds(), 1, 1, 256, false, renderSurfaceLayerList);

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

TEST(LayerIteratorTest, complexTree)
{
    scoped_refptr<TestLayer> rootLayer = TestLayer::Create();
    scoped_refptr<TestLayer> root1 = TestLayer::Create();
    scoped_refptr<TestLayer> root2 = TestLayer::Create();
    scoped_refptr<TestLayer> root3 = TestLayer::Create();
    scoped_refptr<TestLayer> root21 = TestLayer::Create();
    scoped_refptr<TestLayer> root22 = TestLayer::Create();
    scoped_refptr<TestLayer> root23 = TestLayer::Create();
    scoped_refptr<TestLayer> root221 = TestLayer::Create();
    scoped_refptr<TestLayer> root231 = TestLayer::Create();

    rootLayer->CreateRenderSurface();

    rootLayer->AddChild(root1);
    rootLayer->AddChild(root2);
    rootLayer->AddChild(root3);
    root2->AddChild(root21);
    root2->AddChild(root22);
    root2->AddChild(root23);
    root22->AddChild(root221);
    root23->AddChild(root231);

    std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
    LayerTreeHostCommon::calculateDrawProperties(rootLayer.get(), rootLayer->bounds(), 1, 1, 256, false, renderSurfaceLayerList);

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

TEST(LayerIteratorTest, complexTreeMultiSurface)
{
    scoped_refptr<TestLayer> rootLayer = TestLayer::Create();
    scoped_refptr<TestLayer> root1 = TestLayer::Create();
    scoped_refptr<TestLayer> root2 = TestLayer::Create();
    scoped_refptr<TestLayer> root3 = TestLayer::Create();
    scoped_refptr<TestLayer> root21 = TestLayer::Create();
    scoped_refptr<TestLayer> root22 = TestLayer::Create();
    scoped_refptr<TestLayer> root23 = TestLayer::Create();
    scoped_refptr<TestLayer> root221 = TestLayer::Create();
    scoped_refptr<TestLayer> root231 = TestLayer::Create();

    rootLayer->CreateRenderSurface();
    rootLayer->render_surface()->SetContentRect(gfx::Rect(gfx::Point(), rootLayer->bounds()));

    rootLayer->AddChild(root1);
    rootLayer->AddChild(root2);
    rootLayer->AddChild(root3);
    root2->setDrawsContent(false);
    root2->SetOpacity(0.5);
    root2->SetForceRenderSurface(true); // Force the layer to own a new surface.
    root2->AddChild(root21);
    root2->AddChild(root22);
    root2->AddChild(root23);
    root22->SetOpacity(0.5);
    root22->AddChild(root221);
    root23->SetOpacity(0.5);
    root23->AddChild(root231);

    std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
    LayerTreeHostCommon::calculateDrawProperties(rootLayer.get(), rootLayer->bounds(), 1, 1, 256, false, renderSurfaceLayerList);

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

}  // namespace
}  // namespace cc
