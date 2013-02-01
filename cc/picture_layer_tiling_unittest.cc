// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_tiling.h"

#include "cc/test/fake_picture_layer_tiling_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

class PictureLayerTilingIteratorTest : public testing::Test {
 public:
  PictureLayerTilingIteratorTest() {}
  virtual ~PictureLayerTilingIteratorTest() {}

  void Initialize(gfx::Size tile_size,
                  float contents_scale,
                  gfx::Size layer_bounds) {
    client_.SetTileSize(tile_size);
    tiling_ = PictureLayerTiling::Create(contents_scale, tile_size);
    tiling_->SetClient(&client_);
    tiling_->SetLayerBounds(layer_bounds);
  }

  void VerifyTilesExactlyCoverRect(float rect_scale, gfx::Rect rect) {
    // Iterators are not valid if this ratio is too large (i.e. the
    // tiling is too high-res for a low-res destination rect.)  This is an
    // artifact of snapping geometry to integer coordinates and then mapping
    // back to floating point texture coordinates.
    float dest_to_contents_scale = tiling_->contents_scale() / rect_scale;
    ASSERT_LE(dest_to_contents_scale, 2.0);

    Region remaining = rect;
    for (PictureLayerTiling::Iterator iter(tiling_.get(), rect_scale, rect);
         iter;
         ++iter) {

      // Geometry cannot overlap previous geometry at all
      gfx::Rect geometry = iter.geometry_rect();
      EXPECT_TRUE(rect.Contains(geometry));
      EXPECT_TRUE(remaining.Contains(geometry));
      remaining.Subtract(geometry);

      // Sanity check that texture coords are within the texture rect.
      gfx::RectF texture_rect = iter.texture_rect();
      EXPECT_GE(texture_rect.x(), 0);
      EXPECT_GE(texture_rect.y(), 0);
      EXPECT_LE(texture_rect.right(), client_.TileSize().width());
      EXPECT_LE(texture_rect.bottom(), client_.TileSize().height());

      EXPECT_EQ(iter.texture_size(), client_.TileSize());
    }

    // The entire rect must be filled by geometry from the tiling.
    EXPECT_TRUE(remaining.IsEmpty());
  }

 protected:
  FakePictureLayerTilingClient client_;
  scoped_ptr<PictureLayerTiling> tiling_;

  DISALLOW_COPY_AND_ASSIGN(PictureLayerTilingIteratorTest);
};

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsNoScale) {
  Initialize(gfx::Size(100, 100), 1, gfx::Size(1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(52, 83, 789, 412));

  // With borders, a size of 3x3 = 1 pixel of content.
  Initialize(gfx::Size(3, 3), 1, gfx::Size(10, 10));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1, 1));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(1, 1, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(3, 2, 5, 2));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsTilingScale) {
  Initialize(gfx::Size(200, 100), 2.0f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  Initialize(gfx::Size(3, 3), 2.0f, gfx::Size(10, 10));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1, 1));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(1, 1, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(3, 2, 5, 2));

  Initialize(gfx::Size(100, 200), 0.5f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  Initialize(gfx::Size(150, 250), 0.37f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  Initialize(gfx::Size(312, 123), 0.01f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsBothScale) {
  Initialize(gfx::Size(50, 50), 4.0f, gfx::Size(800, 600));
  VerifyTilesExactlyCoverRect(2.0f, gfx::Rect());
  VerifyTilesExactlyCoverRect(2.0f, gfx::Rect(0, 0, 1600, 1200));
  VerifyTilesExactlyCoverRect(2.0f, gfx::Rect(512, 365, 253, 182));

  float scale = 6.7f;
  gfx::Size bounds(800, 600);
  gfx::Rect full_rect(gfx::ToCeiledSize(gfx::ScaleSize(bounds, scale)));
  Initialize(gfx::Size(256, 512), 5.2f, bounds);
  VerifyTilesExactlyCoverRect(scale, full_rect);
  VerifyTilesExactlyCoverRect(scale, gfx::Rect(2014, 1579, 867, 1033));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorEmptyRect) {
  Initialize(gfx::Size(100, 100), 1, gfx::Size(800, 600));

  gfx::Rect empty;
  PictureLayerTiling::Iterator iter(tiling_.get(), 1, empty);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, NonIntersectingRect) {
  Initialize(gfx::Size(100, 100), 1, gfx::Size(800, 600));
  gfx::Rect non_intersecting(1000, 1000, 50, 50);
  PictureLayerTiling::Iterator iter(tiling_.get(), 1, non_intersecting);
  EXPECT_FALSE(iter);
}

}  // namespace
}  // namespace cc
