// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling_set.h"

#include <vector>

#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
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
  client.SetTileSize(gfx::Size(256, 256));

  gfx::Size layer_bounds(1000, 800);
  set.SetLayerBounds(layer_bounds);

  set.AddTiling(1.0);
  set.AddTiling(1.5);
  set.AddTiling(2.0);

  float contents_scale = 2.0;
  gfx::Size content_bounds(
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale)));
  gfx::Rect content_rect(content_bounds);

  Region remaining(content_rect);
  PictureLayerTilingSet::CoverageIterator iter(
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
  void runTest(
      int num_tilings,
      float min_scale,
      float scale_increment,
      float ideal_contents_scale,
      float expected_scale) {
    scoped_ptr<FakeOutputSurface> output_surface =
        FakeOutputSurface::Create3d();
    scoped_ptr<ResourceProvider> resource_provider =
        ResourceProvider::Create(output_surface.get());

    FakePictureLayerTilingClient client;
    client.SetTileSize(gfx::Size(256, 256));
    PictureLayerTilingSet set(&client);

    gfx::Size layer_bounds(1000, 800);
    set.SetLayerBounds(layer_bounds);

    float scale = min_scale;
    for (int i = 0; i < num_tilings; ++i, scale += scale_increment) {
      PictureLayerTiling* tiling = set.AddTiling(scale);
      std::vector<Tile*> tiles = tiling->AllTilesForTesting();
      for (size_t i = 0; i < tiles.size(); ++i) {
        EXPECT_FALSE(tiles[i]->drawing_info().GetResourceForTesting());

        tiles[i]->drawing_info().GetResourceForTesting() =
            make_scoped_ptr(new ResourcePool::Resource(
                resource_provider.get(),
                gfx::Size(1, 1),
                resource_provider->best_texture_format()));
      }
    }

    float max_contents_scale = scale;
    gfx::Size content_bounds(
        gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, max_contents_scale)));
    gfx::Rect content_rect(content_bounds);

    Region remaining(content_rect);
    PictureLayerTilingSet::CoverageIterator iter(
        &set,
        max_contents_scale,
        content_rect,
        ideal_contents_scale);
    for (; iter; ++iter) {
      gfx::Rect geometry_rect = iter.geometry_rect();
      EXPECT_TRUE(content_rect.Contains(geometry_rect));
      ASSERT_TRUE(remaining.Contains(geometry_rect));
      remaining.Subtract(geometry_rect);

      float scale = iter.CurrentTiling()->contents_scale();
      EXPECT_EQ(expected_scale, scale);

      if (num_tilings)
        EXPECT_TRUE(*iter);
      else
        EXPECT_FALSE(*iter);
    }
    EXPECT_TRUE(remaining.IsEmpty());
  }
};

TEST_F(PictureLayerTilingSetTestWithResources, NoTilings) {
  runTest(0, 0.f, 0.f, 2.f, 0.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, OneTiling_Smaller) {
  runTest(1, 1.f, 0.f, 2.f, 1.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, OneTiling_Larger) {
  runTest(1, 3.f, 0.f, 2.f, 3.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_Smaller) {
  runTest(2, 1.f, 1.f, 3.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_SmallerEqual) {
  runTest(2, 1.f, 1.f, 2.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_LargerEqual) {
  runTest(2, 1.f, 1.f, 1.f, 1.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_Larger) {
  runTest(2, 2.f, 8.f, 1.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, ManyTilings_Equal) {
  runTest(10, 1.f, 1.f, 5.f, 5.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, ManyTilings_NotEqual) {
  runTest(10, 1.f, 1.f, 4.5f, 5.f);
}

}  // namespace
}  // namespace cc
