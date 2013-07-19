// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling.h"

#include <limits>

#include "cc/base/math_util.h"
#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

static gfx::Rect ViewportInLayerSpace(
    const gfx::Transform& transform,
    gfx::Size device_viewport) {

  gfx::Transform inverse;
  if (!transform.GetInverse(&inverse))
    return gfx::Rect();

  gfx::RectF viewport_in_layer_space = MathUtil::ProjectClippedRect(
      inverse, gfx::RectF(gfx::Point(0, 0), device_viewport));

  return ToEnclosingRect(viewport_in_layer_space);
}

class TestablePictureLayerTiling : public PictureLayerTiling {
 public:
  using PictureLayerTiling::SetLiveTilesRect;
  using PictureLayerTiling::TileAt;

  static scoped_ptr<TestablePictureLayerTiling> Create(
      float contents_scale,
      gfx::Size layer_bounds,
      PictureLayerTilingClient* client) {
    return make_scoped_ptr(new TestablePictureLayerTiling(
        contents_scale,
        layer_bounds,
        client));
  }

 protected:
  TestablePictureLayerTiling(float contents_scale,
                             gfx::Size layer_bounds,
                             PictureLayerTilingClient* client)
      : PictureLayerTiling(contents_scale, layer_bounds, client) { }
};

class PictureLayerTilingIteratorTest : public testing::Test {
 public:
  PictureLayerTilingIteratorTest() {}
  virtual ~PictureLayerTilingIteratorTest() {}

  void Initialize(gfx::Size tile_size,
                  float contents_scale,
                  gfx::Size layer_bounds) {
    client_.SetTileSize(tile_size);
    tiling_ = TestablePictureLayerTiling::Create(contents_scale,
                                                 layer_bounds,
                                                 &client_);
  }

  void SetLiveRectAndVerifyTiles(gfx::Rect live_tiles_rect) {
    tiling_->SetLiveTilesRect(live_tiles_rect);

    std::vector<Tile*> tiles = tiling_->AllTilesForTesting();
    for (std::vector<Tile*>::iterator iter = tiles.begin();
         iter != tiles.end();
         ++iter) {
      EXPECT_TRUE(live_tiles_rect.Intersects((*iter)->content_rect()));
    }
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
    for (PictureLayerTiling::CoverageIterator
             iter(tiling_.get(), rect_scale, request_rect);
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

  void VerifyTiles(
      float rect_scale,
      gfx::Rect rect,
      base::Callback<void(Tile* tile, gfx::Rect geometry_rect)> callback) {
    VerifyTiles(tiling_.get(),
                rect_scale,
                rect,
                callback);
  }

  void VerifyTiles(
      PictureLayerTiling* tiling,
      float rect_scale,
      gfx::Rect rect,
      base::Callback<void(Tile* tile, gfx::Rect geometry_rect)> callback) {
    Region remaining = rect;
    for (PictureLayerTiling::CoverageIterator iter(tiling, rect_scale, rect);
         iter;
         ++iter) {
      remaining.Subtract(iter.geometry_rect());
      callback.Run(*iter, iter.geometry_rect());
    }
    EXPECT_TRUE(remaining.IsEmpty());
  }

  void VerifyTilesCoverNonContainedRect(float rect_scale, gfx::Rect dest_rect) {
    float dest_to_contents_scale = tiling_->contents_scale() / rect_scale;
    gfx::Rect clamped_rect = gfx::ScaleToEnclosingRect(
        tiling_->ContentRect(), 1.f / dest_to_contents_scale);
    clamped_rect.Intersect(dest_rect);
    VerifyTilesExactlyCoverRect(rect_scale, dest_rect, clamped_rect);
  }

 protected:
  FakePictureLayerTilingClient client_;
  scoped_ptr<TestablePictureLayerTiling> tiling_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureLayerTilingIteratorTest);
};

TEST_F(PictureLayerTilingIteratorTest, LiveTilesExactlyCoverLiveTileRect) {
  Initialize(gfx::Size(100, 100), 1, gfx::Size(1099, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(100, 100));
  SetLiveRectAndVerifyTiles(gfx::Rect(101, 99));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(1, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(201, 800));
}

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
  PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1.0f, empty);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, NonIntersectingRect) {
  Initialize(gfx::Size(100, 100), 1.0f, gfx::Size(800, 600));
  gfx::Rect non_intersecting(1000, 1000, 50, 50);
  PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1, non_intersecting);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, LayerEdgeTextureCoordinates) {
  Initialize(gfx::Size(300, 300), 1.0f, gfx::Size(256, 256));
  // All of these sizes are 256x256, scaled and ceiled.
  VerifyTilesExactlyCoverRect(1.0f, gfx::Rect(0, 0, 256, 256));
  VerifyTilesExactlyCoverRect(0.8f, gfx::Rect(0, 0, 205, 205));
  VerifyTilesExactlyCoverRect(1.2f, gfx::Rect(0, 0, 308, 308));
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

TEST(PictureLayerTilingTest, ExpandRectOutOfBoundsFarAway) {
  gfx::Rect in(400, 500, 100, 200);
  gfx::Rect bounds(0, 0, 10, 10);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_TRUE(out.IsEmpty());
}

TEST(PictureLayerTilingTest, ExpandRectOutOfBoundsExpandedFullyCover) {
  gfx::Rect in(40, 50, 100, 100);
  gfx::Rect bounds(0, 0, 10, 10);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectOutOfBoundsExpandedPartlyCover) {
  gfx::Rect in(600, 600, 100, 100);
  gfx::Rect bounds(0, 0, 500, 500);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_EQ(bounds.right(), out.right());
  EXPECT_EQ(bounds.bottom(), out.bottom());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() - out.height());
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, EmptyStartingRect) {
  // If a layer has a non-invertible transform, then the starting rect
  // for the layer would be empty.
  gfx::Rect in(40, 40, 0, 0);
  gfx::Rect bounds(0, 0, 10, 10);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds);
  EXPECT_TRUE(out.IsEmpty());
}

