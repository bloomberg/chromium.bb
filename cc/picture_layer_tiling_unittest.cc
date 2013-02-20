// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_tiling.h"

#include "cc/test/fake_picture_layer_tiling_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
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
    tiling_ = PictureLayerTiling::Create(contents_scale);
    tiling_->SetClient(&client_);
    tiling_->SetLayerBounds(layer_bounds);
  }

  void VerifyTilesExactlyCoverRect(
      float rect_scale,
      gfx::Rect request_rect,
      gfx::Rect expect_rect) {
    EXPECT_TRUE(request_rect.Contains(expect_rect));

    // Iterators are not valid if this ratio is too large (i.e. the
    // tiling is too high-res for a low-res destination rect.)  This is an
    // artifact of snapping geometry to integer coordinates and then mapping
    // back to floating point texture coordinates.
    float dest_to_contents_scale = tiling_->contents_scale() / rect_scale;
    ASSERT_LE(dest_to_contents_scale, 2.0);

    Region remaining = expect_rect;
    for (PictureLayerTiling::Iterator iter(tiling_.get(),
                                           rect_scale,
                                           request_rect,
                                           PictureLayerTiling::LayerDeviceAlignmentUnknown);
         iter;
         ++iter) {

      // Geometry cannot overlap previous geometry at all
      gfx::Rect geometry = iter.geometry_rect();
      EXPECT_TRUE(expect_rect.Contains(geometry));
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

  void VerifyTilesExactlyCoverRect(float rect_scale, gfx::Rect rect) {
    VerifyTilesExactlyCoverRect(rect_scale, rect, rect);
  }

  void VerifyTilesCoverNonContainedRect(float rect_scale, gfx::Rect dest_rect) {
    float dest_to_contents_scale = tiling_->contents_scale() / rect_scale;
    gfx::Rect clamped_rect(gfx::ToEnclosingRect(gfx::ScaleRect(
        tiling_->ContentRect(), 1 / dest_to_contents_scale)));
    clamped_rect.Intersect(dest_rect);
    VerifyTilesExactlyCoverRect(rect_scale, dest_rect, clamped_rect);
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
  Initialize(gfx::Size(100, 100), 1.0f, gfx::Size(800, 600));

  gfx::Rect empty;
  PictureLayerTiling::Iterator iter(tiling_.get(), 1.0f, empty,
                                    PictureLayerTiling::LayerDeviceAlignmentUnknown);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, NonIntersectingRect) {
  Initialize(gfx::Size(100, 100), 1.0f, gfx::Size(800, 600));
  gfx::Rect non_intersecting(1000, 1000, 50, 50);
  PictureLayerTiling::Iterator iter(tiling_.get(), 1, non_intersecting,
                                    PictureLayerTiling::LayerDeviceAlignmentUnknown);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, LayerEdgeTextureCoordinates) {
  Initialize(gfx::Size(300, 300), 1.0f, gfx::Size(256, 256));
  // All of these sizes are 256x256, scaled and ceiled.
  VerifyTilesExactlyCoverRect(1.0f, gfx::Rect(gfx::Size(256, 256)));
  VerifyTilesExactlyCoverRect(0.8f, gfx::Rect(gfx::Size(205, 205)));
  VerifyTilesExactlyCoverRect(1.2f, gfx::Rect(gfx::Size(308, 308)));
}

TEST_F(PictureLayerTilingIteratorTest, NonContainedDestRect) {
  Initialize(gfx::Size(100, 100), 1.0f, gfx::Size(400, 400));

  // Too large in all dimensions
  VerifyTilesCoverNonContainedRect(1.0f, gfx::Rect(-1000, -1000, 2000, 2000));
  VerifyTilesCoverNonContainedRect(1.5f, gfx::Rect(-1000, -1000, 2000, 2000));
  VerifyTilesCoverNonContainedRect(0.5f, gfx::Rect(-1000, -1000, 2000, 2000));

  // Partially covering content, but too large
  VerifyTilesCoverNonContainedRect(1.0f, gfx::Rect(-1000, 100, 2000, 100));
  VerifyTilesCoverNonContainedRect(1.5f, gfx::Rect(-1000, 100, 2000, 100));
  VerifyTilesCoverNonContainedRect(0.5f, gfx::Rect(-1000, 100, 2000, 100));
}

TEST(PictureLayerTilingTest, ExpandRectEqual) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 10000);
  int64 target_area = 100 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(in.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectSmaller) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 10000);
  int64 target_area = 100 * 100;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.width() - in.width(), out.height() - in.height());
  EXPECT_NEAR(100 * 100, out.width() * out.height(), 50);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectUnbounded) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.width() - in.width(), out.height() - in.height());
  EXPECT_NEAR(200 * 200, out.width() * out.height(), 100);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedSmaller) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(50, 60, 40, 30);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedEqual) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds = in;
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedSmallerStretchVertical) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(45, 0, 90, 300);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedEqualStretchVertical) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(40, 0, 100, 300);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedSmallerStretchHorizontal) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(0, 55, 180, 190);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedEqualStretchHorizontal) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(0, 50, 180, 200);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedLeft) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(20, -1000, 10000, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.bottom() - in.bottom(), out.right() - in.right());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() - out.height() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedRight) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 1000+120, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.bottom() - in.bottom(), in.x() - out.x());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() - out.height() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedTop) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, 30, 10000, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.right() - in.right(), out.bottom() - in.bottom());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() * 2 - out.height());
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedBottom) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 1000 + 220);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.right() - in.right(), in.y() - out.y());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() * 2 - out.height());
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectSquishedHorizontally) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(0, -4000, 100+40+20, 100000);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(20, out.right() - in.right());
  EXPECT_EQ(40, in.x() - out.x());
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectSquishedVertically) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-4000, 0, 100000, 200+50+30);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(30, out.bottom() - in.bottom());
  EXPECT_EQ(50, in.y() - out.y());
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.height() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

}  // namespace
}  // namespace cc
