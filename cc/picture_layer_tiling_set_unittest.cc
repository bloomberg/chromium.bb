// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_tiling_set.h"

#include "cc/resource_pool.h"
#include "cc/resource_provider.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/fake_tile_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

TEST(PictureLayerTilingSetTest, NoResources) {
  FakePictureLayerTilingClient client;
  PictureLayerTilingSet set(&client);
  gfx::Size default_tile_size(256, 256);

  gfx::Size layer_bounds(1000, 800);
  set.SetLayerBounds(layer_bounds);

  set.AddTiling(1.0, default_tile_size);
  set.AddTiling(1.5, default_tile_size);
  set.AddTiling(2.0, default_tile_size);

  float contents_scale = 2.0;
  gfx::Size content_bounds(
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale)));
  gfx::Rect content_rect(gfx::Point(), content_bounds);

  Region remaining(content_rect);
  PictureLayerTilingSet::Iterator iter(
      &set,
      contents_scale,
      content_rect,
      contents_scale);
  for (; iter; ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    EXPECT_TRUE(content_rect.Contains(geometry_rect));
    ASSERT_TRUE(remaining.Contains(geometry_rect));
    remaining.Subtract(geometry_rect);

    // No tiles have resources, so no iter represents a real tile.
    EXPECT_FALSE(*iter);
  }
  EXPECT_TRUE(remaining.IsEmpty());
}

class PictureLayerTilingSetTestWithResources : public testing::Test {
 public:
  void runTest(int num_tilings, float min_scale, float scale_increment) {
    scoped_ptr<FakeOutputSurface> output_surface =
        FakeOutputSurface::Create3d();
    scoped_ptr<ResourceProvider> resource_provider =
        ResourceProvider::create(output_surface.get());

    FakePictureLayerTilingClient client;
    PictureLayerTilingSet set(&client);
    gfx::Size default_tile_size(256, 256);

    gfx::Size layer_bounds(1000, 800);
    set.SetLayerBounds(layer_bounds);

    float contents_scale = 2.f;
    gfx::Size content_bounds(
        gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale)));
    gfx::Rect content_rect(gfx::Point(), content_bounds);

    float scale = min_scale;
    for (int i = 0; i < num_tilings; ++i, scale += scale_increment) {
      PictureLayerTiling* tiling = set.AddTiling(scale, default_tile_size);
      std::vector<Tile*> tiles = tiling->AllTilesForTesting();
      for (size_t i = 0; i < tiles.size(); ++i) {
        EXPECT_FALSE(tiles[i]->ManagedStateForTesting().resource);

        tiles[i]->ManagedStateForTesting().resource =
            make_scoped_ptr(new ResourcePool::Resource(
                resource_provider.get(),
                gfx::Size(1, 1),
                resource_provider->bestTextureFormat()));
      }
    }

    Region remaining(content_rect);
    PictureLayerTilingSet::Iterator iter(
        &set,
        contents_scale,
        content_rect,
        contents_scale);
    for (; iter; ++iter) {
      gfx::Rect geometry_rect = iter.geometry_rect();
      EXPECT_TRUE(content_rect.Contains(geometry_rect));
      ASSERT_TRUE(remaining.Contains(geometry_rect));
      remaining.Subtract(geometry_rect);

      if (num_tilings)
        EXPECT_TRUE(*iter);
      else
        EXPECT_FALSE(*iter);
    }
    EXPECT_TRUE(remaining.IsEmpty());
  }
};

TEST_F(PictureLayerTilingSetTestWithResources, NoTilings) {
  runTest(0, 0.f, 0.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, OneTiling) {
  runTest(1, 1.f, 0.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings) {
  runTest(2, 1.f, 1.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, ManyTilings) {
  runTest(10, 1.f, 1.f);
}

}  // namespace
}  // namespace cc
