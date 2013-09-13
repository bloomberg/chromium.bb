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
  return gfx::Rect(gfx::ToRoundedInt(rect_f.x()),
                   gfx::ToRoundedInt(rect_f.y()),
                   gfx::ToRoundedInt(rect_f.width()),
                   gfx::ToRoundedInt(rect_f.height()));
}

TEST(NinePatchLayerImplTest, VerifyDrawQuads) {
  // Input is a 100x100 bitmap with a 40x50 aperture at x=20, y=30.
  // The bounds of the layer are set to 400x400, so the draw quads
  // generated should leave the border width (40) intact.
  MockQuadCuller quad_culler;
  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(400, 400);
  gfx::Rect visible_content_rect(layer_size);
  gfx::Rect aperture_rect(20, 30, 40, 50);
  gfx::Rect scaled_aperture_non_uniform(20, 30, 340, 350);

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<NinePatchLayerImpl> layer =
      NinePatchLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();
  layer->SetLayout(bitmap_size, aperture_rect);
  layer->SetResourceId(1);

  // This scale should not affect the generated quad geometry, but only
  // the shared draw transform.
  gfx::Transform transform;
  transform.Scale(10, 10);
  layer->draw_properties().target_space_transform = transform;

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  // Verify quad rects
  const QuadList& quads = quad_culler.quad_list();
  EXPECT_EQ(8u, quads.size());
  Region remaining(visible_content_rect);
  for (size_t i = 0; i < quads.size(); ++i) {
    DrawQuad* quad = quads[i];
    gfx::Rect quad_rect = quad->rect;

    EXPECT_TRUE(visible_content_rect.Contains(quad_rect)) << i;
    EXPECT_TRUE(remaining.Contains(quad_rect)) << i;
    EXPECT_EQ(transform, quad->quadTransform());
    remaining.Subtract(Region(quad_rect));
  }
  EXPECT_RECT_EQ(scaled_aperture_non_uniform, remaining.bounds());
  Region scaled_aperture_region(scaled_aperture_non_uniform);
  EXPECT_EQ(scaled_aperture_region, remaining);

  // Verify UV rects
  gfx::Rect bitmap_rect(bitmap_size);
  Region tex_remaining(bitmap_rect);
  for (size_t i = 0; i < quads.size(); ++i) {
    DrawQuad* quad = quads[i];
    const TextureDrawQuad* tex_quad = TextureDrawQuad::MaterialCast(quad);
    gfx::RectF tex_rect =
        gfx::BoundingRect(tex_quad->uv_top_left, tex_quad->uv_bottom_right);
    tex_rect.Scale(bitmap_size.width(), bitmap_size.height());
    tex_remaining.Subtract(Region(ToRoundedIntRect(tex_rect)));
  }
  EXPECT_RECT_EQ(aperture_rect, tex_remaining.bounds());
  Region aperture_region(aperture_rect);
  EXPECT_EQ(aperture_region, tex_remaining);
}

TEST(NinePatchLayerImplTest, VerifyDrawQuadsForSqueezedLayer) {
  // Test with a layer much smaller than the bitmap.
  MockQuadCuller quad_culler;
  gfx::Size bitmap_size(101, 101);
  gfx::Size layer_size(51, 51);
  gfx::Rect visible_content_rect(layer_size);
  gfx::Rect aperture_rect(20, 30, 40, 45);  // rightWidth: 40, botHeight: 25

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<NinePatchLayerImpl> layer =
      NinePatchLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();
  layer->SetLayout(bitmap_size, aperture_rect);
  layer->SetResourceId(1);

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  // Verify corner rects fill the layer and don't overlap
  const QuadList& quads = quad_culler.quad_list();
  EXPECT_EQ(4u, quads.size());
  Region filled;
  for (size_t i = 0; i < quads.size(); ++i) {
    DrawQuad* quad = quads[i];
    gfx::Rect quad_rect = quad->rect;

    EXPECT_FALSE(filled.Intersects(quad_rect));
    filled.Union(quad_rect);
  }
  Region expected_full(visible_content_rect);
  EXPECT_EQ(expected_full, filled);

  // Verify UV rects cover the corners of the bitmap and the crop is weighted
  // proportionately to the relative corner sizes (for uneven apertures).
  gfx::Rect bitmap_rect(bitmap_size);
  Region tex_remaining(bitmap_rect);
  for (size_t i = 0; i < quads.size(); ++i) {
    DrawQuad* quad = quads[i];
    const TextureDrawQuad* tex_quad = TextureDrawQuad::MaterialCast(quad);
    gfx::RectF tex_rect =
        gfx::BoundingRect(tex_quad->uv_top_left, tex_quad->uv_bottom_right);
    tex_rect.Scale(bitmap_size.width(), bitmap_size.height());
    tex_remaining.Subtract(Region(ToRoundedIntRect(tex_rect)));
  }
  Region expected_remaining_region = Region(gfx::Rect(bitmap_size));
  expected_remaining_region.Subtract(gfx::Rect(0, 0, 17, 28));
  expected_remaining_region.Subtract(gfx::Rect(67, 0, 34, 28));
  expected_remaining_region.Subtract(gfx::Rect(0, 78, 17, 23));
  expected_remaining_region.Subtract(gfx::Rect(67, 78, 34, 23));
  EXPECT_EQ(expected_remaining_region, tex_remaining);
}

}  // namespace
}  // namespace cc
