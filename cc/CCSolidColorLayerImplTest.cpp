// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCSolidColorLayerImpl.h"

#include "CCAppendQuadsData.h"
#include "CCLayerTestCommon.h"
#include "CCSingleThreadProxy.h"
#include "CCSolidColorDrawQuad.h"
#include "MockCCQuadCuller.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace cc;
using namespace CCLayerTestCommon;

namespace {

TEST(CCSolidColorLayerImplTest, verifyTilingCompleteAndNoOverlap)
{
    DebugScopedSetImplThread scopedImplThread;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(800, 600);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    CCAppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    verifyQuadsExactlyCoverRect(quadCuller.quadList(), visibleContentRect);
}

TEST(CCSolidColorLayerImplTest, verifyCorrectBackgroundColorInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    SkColor testColor = 0xFFA55AFF;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->setBackgroundColor(testColor);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    CCAppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(CCSolidColorDrawQuad::materialCast(quadCuller.quadList()[0].get())->color(), testColor);
}

TEST(CCSolidColorLayerImplTest, verifyCorrectOpacityInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    const float opacity = 0.5f;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->setDrawOpacity(opacity);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    CCAppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(opacity, CCSolidColorDrawQuad::materialCast(quadCuller.quadList()[0].get())->opacity());
}

} // namespace
