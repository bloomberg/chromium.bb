// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/solid_color_layer_impl.h"

#include "cc/append_quads_data.h"
#include "cc/single_thread_proxy.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace LayerTestCommon;

namespace {

TEST(SolidColorLayerImplTest, verifyTilingCompleteAndNoOverlap)
{
    DebugScopedSetImplThread scopedImplThread;

    MockQuadCuller quadCuller;
    IntSize layerSize = IntSize(800, 600);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    scoped_ptr<SolidColorLayerImpl> layer = SolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    verifyQuadsExactlyCoverRect(quadCuller.quadList(), visibleContentRect);
}

TEST(SolidColorLayerImplTest, verifyCorrectBackgroundColorInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    SkColor testColor = 0xFFA55AFF;

    MockQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    scoped_ptr<SolidColorLayerImpl> layer = SolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->setBackgroundColor(testColor);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(SolidColorDrawQuad::materialCast(quadCuller.quadList()[0])->color(), testColor);
}

TEST(SolidColorLayerImplTest, verifyCorrectOpacityInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    const float opacity = 0.5f;

    MockQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    scoped_ptr<SolidColorLayerImpl> layer = SolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->setDrawOpacity(opacity);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(opacity, SolidColorDrawQuad::materialCast(quadCuller.quadList()[0])->opacity());
}

} // namespace
