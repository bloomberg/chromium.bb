// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/content_layer.h"

#include "cc/bitmap_content_layer_updater.h"
#include "cc/content_layer_client.h"
#include "cc/rendering_stats.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebFloatRect.h>
#include <public/WebRect.h>

using namespace cc;
using namespace WebKit;

namespace {

class MockContentLayerClient : public ContentLayerClient {
public:
    explicit MockContentLayerClient(IntRect opaqueLayerRect)
        : m_opaqueLayerRect(opaqueLayerRect)
    {
    }

    virtual void paintContents(SkCanvas*, const IntRect&, FloatRect& opaque) OVERRIDE
    {
        opaque = FloatRect(m_opaqueLayerRect);
    }

private:
    IntRect m_opaqueLayerRect;
};

TEST(ContentLayerTest, ContentLayerPainterWithDeviceScale)
{
    float contentsScale = 2;
    IntRect contentRect(10, 10, 100, 100);
    IntRect opaqueRectInLayerSpace(5, 5, 20, 20);
    IntRect opaqueRectInContentSpace = opaqueRectInLayerSpace;
    opaqueRectInContentSpace.scale(contentsScale);
    MockContentLayerClient client(opaqueRectInLayerSpace);
    scoped_refptr<BitmapContentLayerUpdater> updater = BitmapContentLayerUpdater::create(ContentLayerPainter::create(&client).PassAs<LayerPainter>());

    IntRect resultingOpaqueRect;
    RenderingStats stats;
    updater->prepareToUpdate(contentRect, IntSize(256, 256), contentsScale, contentsScale, resultingOpaqueRect, stats);

    EXPECT_RECT_EQ(opaqueRectInContentSpace, resultingOpaqueRect);
}

} // namespace
