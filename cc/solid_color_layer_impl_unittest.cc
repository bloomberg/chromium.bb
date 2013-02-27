// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/solid_color_layer_impl.h"

#include "cc/append_quads_data.h"
#include "cc/single_thread_proxy.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/solid_color_layer.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SolidColorLayerImplTest, verifyTilingCompleteAndNoOverlap)
{
    MockQuadCuller quadCuller;
    gfx::Size layerSize = gfx::Size(800, 600);
    gfx::Rect visibleContentRect = gfx::Rect(gfx::Point(), layerSize);

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<SolidColorLayerImpl> layer = SolidColorLayerImpl::create(hostImpl.activeTree(), 1);
    layer->drawProperties().visible_content_rect = visibleContentRect;
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->createRenderSurface();
    layer->drawProperties().render_target = layer.get();

    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    LayerTestCommon::verifyQuadsExactlyCoverRect(quadCuller.quadList(), visibleContentRect);
}

TEST(SolidColorLayerImplTest, verifyCorrectBackgroundColorInQuad)
{
    SkColor testColor = 0xFFA55AFF;

    MockQuadCuller quadCuller;
    gfx::Size layerSize = gfx::Size(100, 100);
    gfx::Rect visibleContentRect = gfx::Rect(gfx::Point(), layerSize);

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<SolidColorLayerImpl> layer = SolidColorLayerImpl::create(hostImpl.activeTree(), 1);
    layer->drawProperties().visible_content_rect = visibleContentRect;
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->setBackgroundColor(testColor);
    layer->createRenderSurface();
    layer->drawProperties().render_target = layer.get();

    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(SolidColorDrawQuad::MaterialCast(quadCuller.quadList()[0])->color, testColor);
}

TEST(SolidColorLayerImplTest, verifyCorrectOpacityInQuad)
{
    const float opacity = 0.5f;

    MockQuadCuller quadCuller;
    gfx::Size layerSize = gfx::Size(100, 100);
    gfx::Rect visibleContentRect = gfx::Rect(gfx::Point(), layerSize);

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<SolidColorLayerImpl> layer = SolidColorLayerImpl::create(hostImpl.activeTree(), 1);
    layer->drawProperties().visible_content_rect = visibleContentRect;
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->drawProperties().opacity = opacity;
    layer->createRenderSurface();
    layer->drawProperties().render_target = layer.get();

    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(opacity, SolidColorDrawQuad::MaterialCast(quadCuller.quadList()[0])->opacity());
}

TEST(SolidColorLayerImplTest, verifyOpaqueRect)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    gfx::Size layerSize = gfx::Size(100, 100);
    gfx::Rect visibleContentRect = gfx::Rect(gfx::Point(), layerSize);

    scoped_refptr<SolidColorLayer> layer = SolidColorLayer::create();
    layer->setBounds(layerSize);
    layer->setForceRenderSurface(true);

    scoped_refptr<Layer> root = Layer::create();
    root->addChild(layer);

    std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
    LayerTreeHostCommon::calculateDrawProperties(
        root,
        gfx::Size(500, 500),
        1,
        1,
        1024,
        false,
        renderSurfaceLayerList);

    EXPECT_FALSE(layer->contentsOpaque());
    layer->setBackgroundColor(SkColorSetARGBInline(255, 10, 20, 30));
    EXPECT_TRUE(layer->contentsOpaque());

    {
        scoped_ptr<SolidColorLayerImpl> layerImpl = SolidColorLayerImpl::create(hostImpl.activeTree(), layer->id());
        layer->pushPropertiesTo(layerImpl.get());

        // The impl layer should call itself opaque as well.
        EXPECT_TRUE(layerImpl->contentsOpaque());

        // Impl layer has 1 opacity, and the color is opaque, so the opaqueRect should be the full tile.
        layerImpl->drawProperties().opacity = 1;

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layerImpl->appendQuads(quadCuller, data);

        ASSERT_EQ(quadCuller.quadList().size(), 1U);
        EXPECT_EQ(visibleContentRect.ToString(), quadCuller.quadList()[0]->opaque_rect.ToString());
    }

    EXPECT_TRUE(layer->contentsOpaque());
    layer->setBackgroundColor(SkColorSetARGBInline(254, 10, 20, 30));
    EXPECT_FALSE(layer->contentsOpaque());

    {
        scoped_ptr<SolidColorLayerImpl> layerImpl = SolidColorLayerImpl::create(hostImpl.activeTree(), layer->id());
        layer->pushPropertiesTo(layerImpl.get());

        // The impl layer should callnot itself opaque anymore.
        EXPECT_FALSE(layerImpl->contentsOpaque());

        // Impl layer has 1 opacity, but the color is not opaque, so the opaque_rect should be empty.
        layerImpl->drawProperties().opacity = 1;

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layerImpl->appendQuads(quadCuller, data);

        ASSERT_EQ(quadCuller.quadList().size(), 1U);
        EXPECT_EQ(gfx::Rect().ToString(), quadCuller.quadList()[0]->opaque_rect.ToString());
    }
}

}  // namespace
}  // namespace cc