static void TileExists(bool exists, Tile* tile, gfx::Rect geometry_rect) {
  EXPECT_EQ(exists, tile != NULL) << geometry_rect.ToString();
}

TEST_F(PictureLayerTilingIteratorTest, TilesExist) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      layer_bounds,  // device viewport
      gfx::Rect(layer_bounds),  // viewport in layer space
      gfx::Rect(layer_bounds),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      1.0,  // current frame time
      10000);  // max tiles in tile manager
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));

  // Make the viewport rect empty. All tiles are killed and become zombies.
  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      layer_bounds,  // device viewport
      gfx::Rect(),  // viewport in layer space
      gfx::Rect(),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      2.0,  // current frame time
      10000);  // max tiles in tile manager
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));
}

TEST_F(PictureLayerTilingIteratorTest, TilesExistGiantViewport) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  gfx::Rect giant_rect(-10000000, -10000000, 1000000000, 1000000000);

  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      layer_bounds,  // device viewport
      giant_rect,  // viewport in layer space
      gfx::Rect(layer_bounds),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      1.0,  // current frame time
      10000);  // max tiles in tile manager
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));

  // If the visible content rect is empty, it should still have live tiles.
  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      layer_bounds,  // device viewport
      giant_rect,  // viewport in layer space
      gfx::Rect(),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      2.0,  // current frame time
      10000);  // max tiles in tile manager
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));
}

TEST_F(PictureLayerTilingIteratorTest, TilesExistOutsideViewport) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  // This rect does not intersect with the layer, as the layer is outside the
  // viewport.
  gfx::Rect viewport_rect(1100, 0, 1000, 1000);
  EXPECT_FALSE(viewport_rect.Intersects(gfx::Rect(layer_bounds)));

  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      layer_bounds,  // device viewport
      viewport_rect,  // viewport in layer space
      gfx::Rect(),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      1.0,  // current frame time
      10000);  // max tiles in tile manager
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));
}

static void TilesIntersectingRectExist(gfx::Rect rect,
                                       bool intersect_exists,
                                       Tile* tile,
                                       gfx::Rect geometry_rect) {
  bool intersects = rect.Intersects(geometry_rect);
  bool expected_exists = intersect_exists ? intersects : !intersects;
  EXPECT_EQ(expected_exists, tile != NULL)
      << "Rects intersecting " << rect.ToString() << " should exist. "
      << "Current tile rect is " << geometry_rect.ToString();
}

