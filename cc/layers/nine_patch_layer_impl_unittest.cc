// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "cc/layers/append_quads_data.h"
#include "cc/layers/nine_patch_layer_impl.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

gfx::Rect ToRoundedIntRect(gfx::RectF rect_f) {
    return gfx::Rect(gfx::ToRoundedInt(rect_f.x()), gfx::ToRoundedInt(rect_f.y()), gfx::ToRoundedInt(rect_f.width()), gfx::ToRoundedInt(rect_f.height()));
}

TEST(NinePatchLayerImplTest, verifyDrawQuads)
{
    // Input is a 100x100 bitmap with a 40x50 aperture at x=20, y=30.
    // The bounds of the layer are set to 400x400, so the draw quads
    // generated should leave the border width (40) intact.
    MockQuadCuller quadCuller;
    gfx::Size bitmapSize(100, 100);
    gfx::Size layerSize(400, 400);
    gfx::Rect visibleContentRect(gfx::Point(), layerSize);
    gfx::Rect apertureRect(20, 30, 40, 50);
    gfx::Rect scaledApertureNonUniform(20, 30, 340, 350);

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<NinePatchLayerImpl> layer = NinePatchLayerImpl::Create(hostImpl.active_tree(), 1);
    layer->draw_properties().visible_content_rect = visibleContentRect;
    layer->SetBounds(layerSize);
    layer->SetContentBounds(layerSize);
    layer->CreateRenderSurface();
    layer->draw_properties().render_target = layer.get();
    layer->SetLayout(bitmapSize, apertureRect);
    layer->SetResourceId(1);

    // This scale should not affect the generated quad geometry, but only
    // the shared draw transform.
    gfx::Transform transform;
    transform.Scale(10, 10);
    layer->draw_properties().target_space_transform = transform;

    AppendQuadsData data;
    layer->AppendQuads(&quadCuller, &data);

    // Verify quad rects
    const QuadList& quads = quadCuller.quad_list();
    EXPECT_EQ(8, quads.size());
    Region remaining(visibleContentRect);
    for (size_t i = 0; i < quads.size(); ++i) {
        DrawQuad* quad = quads[i];
        gfx::Rect quadRect = quad->rect;

        EXPECT_TRUE(visibleContentRect.Contains(quadRect)) << i;
        EXPECT_TRUE(remaining.Contains(quadRect)) << i;
        EXPECT_EQ(transform, quad->quadTransform());
        remaining.Subtract(Region(quadRect));
    }
    EXPECT_RECT_EQ(scaledApertureNonUniform, remaining.bounds());
    Region scaledApertureRegion(scaledApertureNonUniform);
    EXPECT_EQ(scaledApertureRegion, remaining);

    // Verify UV rects
    gfx::Rect bitmapRect(gfx::Point(), bitmapSize);
    Region texRemaining(bitmapRect);
    for (size_t i = 0; i < quads.size(); ++i) {
        DrawQuad* quad = quads[i];
        const TextureDrawQuad* texQuad = TextureDrawQuad::MaterialCast(quad);
        gfx::RectF texRect = gfx::BoundingRect(texQuad->uv_top_left, texQuad->uv_bottom_right);
        texRect.Scale(bitmapSize.width(), bitmapSize.height());
        texRemaining.Subtract(Region(ToRoundedIntRect(texRect)));
    }
    EXPECT_RECT_EQ(apertureRect, texRemaining.bounds());
    Region apertureRegion(apertureRect);
    EXPECT_EQ(apertureRegion, texRemaining);
}

TEST(NinePatchLayerImplTest, verifyDrawQuadsForSqueezedLayer)
{
    // Test with a layer much smaller than the bitmap.
    MockQuadCuller quadCuller;
    gfx::Size bitmapSize(101, 101);
    gfx::Size layerSize(51, 51);
    gfx::Rect visibleContentRect(gfx::Point(), layerSize);
    gfx::Rect apertureRect(20, 30, 40, 45); // rightWidth: 40, botHeight: 25

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<NinePatchLayerImpl> layer = NinePatchLayerImpl::Create(hostImpl.active_tree(), 1);
    layer->draw_properties().visible_content_rect = visibleContentRect;
    layer->SetBounds(layerSize);
    layer->SetContentBounds(layerSize);
    layer->CreateRenderSurface();
    layer->draw_properties().render_target = layer.get();
    layer->SetLayout(bitmapSize, apertureRect);
    layer->SetResourceId(1);

    AppendQuadsData data;
    layer->AppendQuads(&quadCuller, &data);

    // Verify corner rects fill the layer and don't overlap
    const QuadList& quads = quadCuller.quad_list();
    EXPECT_EQ(4, quads.size());
    Region filled;
    for (size_t i = 0; i < quads.size(); ++i) {
        DrawQuad* quad = quads[i];
        gfx::Rect quadRect = quad->rect;

        EXPECT_FALSE(filled.Intersects(quadRect));
        filled.Union(quadRect);
    }
    Region expectedFull(visibleContentRect);
    EXPECT_EQ(expectedFull, filled);

    // Verify UV rects cover the corners of the bitmap and the crop is weighted
    // proportionately to the relative corner sizes (for uneven apertures).
    gfx::Rect bitmapRect(gfx::Point(), bitmapSize);
    Region texRemaining(bitmapRect);
    for (size_t i = 0; i < quads.size(); ++i) {
        DrawQuad* quad = quads[i];
        const TextureDrawQuad* texQuad = TextureDrawQuad::MaterialCast(quad);
        gfx::RectF texRect = gfx::BoundingRect(texQuad->uv_top_left, texQuad->uv_bottom_right);
        texRect.Scale(bitmapSize.width(), bitmapSize.height());
        texRemaining.Subtract(Region(ToRoundedIntRect(texRect)));
    }
    Region expectedRemainingRegion = Region(gfx::Rect(bitmapSize));
    expectedRemainingRegion.Subtract(gfx::Rect(0, 0, 17, 28));
    expectedRemainingRegion.Subtract(gfx::Rect(67, 0, 34, 28));
    expectedRemainingRegion.Subtract(gfx::Rect(0, 78, 17, 23));
    expectedRemainingRegion.Subtract(gfx::Rect(67, 78, 34, 23));
    EXPECT_EQ(expectedRemainingRegion, texRemaining);
}

}  // namespace
}  // namespace cc
