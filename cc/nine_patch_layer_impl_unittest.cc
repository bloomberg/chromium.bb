// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/nine_patch_layer_impl.h"

#include "cc/append_quads_data.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/texture_draw_quad.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebTransformationMatrix.h>

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

    scoped_ptr<NinePatchLayerImpl> layer = NinePatchLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());
    layer->setLayout(bitmapSize, apertureRect);
    layer->setResourceId(1);

    // This scale should not affect the generated quad geometry, but only
    // the shared draw transform.
    WebKit::WebTransformationMatrix transform;
    transform.scale(10);
    layer->setDrawTransform(transform);

    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);

    // Verify quad rects
    const QuadList& quads = quadCuller.quadList();
    EXPECT_EQ(quads.size(), 8);
    Region remaining(visibleContentRect);
    for (size_t i = 0; i < quads.size(); ++i) {
        DrawQuad* quad = quads[i];
        gfx::Rect quadRect = quad->rect;

        EXPECT_TRUE(visibleContentRect.Contains(quadRect)) << i;
        EXPECT_TRUE(remaining.Contains(quadRect)) << i;
        EXPECT_EQ(quad->quadTransform(), transform) << i;
        remaining.Subtract(Region(quadRect));
    }
    EXPECT_RECT_EQ(remaining.bounds(), scaledApertureNonUniform);
    Region scaledApertureRegion(scaledApertureNonUniform);
    EXPECT_EQ(remaining, scaledApertureRegion);

    // Verify UV rects
    gfx::Rect bitmapRect(gfx::Point(), bitmapSize);
    Region texRemaining(bitmapRect);
    for (size_t i = 0; i < quads.size(); ++i) {
        DrawQuad* quad = quads[i];
        const TextureDrawQuad* texQuad = TextureDrawQuad::MaterialCast(quad);
        gfx::RectF texRect = texQuad->uv_rect;
        texRect.Scale(bitmapSize.width(), bitmapSize.height());
        texRemaining.Subtract(Region(ToRoundedIntRect(texRect)));
    }
    EXPECT_RECT_EQ(texRemaining.bounds(), apertureRect);
    Region apertureRegion(apertureRect);
    EXPECT_EQ(texRemaining, apertureRegion);
}

}  // namespace
}  // namespace cc