TEST_F(PictureLayerTilingIteratorTest,
       TilesExistLargeViewportAndLayerWithSmallVisibleArea) {
  gfx::Size layer_bounds(10000, 10000);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  gfx::Rect visible_rect(8000, 8000, 50, 50);

  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      layer_bounds,  // device viewport
      gfx::Rect(layer_bounds),  // viewport in layer space
      visible_rect,  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      1.0,  // current frame time
      1);  // max tiles in tile manager
  VerifyTiles(1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TilesIntersectingRectExist, visible_rect, true));
}

static void CountExistingTiles(int *count,
                               Tile* tile,
                               gfx::Rect geometry_rect) {
  if (tile != NULL)
    ++(*count);
}

TEST_F(PictureLayerTilingIteratorTest,
       TilesExistLargeViewportAndLayerWithLargeVisibleArea) {
  gfx::Size layer_bounds(10000, 10000);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      layer_bounds,  // device viewport
      gfx::Rect(layer_bounds),  // viewport in layer space
      gfx::Rect(layer_bounds),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      1.0,  // current frame time
      1);  // max tiles in tile manager

  int num_tiles = 0;
  VerifyTiles(1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&CountExistingTiles, &num_tiles));
  // If we're making a rect the size of one tile, it can only overlap up to 4
  // tiles depending on its position.
  EXPECT_LE(num_tiles, 4);
  VerifyTiles(1.f, gfx::Rect(), base::Bind(&TileExists, false));
}

TEST_F(PictureLayerTilingIteratorTest, AddTilingsToMatchScale) {
  gfx::Size layer_bounds(1099, 801);
  gfx::Size tile_size(100, 100);

  client_.SetTileSize(tile_size);

  PictureLayerTilingSet active_set(&client_, layer_bounds);

  active_set.AddTiling(1.f);

  VerifyTiles(active_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, false));

  active_set.UpdateTilePriorities(
      PENDING_TREE,
      layer_bounds,  // device viewport
      gfx::Rect(layer_bounds),  // viewport in layer space
      gfx::Rect(layer_bounds),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      1.0,  // current frame time
      10000);  // max tiles in tile manager

  // The active tiling has tiles now.
  VerifyTiles(active_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));

  // Add the same tilings to the pending set.
  PictureLayerTilingSet pending_set(&client_, layer_bounds);
  Region invalidation;
  pending_set.SyncTilings(active_set, layer_bounds, invalidation, 0.f);

  // The pending tiling starts with no tiles.
  VerifyTiles(pending_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, false));

  // UpdateTilePriorities on the pending tiling at the same frame time. The
  // pending tiling should get tiles.
  pending_set.UpdateTilePriorities(
      PENDING_TREE,
      layer_bounds,  // device viewport
      gfx::Rect(layer_bounds),  // viewport in layer space
      gfx::Rect(layer_bounds),  // visible content rect
      layer_bounds,  // last layer bounds
      layer_bounds,  // current layer bounds
      1.f,  // last contents scale
      1.f,  // current contents scale
      gfx::Transform(),  // last screen transform
      gfx::Transform(),  // current screen transform
      1.0,  // current frame time
      10000);  // max tiles in tile manager

  VerifyTiles(pending_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));
}

TEST(UpdateTilePrioritiesTest, VisibleTiles) {
  // The TilePriority of visible tiles should have zero distance_to_visible
  // and time_to_visible.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 200, 200);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;
  size_t max_tiles_for_interest_area = 10000;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);
}

TEST(UpdateTilePrioritiesTest, OffscreenTiles) {
  // The TilePriority of offscreen tiles (without movement) should have nonzero
  // distance_to_visible and infinite time_to_visible.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);  // offscreen; nothing is visible.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;
  size_t max_tiles_for_interest_area = 10000;

  current_screen_transform.Translate(850, 0);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  // Furthermore, in this scenario tiles on the right hand side should have a
  // larger distance to visible.
  TilePriority left = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  TilePriority right = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(right.distance_to_visible_in_pixels,
            left.distance_to_visible_in_pixels);

  left = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  right = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(right.distance_to_visible_in_pixels,
            left.distance_to_visible_in_pixels);
}

