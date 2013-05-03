// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling.h"

#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

class TestablePictureLayerTiling : public PictureLayerTiling {
 public:
  using PictureLayerTiling::SetLiveTilesRect;

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
    gfx::Rect clamped_rect(gfx::ToEnclosingRect(gfx::ScaleRect(
        tiling_->ContentRect(), 1 / dest_to_contents_scale)));
    clamped_rect.Intersect(dest_rect);
    VerifyTilesExactlyCoverRect(rect_scale, dest_rect, clamped_rect);
  }

 protected:
  FakePictureLayerTilingClient client_;
  scoped_ptr<TestablePictureLayerTiling> tiling_;

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
      false,  // store screen space quads on tiles
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
      false,  // store screen space quads on tiles
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
      false,  // store screen space quads on tiles
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
      false,  // store screen space quads on tiles
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
      false,  // store screen space quads on tiles
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
      false,  // store screen space quads on tiles
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
      false,  // store screen space quads on tiles
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
      false,  // store screen space quads on tiles
      10000);  // max tiles in tile manager

  // The active tiling has tiles now.
  VerifyTiles(active_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));

  // Add the same tilings to the pending set.
  PictureLayerTilingSet pending_set(&client_, layer_bounds);
  pending_set.AddTilingsToMatchScales(active_set, 0.f);

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
      false,  // store screen space quads on tiles
      10000);  // max tiles in tile manager

  VerifyTiles(pending_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));
}

TEST_F(PictureLayerTilingIteratorTest, LCDText) {
  gfx::Size layer_bounds(1099, 801);
  gfx::Size tile_size(100, 100);

  Initialize(tile_size, 1.f, layer_bounds);


  tiling_->UpdateTilePriorities(
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
      false,  // store screen space quads on tiles
      10000);  // max tiles in tile manager

  VerifyTiles(tiling_.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));

  // Mark tiles in this rect as having text.
  gfx::Rect text_rect(350, 350, 400, 400);
  client_.set_text_rect(text_rect);

  // Prevent new tiles from being created.
  client_.set_allow_create_tile(false);

  client_.set_invalidation(gfx::Rect(layer_bounds));
  tiling_->DestroyAndRecreateTilesWithText();

  // Tiles touching the text_rect should be gone.
  VerifyTiles(tiling_.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TilesIntersectingRectExist, text_rect, false));
}

TEST_F(PictureLayerTilingIteratorTest, LCDText_CanRecreate) {
  gfx::Size layer_bounds(1099, 801);
  gfx::Size tile_size(100, 100);

  Initialize(tile_size, 1.f, layer_bounds);


  tiling_->UpdateTilePriorities(
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
      false,  // store screen space quads on tiles
      10000);  // max tiles in tile manager

  VerifyTiles(tiling_.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));

  // Mark tiles in this rect as having text.
  gfx::Rect text_rect(350, 350, 400, 400);
  client_.set_text_rect(text_rect);

  client_.set_invalidation(gfx::Rect(layer_bounds));
  tiling_->DestroyAndRecreateTilesWithText();

  // Tiles touching the text_rect are recreated.
  VerifyTiles(tiling_.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));
}

TEST_F(PictureLayerTilingIteratorTest, LCDText_WithTwin) {
  gfx::Size layer_bounds(1099, 801);
  gfx::Size tile_size(100, 100);

  Initialize(tile_size, 1.f, layer_bounds);

  // Make a twin tiling for |tiling_|.
  FakePictureLayerTilingClient twin_client;
  twin_client.SetTileSize(tile_size);

  scoped_ptr<PictureLayerTiling> twin_tiling =
      PictureLayerTiling::Create(1.f, layer_bounds, &twin_client);
  client_.set_twin_tiling(twin_tiling.get());

  VerifyTiles(tiling_.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, false));
  VerifyTiles(twin_tiling.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, false));

  tiling_->UpdateTilePriorities(
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
      false,  // store screen space quads on tiles
      10000);  // max tiles in tile manager
  twin_tiling->UpdateTilePriorities(
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
      false,  // store screen space quads on tiles
      10000);  // max tiles in tile manager

  VerifyTiles(tiling_.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));
  VerifyTiles(twin_tiling.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));

  // Mark tiles in this rect as having text.
  gfx::Rect text_rect(350, 350, 400, 400);
  client_.set_text_rect(text_rect);

  // Prevent new tiles from being created by the |tiling_|. It can still
  // get tiles from its twin.
  client_.set_allow_create_tile(false);

  client_.set_invalidation(gfx::Rect(layer_bounds));
  tiling_->DestroyAndRecreateTilesWithText();

  // Tiles touching the text_rect should be gone. Tiles from our twin are
  // not used because we are invalidated.
  VerifyTiles(tiling_.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TilesIntersectingRectExist, text_rect, false));
  VerifyTiles(twin_tiling.get(),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));

  // Destroy all tilings before the twin_client.
  tiling_.reset();
  twin_tiling.reset();
}

}  // namespace
}  // namespace cc
