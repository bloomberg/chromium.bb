// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/content_layer.h"

#include "cc/bitmap_content_layer_updater.h"
#include "cc/content_layer_client.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"

using namespace WebKit;

namespace cc {
namespace {

class MockContentLayerClient : public ContentLayerClient {
public:
    explicit MockContentLayerClient(gfx::Rect opaqueLayerRect)
        : m_opaqueLayerRect(opaqueLayerRect)
    {
    }

    virtual void paintContents(SkCanvas*, const gfx::Rect&, gfx::RectF& opaque) OVERRIDE
    {
        opaque = gfx::RectF(m_opaqueLayerRect);
    }

private:
    gfx::Rect m_opaqueLayerRect;
};

TEST(ContentLayerTest, ContentLayerPainterWithDeviceScale)
{
    float contentsScale = 2;
    gfx::Rect contentRect(10, 10, 100, 100);
    gfx::Rect opaqueRectInLayerSpace(5, 5, 20, 20);
    gfx::RectF opaqueRectInContentSpace = gfx::ScaleRect(opaqueRectInLayerSpace, contentsScale, contentsScale);
    MockContentLayerClient client(opaqueRectInLayerSpace);
    scoped_refptr<BitmapContentLayerUpdater> updater = BitmapContentLayerUpdater::create(ContentLayerPainter::create(&client).PassAs<LayerPainter>());

    gfx::Rect resultingOpaqueRect;
    updater->prepareToUpdate(contentRect, gfx::Size(256, 256), contentsScale, contentsScale, resultingOpaqueRect, NULL);

    EXPECT_RECT_EQ(gfx::ToEnclosingRect(opaqueRectInContentSpace), resultingOpaqueRect);
}

}  // namespace
}  // namespace cc
