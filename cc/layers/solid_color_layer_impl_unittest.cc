// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <vector>

#include "cc/layers/append_quads_data.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SolidColorLayerImplTest, VerifyTilingCompleteAndNoOverlap) {
  MockQuadCuller quad_culler;
  gfx::Size layer_size = gfx::Size(800, 600);
  gfx::Rect visible_content_rect = gfx::Rect(gfx::Point(), layer_size);

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  LayerTestCommon::VerifyQuadsExactlyCoverRect(quad_culler.quad_list(),
                                               visible_content_rect);
}

TEST(SolidColorLayerImplTest, VerifyCorrectBackgroundColorInQuad) {
  SkColor test_color = 0xFFA55AFF;

  MockQuadCuller quad_culler;
  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_content_rect = gfx::Rect(gfx::Point(), layer_size);

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->SetBackgroundColor(test_color);
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  ASSERT_EQ(quad_culler.quad_list().size(), 1U);
  EXPECT_EQ(SolidColorDrawQuad::MaterialCast(quad_culler.quad_list()[0])->color,
            test_color);
}

TEST(SolidColorLayerImplTest, VerifyCorrectOpacityInQuad) {
  const float opacity = 0.5f;

  MockQuadCuller quad_culler;
  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_content_rect = gfx::Rect(gfx::Point(), layer_size);

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->draw_properties().opacity = opacity;
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  ASSERT_EQ(quad_culler.quad_list().size(), 1U);
  EXPECT_EQ(opacity,
            SolidColorDrawQuad::MaterialCast(quad_culler.quad_list()[0])
                ->opacity());
}

TEST(SolidColorLayerImplTest, VerifyOpaqueRect) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);

  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_content_rect = gfx::Rect(gfx::Point(), layer_size);

  scoped_refptr<SolidColorLayer> layer = SolidColorLayer::Create();
  layer->SetBounds(layer_size);
  layer->SetForceRenderSurface(true);

  scoped_refptr<Layer> root = Layer::Create();
  root->AddChild(layer);

  std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
  LayerTreeHostCommon::calculateDrawProperties(
      root, gfx::Size(500, 500), 1, 1, 1024, false, renderSurfaceLayerList);

  EXPECT_FALSE(layer->contents_opaque());
  layer->SetBackgroundColor(SkColorSetARGBInline(255, 10, 20, 30));
  EXPECT_TRUE(layer->contents_opaque());
  {
    scoped_ptr<SolidColorLayerImpl> layer_impl =
        SolidColorLayerImpl::Create(host_impl.active_tree(), layer->id());
    layer->PushPropertiesTo(layer_impl.get());

    // The impl layer should call itself opaque as well.
    EXPECT_TRUE(layer_impl->contents_opaque());

    // Impl layer has 1 opacity, and the color is opaque, so the opaqueRect
    // should be the full tile.
    layer_impl->draw_properties().opacity = 1;

    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer_impl->AppendQuads(&quad_culler, &data);

    ASSERT_EQ(quad_culler.quad_list().size(), 1U);
    EXPECT_EQ(visible_content_rect.ToString(),
              quad_culler.quad_list()[0]->opaque_rect.ToString());
  }

  EXPECT_TRUE(layer->contents_opaque());
  layer->SetBackgroundColor(SkColorSetARGBInline(254, 10, 20, 30));
  EXPECT_FALSE(layer->contents_opaque());
  {
    scoped_ptr<SolidColorLayerImpl> layer_impl =
        SolidColorLayerImpl::Create(host_impl.active_tree(), layer->id());
    layer->PushPropertiesTo(layer_impl.get());

    // The impl layer should callnot itself opaque anymore.
    EXPECT_FALSE(layer_impl->contents_opaque());

    // Impl layer has 1 opacity, but the color is not opaque, so the opaque_rect
    // should be empty.
    layer_impl->draw_properties().opacity = 1;

    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer_impl->AppendQuads(&quad_culler, &data);

    ASSERT_EQ(quad_culler.quad_list().size(), 1U);
    EXPECT_EQ(gfx::Rect().ToString(),
              quad_culler.quad_list()[0]->opaque_rect.ToString());
  }
}

}  // namespace
}  // namespace cc