TEST(UpdateTilePrioritiesTest, PartiallyOffscreenLayer) {
  // Sanity check that a layer with some tiles visible and others offscreen has
  // correct TilePriorities for each tile.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 100, 100);  // only top quarter.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;
  size_t max_tiles_for_interest_area = 10000;

  current_screen_transform.Translate(705, 505);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);
}

TEST(UpdateTilePrioritiesTest, PartiallyOffscreenRotatedLayer) {
  // Each tile of a layer may be affected differently by a transform; Check
  // that UpdateTilePriorities correctly accounts for the transform between
  // layer space and screen space.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 100, 100);  // only top-left quarter.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;
  size_t max_tiles_for_interest_area = 10000;

  // A diagonally rotated layer that is partially off the bottom of the screen.
  // In this configuration, only the top-left tile would be visible.
  current_screen_transform.Translate(400, 550);
  current_screen_transform.RotateAboutZAxis(45);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  // Furthermore, in this scenario the bottom-right tile should have the larger
  // distance to visible.
  TilePriority top_left = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  TilePriority top_right = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  TilePriority bottom_left = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  TilePriority bottom_right = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(top_right.distance_to_visible_in_pixels,
            top_left.distance_to_visible_in_pixels);
  EXPECT_GT(bottom_left.distance_to_visible_in_pixels,
            top_left.distance_to_visible_in_pixels);

  EXPECT_GT(bottom_right.distance_to_visible_in_pixels,
            bottom_left.distance_to_visible_in_pixels);
  EXPECT_GT(bottom_right.distance_to_visible_in_pixels,
            top_right.distance_to_visible_in_pixels);
}

TEST(UpdateTilePrioritiesTest, PerspectiveLayer) {
  // Perspective transforms need to take a different code path.
  // This test checks tile priorities of a perspective layer.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);  // offscreen.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;
  size_t max_tiles_for_interest_area = 10000;

  // A 3d perspective layer rotated about its Y axis, translated to almost
  // fully offscreen. The left side will appear closer (i.e. larger in 2d) than
  // the right side, so the top-left tile will technically be closer than the
  // top-right.

  // Translate layer to offscreen
  current_screen_transform.Translate(400.0, 630.0);
  // Apply perspective about the center of the layer
  current_screen_transform.Translate(100.0, 100.0);
  current_screen_transform.ApplyPerspectiveDepth(100.0);
  current_screen_transform.RotateAboutYAxis(10.0);
  current_screen_transform.Translate(-100.0, -100.0);
  last_screen_transform = current_screen_transform;

  // Sanity check that this transform wouldn't cause w<0 clipping.
  bool clipped;
  MathUtil::MapQuad(current_screen_transform,
                    gfx::QuadF(gfx::RectF(0, 0, 200, 200)),
                    &clipped);
  ASSERT_FALSE(clipped);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  // All tiles will have a positive distance_to_visible
  // and an infinite time_to_visible.
  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  // Furthermore, in this scenario the top-left distance_to_visible
  // will be smallest, followed by top-right. The bottom layers
  // will of course be further than the top layers.
  TilePriority top_left = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  TilePriority top_right = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  TilePriority bottom_left = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  TilePriority bottom_right = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(top_right.distance_to_visible_in_pixels,
            top_left.distance_to_visible_in_pixels);

  EXPECT_GT(bottom_right.distance_to_visible_in_pixels,
            top_right.distance_to_visible_in_pixels);

  EXPECT_GT(bottom_left.distance_to_visible_in_pixels,
            top_left.distance_to_visible_in_pixels);
}

