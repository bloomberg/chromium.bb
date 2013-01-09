// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_tiling_set.h"

#include "cc/test/fake_picture_layer_tiling_client.h"
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

}  // namespace
}  // namespace cc
