// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/content_layer.h"

#include "cc/bitmap_content_layer_updater.h"
#include "cc/content_layer_client.h"
#include "cc/rendering_stats.h"
#include "cc/resource_update_queue.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"

using namespace WebKit;

namespace cc {
namespace {

class FakeContentLayer : public ContentLayer {
public:
    explicit FakeContentLayer(ContentLayerClient* client) : ContentLayer(client) { }

    virtual void setNeedsDisplayRect(const gfx::RectF& dirtyRect) OVERRIDE
    {
        m_lastDirtyRect = dirtyRect;
        ContentLayer::setNeedsDisplayRect(dirtyRect);
    }
    gfx::RectF lastDirtyRect() const { return m_lastDirtyRect; }

protected:
    virtual ~FakeContentLayer() { }
    virtual void createUpdaterIfNeeded() OVERRIDE { }

private:
    gfx::RectF m_lastDirtyRect;
};

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
    RenderingStats stats;
    updater->prepareToUpdate(contentRect, gfx::Size(256, 256), contentsScale, contentsScale, resultingOpaqueRect, stats);

    EXPECT_RECT_EQ(gfx::ToEnclosingRect(opaqueRectInContentSpace), resultingOpaqueRect);
}

TEST(ContentLayerTest, UseLCDTextEnableCount)
{
    scoped_refptr<FakeContentLayer> layer = new FakeContentLayer(NULL);
    layer->setBounds(gfx::Size(100, 100));
    ResourceUpdateQueue queue;
    RenderingStats stats;

    // By default LCD text is disabled.
    EXPECT_FALSE(layer->useLCDText());

    // LCD text can be enabled once.
    layer->drawProperties().can_use_lcd_text = true;
    layer->update(queue, NULL, stats);
    EXPECT_TRUE(layer->useLCDText());

    // LCD text can always be disabled.
    layer->drawProperties().can_use_lcd_text = false;
    layer->update(queue, NULL, stats);
    EXPECT_FALSE(layer->useLCDText());

    // LCD text cannot be enabled anymore.
    layer->drawProperties().can_use_lcd_text = true;
    layer->update(queue, NULL, stats);
    EXPECT_FALSE(layer->useLCDText());
}

TEST(ContentLayerTest, UseLCDTextChangeTriggersRepaint)
{
    scoped_refptr<FakeContentLayer> layer = new FakeContentLayer(NULL);
    layer->setBounds(gfx::Size(100, 100));
    gfx::RectF dirtyRect(100, 100);
    ResourceUpdateQueue queue;
    RenderingStats stats;

    // By default LCD text is disabled.
    EXPECT_FALSE(layer->useLCDText());

    // Enable LCD text and verify that it triggers invalidation.
    layer->drawProperties().can_use_lcd_text = true;
    layer->update(queue, NULL, stats);
    EXPECT_TRUE(layer->useLCDText());
    EXPECT_EQ(dirtyRect, layer->lastDirtyRect());

    // Reset dirty rect.
    layer->setNeedsDisplayRect(gfx::RectF());
    EXPECT_EQ(gfx::RectF(), layer->lastDirtyRect());

    // Disable LCD text and verify that it triggers invalidation.
    layer->drawProperties().can_use_lcd_text = false;
    layer->update(queue, NULL, stats);
    EXPECT_FALSE(layer->useLCDText());
    EXPECT_EQ(dirtyRect, layer->lastDirtyRect());
}

}  // namespace
}  // namespace cc