TEST(UpdateTilePrioritiesTest, PerspectiveLayerClippedByW) {
  // Perspective transforms need to take a different code path.
  // This test checks tile priorities of a perspective layer.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);  // offscreen.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;
  size_t max_tiles_for_interest_area = 10000;

  // A 3d perspective layer rotated about its Y axis, translated to almost
  // fully offscreen. The left side will appear closer (i.e. larger in 2d) than
  // the right side, so the top-left tile will technically be closer than the
  // top-right.

  // Translate layer to offscreen
  current_screen_transform.Translate(400.0, 970.0);
  // Apply perspective and rotation about the center of the layer
  current_screen_transform.Translate(100.0, 100.0);
  current_screen_transform.ApplyPerspectiveDepth(10.0);
  current_screen_transform.RotateAboutYAxis(10.0);
  current_screen_transform.Translate(-100.0, -100.0);
  last_screen_transform = current_screen_transform;

  // Sanity check that this transform does cause w<0 clipping for the left side
  // of the layer, but not the right side.
  bool clipped;
  MathUtil::MapQuad(current_screen_transform,
                    gfx::QuadF(gfx::RectF(0, 0, 100, 200)),
                    &clipped);
  ASSERT_TRUE(clipped);

  MathUtil::MapQuad(current_screen_transform,
                    gfx::QuadF(gfx::RectF(100, 0, 100, 200)),
                    &clipped);
  ASSERT_FALSE(clipped);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  // Left-side tiles will be clipped by the transform, so we have to assume
  // they are visible just in case.
  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  // Right-side tiles will have a positive distance_to_visible
  // and an infinite time_to_visible.
  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);
}

TEST(UpdateTilePrioritiesTest, BasicMotion) {
  // Test that time_to_visible is computed correctly when
  // there is some motion.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double last_frame_time_in_seconds = 1.0;
  double current_frame_time_in_seconds = 2.0;
  size_t max_tiles_for_interest_area = 10000;

  // Offscreen layer is coming closer to viewport at 1000 pixels per second.
  current_screen_transform.Translate(1800, 0);
  last_screen_transform.Translate(2800, 0);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  // previous ("last") frame
  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      last_layer_bounds,
      last_layer_contents_scale,
      last_layer_contents_scale,
      last_screen_transform,
      last_screen_transform,
      last_frame_time_in_seconds,
      max_tiles_for_interest_area);

  // current frame
  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(1.f,
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(1.f,
                  priority.time_to_visible_in_seconds);

  // time_to_visible for the right hand side layers needs an extra 0.099
  // seconds because this tile is 99 pixels further away.
  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(1.099f,
                  priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(1.099f,
                  priority.time_to_visible_in_seconds);
}

TEST(UpdateTilePrioritiesTest, RotationMotion) {
  // Each tile of a layer may be affected differently by a transform; Check
  // that UpdateTilePriorities correctly accounts for the transform between
  // layer space and screen space.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);  // offscren.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double last_frame_time_in_seconds = 1.0;
  double current_frame_time_in_seconds = 2.0;
  size_t max_tiles_for_interest_area = 10000;

  // Rotation motion is set up specifically so that:
  //  - rotation occurs about the center of the layer
  //  - the top-left tile becomes visible on rotation
  //  - the top-right tile will have an infinite time_to_visible
  //    because it is rotating away from viewport.
  //  - bottom-left layer will have a positive non-zero time_to_visible
  //    because it is rotating toward the viewport.
  current_screen_transform.Translate(400, 550);
  current_screen_transform.RotateAboutZAxis(45);

  last_screen_transform.Translate(400, 550);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  // previous ("last") frame
  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      last_layer_bounds,
      last_layer_contents_scale,
      last_layer_contents_scale,
      last_screen_transform,
      last_screen_transform,
      last_frame_time_in_seconds,
      max_tiles_for_interest_area);

  // current frame
  tiling->UpdateTilePriorities(
      ACTIVE_TREE,
      device_viewport,
      viewport_in_layer_space,
      visible_layer_rect,
      last_layer_bounds,
      current_layer_bounds,
      last_layer_contents_scale,
      current_layer_contents_scale,
      last_screen_transform,
      current_screen_transform,
      current_frame_time_in_seconds,
      max_tiles_for_interest_area);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible_in_pixels);
  EXPECT_FLOAT_EQ(0.f, priority.time_to_visible_in_seconds);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_GT(priority.time_to_visible_in_seconds, 0.f);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible_in_pixels, 0.f);
  EXPECT_FLOAT_EQ(std::numeric_limits<float>::infinity(),
                  priority.time_to_visible_in_seconds);
}

}  // namespace
}  // namespace cc
