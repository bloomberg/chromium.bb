// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "cc/resources/picture_pile.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

class TestPicturePile : public PicturePile {
 public:
  using PicturePile::buffer_pixels;
  using PicturePile::CanRasterSlowTileCheck;
  using PicturePile::Clear;

  PictureMap& picture_map() { return picture_map_; }
  const gfx::Rect& recorded_viewport() const { return recorded_viewport_; }

  bool CanRasterLayerRect(const gfx::Rect& layer_rect) {
    return CanRaster(1.f, layer_rect);
  }

  typedef PicturePile::PictureInfo PictureInfo;
  typedef PicturePile::PictureMapKey PictureMapKey;
  typedef PicturePile::PictureMap PictureMap;

 protected:
    virtual ~TestPicturePile() {}
};

class PicturePileTestBase {
 public:
  PicturePileTestBase()
      : background_color_(SK_ColorBLUE),
        min_scale_(0.125),
        frame_number_(0),
        contents_opaque_(false) {}

  void InitializeData() {
    pile_ = make_scoped_refptr(new TestPicturePile());
    pile_->SetTileGridSize(gfx::Size(1000, 1000));
    pile_->SetMinContentsScale(min_scale_);
    client_ = FakeContentLayerClient();
    SetTilingSize(pile_->tiling().max_texture_size());
  }

  void SetTilingSize(const gfx::Size& tiling_size) {
    Region invalidation;
    gfx::Rect viewport_rect(tiling_size);
    UpdateAndExpandInvalidation(&invalidation, tiling_size, viewport_rect);
  }

  gfx::Size tiling_size() const { return pile_->tiling_size(); }
  gfx::Rect tiling_rect() const { return gfx::Rect(pile_->tiling_size()); }

  bool UpdateAndExpandInvalidation(Region* invalidation,
                                   const gfx::Size& layer_size,
                                   const gfx::Rect& visible_layer_rect) {
    frame_number_++;
    return pile_->UpdateAndExpandInvalidation(&client_,
                                              invalidation,
                                              background_color_,
                                              contents_opaque_,
                                              false,
                                              layer_size,
                                              visible_layer_rect,
                                              frame_number_,
                                              Picture::RECORD_NORMALLY,
                                              &stats_instrumentation_);
  }

  bool UpdateWholePile() {
    Region invalidation = tiling_rect();
    bool result = UpdateAndExpandInvalidation(
        &invalidation, tiling_size(), tiling_rect());
    EXPECT_EQ(tiling_rect().ToString(), invalidation.ToString());
    return result;
  }

  FakeContentLayerClient client_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  scoped_refptr<TestPicturePile> pile_;
  SkColor background_color_;
  float min_scale_;
  int frame_number_;
  bool contents_opaque_;
};

class PicturePileTest : public PicturePileTestBase, public testing::Test {
 public:
  virtual void SetUp() OVERRIDE { InitializeData(); }
};

TEST_F(PicturePileTest, SmallInvalidateInflated) {
  // Invalidate something inside a tile.
  Region invalidate_rect(gfx::Rect(50, 50, 1, 1));
  UpdateAndExpandInvalidation(&invalidate_rect, tiling_size(), tiling_rect());
  EXPECT_EQ(gfx::Rect(50, 50, 1, 1).ToString(), invalidate_rect.ToString());

  EXPECT_EQ(1, pile_->tiling().num_tiles_x());
  EXPECT_EQ(1, pile_->tiling().num_tiles_y());

  TestPicturePile::PictureInfo& picture_info =
      pile_->picture_map().find(TestPicturePile::PictureMapKey(0, 0))->second;
  // We should have a picture.
  EXPECT_TRUE(!!picture_info.GetPicture());
  gfx::Rect picture_rect = gfx::ScaleToEnclosedRect(
      picture_info.GetPicture()->LayerRect(), min_scale_);

  // The the picture should be large enough that scaling it never makes a rect
  // smaller than 1 px wide or tall.
  EXPECT_FALSE(picture_rect.IsEmpty()) << "Picture rect " <<
      picture_rect.ToString();
}

TEST_F(PicturePileTest, LargeInvalidateInflated) {
  // Invalidate something inside a tile.
  Region invalidate_rect(gfx::Rect(50, 50, 100, 100));
  UpdateAndExpandInvalidation(&invalidate_rect, tiling_size(), tiling_rect());
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100).ToString(), invalidate_rect.ToString());

  EXPECT_EQ(1, pile_->tiling().num_tiles_x());
  EXPECT_EQ(1, pile_->tiling().num_tiles_y());

  TestPicturePile::PictureInfo& picture_info =
      pile_->picture_map().find(TestPicturePile::PictureMapKey(0, 0))->second;
  EXPECT_TRUE(!!picture_info.GetPicture());

  int expected_inflation = pile_->buffer_pixels();

  const Picture* base_picture = picture_info.GetPicture();
  gfx::Rect base_picture_rect(pile_->tiling_size());
  base_picture_rect.Inset(-expected_inflation, -expected_inflation);
  EXPECT_EQ(base_picture_rect.ToString(),
            base_picture->LayerRect().ToString());
}

TEST_F(PicturePileTest, InvalidateOnTileBoundaryInflated) {
  gfx::Size new_tiling_size =
      gfx::ToCeiledSize(gfx::ScaleSize(pile_->tiling_size(), 2.f));
  // This creates initial pictures.
  SetTilingSize(new_tiling_size);

  // Due to border pixels, we should have 3 tiles.
  EXPECT_EQ(3, pile_->tiling().num_tiles_x());
  EXPECT_EQ(3, pile_->tiling().num_tiles_y());

  // We should have 1/.125 - 1 = 7 border pixels.
  EXPECT_EQ(7, pile_->buffer_pixels());
  EXPECT_EQ(7, pile_->tiling().border_texels());

  // Invalidate everything to have a non zero invalidation frequency.
  UpdateWholePile();

  // Invalidate something just over a tile boundary by a single pixel.
  // This will invalidate the tile (1, 1), as well as 1 row of pixels in (1, 0).
  Region invalidate_rect(
      gfx::Rect(pile_->tiling().TileBoundsWithBorder(0, 0).right(),
                pile_->tiling().TileBoundsWithBorder(0, 0).bottom() - 1,
                50,
                50));
  Region expected_invalidation = invalidate_rect;
  UpdateAndExpandInvalidation(&invalidate_rect, tiling_size(), tiling_rect());
  EXPECT_EQ(expected_invalidation.ToString(), invalidate_rect.ToString());

  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;

      // Expect (1, 1) and (1, 0) to be invalidated once more
      // than the rest of the tiles.
      if (i == 1 && (j == 0 || j == 1)) {
        EXPECT_FLOAT_EQ(
            2.0f / TestPicturePile::PictureInfo::INVALIDATION_FRAMES_TRACKED,
            picture_info.GetInvalidationFrequencyForTesting());
      } else {
        EXPECT_FLOAT_EQ(
            1.0f / TestPicturePile::PictureInfo::INVALIDATION_FRAMES_TRACKED,
            picture_info.GetInvalidationFrequencyForTesting());
      }
    }
  }
}

TEST_F(PicturePileTest, StopRecordingOffscreenInvalidations) {
  gfx::Size new_tiling_size =
      gfx::ToCeiledSize(gfx::ScaleSize(pile_->tiling_size(), 4.f));
  SetTilingSize(new_tiling_size);

  gfx::Rect viewport(tiling_size().width(), 1);

  // Update the whole pile until the invalidation frequency is high.
  for (int frame = 0; frame < 33; ++frame) {
    UpdateWholePile();
  }

  // Make sure we have a high invalidation frequency.
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;
      EXPECT_FLOAT_EQ(1.0f, picture_info.GetInvalidationFrequencyForTesting())
          << "i " << i << " j " << j;
    }
  }

  // Update once more with a small viewport.
  Region invalidation(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation, tiling_size(), viewport);
  EXPECT_EQ(tiling_rect().ToString(), invalidation.ToString());

  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;
      EXPECT_FLOAT_EQ(1.0f, picture_info.GetInvalidationFrequencyForTesting());

      // If the y far enough away we expect to find no picture (no re-recording
      // happened). For close y, the picture should change.
      if (j >= 2)
        EXPECT_FALSE(picture_info.GetPicture()) << "i " << i << " j " << j;
      else
        EXPECT_TRUE(picture_info.GetPicture()) << "i " << i << " j " << j;
    }
  }

  // Update a partial tile that doesn't get recorded. We should expand the
  // invalidation to the entire tiles that overlap it.
  Region small_invalidation =
      gfx::Rect(pile_->tiling().TileBounds(3, 4).x(),
                pile_->tiling().TileBounds(3, 4).y() + 10,
                1,
                1);
  UpdateAndExpandInvalidation(&small_invalidation, tiling_size(), viewport);
  EXPECT_TRUE(small_invalidation.Contains(gfx::UnionRects(
      pile_->tiling().TileBounds(2, 4), pile_->tiling().TileBounds(3, 4))))
      << small_invalidation.ToString();

  // Now update with no invalidation and full viewport
  Region empty_invalidation;
  UpdateAndExpandInvalidation(
      &empty_invalidation, tiling_size(), tiling_rect());
  EXPECT_EQ(Region().ToString(), empty_invalidation.ToString());

  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;
      // Expect the invalidation frequency to be less than 1, since we just
      // updated with no invalidations.
      EXPECT_LT(picture_info.GetInvalidationFrequencyForTesting(), 1.f);

      // We expect that there are pictures everywhere now.
      EXPECT_TRUE(picture_info.GetPicture()) << "i " << i << " j " << j;
    }
  }
}

TEST_F(PicturePileTest, ClearingInvalidatesRecordedRect) {
  gfx::Rect rect(0, 0, 5, 5);
  EXPECT_TRUE(pile_->CanRasterLayerRect(rect));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(rect));

  pile_->Clear();

  // Make sure both the cache-aware check (using recorded region) and the normal
  // check are both false after clearing.
  EXPECT_FALSE(pile_->CanRasterLayerRect(rect));
  EXPECT_FALSE(pile_->CanRasterSlowTileCheck(rect));
}

TEST_F(PicturePileTest, FrequentInvalidationCanRaster) {
  // This test makes sure that if part of the page is frequently invalidated
  // and doesn't get re-recorded, then CanRaster is not true for any
  // tiles touching it, but is true for adjacent tiles, even if it
  // overlaps on borders (edge case).
  gfx::Size new_tiling_size =
      gfx::ToCeiledSize(gfx::ScaleSize(pile_->tiling_size(), 4.f));
  SetTilingSize(new_tiling_size);

  gfx::Rect tile01_borders = pile_->tiling().TileBoundsWithBorder(0, 1);
  gfx::Rect tile02_borders = pile_->tiling().TileBoundsWithBorder(0, 2);
  gfx::Rect tile01_noborders = pile_->tiling().TileBounds(0, 1);
  gfx::Rect tile02_noborders = pile_->tiling().TileBounds(0, 2);

  // Sanity check these two tiles are overlapping with borders, since this is
  // what the test is trying to repro.
  EXPECT_TRUE(tile01_borders.Intersects(tile02_borders));
  EXPECT_FALSE(tile01_noborders.Intersects(tile02_noborders));
  UpdateWholePile();
  EXPECT_TRUE(pile_->CanRasterLayerRect(tile01_noborders));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(tile01_noborders));
  EXPECT_TRUE(pile_->CanRasterLayerRect(tile02_noborders));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(tile02_noborders));
  // Sanity check that an initial paint goes down the fast path of having
  // a valid recorded viewport.
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());

  // Update the whole layer until the invalidation frequency is high.
  for (int frame = 0; frame < 33; ++frame) {
    UpdateWholePile();
  }

  // Update once more with a small viewport.
  gfx::Rect viewport(tiling_size().width(), 1);
  Region invalidation(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation, tiling_size(), viewport);
  EXPECT_EQ(tiling_rect().ToString(), invalidation.ToString());

  // Sanity check some pictures exist and others don't.
  EXPECT_TRUE(pile_->picture_map()
                  .find(TestPicturePile::PictureMapKey(0, 1))
                  ->second.GetPicture());
  EXPECT_FALSE(pile_->picture_map()
                   .find(TestPicturePile::PictureMapKey(0, 2))
                   ->second.GetPicture());

  EXPECT_TRUE(pile_->CanRasterLayerRect(tile01_noborders));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(tile01_noborders));
  EXPECT_FALSE(pile_->CanRasterLayerRect(tile02_noborders));
  EXPECT_FALSE(pile_->CanRasterSlowTileCheck(tile02_noborders));
}

TEST_F(PicturePileTest, NoInvalidationValidViewport) {
  // This test validates that the recorded_viewport cache of full tiles
  // is still valid for some use cases.  If it's not, it's a performance
  // issue because CanRaster checks will go down the slow path.
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());

  // No invalidation, same viewport.
  Region invalidation;
  UpdateAndExpandInvalidation(&invalidation, tiling_size(), tiling_rect());
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());
  EXPECT_EQ(Region().ToString(), invalidation.ToString());

  // Partial invalidation, same viewport.
  invalidation = gfx::Rect(0, 0, 1, 1);
  UpdateAndExpandInvalidation(&invalidation, tiling_size(), tiling_rect());
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());
  EXPECT_EQ(gfx::Rect(0, 0, 1, 1).ToString(), invalidation.ToString());

  // No invalidation, changing viewport.
  invalidation = Region();
  UpdateAndExpandInvalidation(
      &invalidation, tiling_size(), gfx::Rect(5, 5, 5, 5));
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
}

TEST_F(PicturePileTest, InvalidationOutsideRecordingRect) {
  gfx::Size huge_layer_size(10000000, 20000000);
  gfx::Rect viewport(300000, 400000, 5000, 6000);

  // Resize the pile and set up the interest rect.
  Region invalidation;
  UpdateAndExpandInvalidation(&invalidation, huge_layer_size, viewport);

  // Invalidation inside the recording rect does not need to be expanded.
  invalidation = viewport;
  UpdateAndExpandInvalidation(&invalidation, huge_layer_size, viewport);
  EXPECT_EQ(viewport.ToString(), invalidation.ToString());

  // Invalidation outside the recording rect should expand to the tiles it
  // covers.
  gfx::Rect recorded_over_tiles =
      pile_->tiling().ExpandRectToTileBounds(pile_->recorded_viewport());
  gfx::Rect invalidation_outside(
      recorded_over_tiles.right(), recorded_over_tiles.y(), 30, 30);
  invalidation = invalidation_outside;
  UpdateAndExpandInvalidation(&invalidation, huge_layer_size, viewport);
  gfx::Rect expanded_recorded_viewport =
      pile_->tiling().ExpandRectToTileBounds(pile_->recorded_viewport());
  Region expected_invalidation =
      pile_->tiling().ExpandRectToTileBounds(invalidation_outside);
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
}

enum Corner {
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_RIGHT,
};

class PicturePileResizeCornerTest : public PicturePileTestBase,
                                    public testing::TestWithParam<Corner> {
 protected:
  virtual void SetUp() OVERRIDE { InitializeData(); }

  static gfx::Rect CornerSinglePixelRect(Corner corner, const gfx::Size& s) {
    switch (corner) {
      case TOP_LEFT:
        return gfx::Rect(0, 0, 1, 1);
      case TOP_RIGHT:
        return gfx::Rect(s.width() - 1, 0, 1, 1);
      case BOTTOM_LEFT:
        return gfx::Rect(0, s.height() - 1, 1, 1);
      case BOTTOM_RIGHT:
        return gfx::Rect(s.width() - 1, s.height() - 1, 1, 1);
    }
    NOTREACHED();
    return gfx::Rect();
  }
};

TEST_P(PicturePileResizeCornerTest, ResizePileOutsideInterestRect) {
  Corner corner = GetParam();

  // This size chosen to be larger than the interest rect size, which is
  // at least kPixelDistanceToRecord * 2 in each dimension.
  int tile_size = 100000;
  // The small number subtracted keeps the last tile in each axis larger than
  // the interest rect also.
  int offset = -100;
  gfx::Size base_tiling_size(6 * tile_size + offset, 6 * tile_size + offset);
  gfx::Size grow_down_tiling_size(6 * tile_size + offset,
                                  8 * tile_size + offset);
  gfx::Size grow_right_tiling_size(8 * tile_size + offset,
                                   6 * tile_size + offset);
  gfx::Size grow_both_tiling_size(8 * tile_size + offset,
                                  8 * tile_size + offset);

  Region invalidation;
  Region expected_invalidation;

  pile_->tiling().SetMaxTextureSize(gfx::Size(tile_size, tile_size));
  SetTilingSize(base_tiling_size);

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  UpdateAndExpandInvalidation(
      &invalidation,
      grow_down_tiling_size,
      CornerSinglePixelRect(corner, grow_down_tiling_size));

  // We should have lost the recordings in the bottom row.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(8, pile_->tiling().num_tiles_y());
  for (int i = 0; i < 6; ++i) {
    for (int j = 0; j < 6; ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_EQ(j < 5, it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated all new pixels in the recording.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_down_tiling_size),
                                          gfx::Rect(base_tiling_size));
  // But the new pixels don't cover the whole bottom row.
  gfx::Rect bottom_row = gfx::UnionRects(pile_->tiling().TileBounds(0, 5),
                                         pile_->tiling().TileBounds(5, 5));
  EXPECT_FALSE(expected_invalidation.Contains(bottom_row));
  // We invalidated the entire old bottom row.
  expected_invalidation.Union(bottom_row);
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation,
                              base_tiling_size,
                              CornerSinglePixelRect(corner, base_tiling_size));

  // We should have lost the recordings that are now outside the tiling only.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_EQ(j < 6, it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  expected_invalidation.Clear();
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation,
      grow_right_tiling_size,
      CornerSinglePixelRect(corner, grow_right_tiling_size));

  // We should have lost the recordings in the right column.
  EXPECT_EQ(8, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < 6; ++i) {
    for (int j = 0; j < 6; ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_EQ(i < 5, it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated all new pixels in the recording.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_right_tiling_size),
                                          gfx::Rect(base_tiling_size));
  // But the new pixels don't cover the whole right_column.
  gfx::Rect right_column = gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                           pile_->tiling().TileBounds(5, 5));
  EXPECT_FALSE(expected_invalidation.Contains(right_column));
  // We invalidated the entire old right column.
  expected_invalidation.Union(right_column);
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation,
                              base_tiling_size,
                              CornerSinglePixelRect(corner, base_tiling_size));

  // We should have lost the recordings that are now outside the tiling only.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_EQ(i < 6, it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  expected_invalidation.Clear();
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation,
      grow_both_tiling_size,
      CornerSinglePixelRect(corner, grow_both_tiling_size));

  // We should have lost the recordings in the right column and bottom row.
  EXPECT_EQ(8, pile_->tiling().num_tiles_x());
  EXPECT_EQ(8, pile_->tiling().num_tiles_y());
  for (int i = 0; i < 6; ++i) {
    for (int j = 0; j < 6; ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_EQ(i < 5 && j < 5, it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated all new pixels in the recording.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_both_tiling_size),
                                          gfx::Rect(base_tiling_size));
  // But the new pixels don't cover the whole right_column.
  Region right_column_and_bottom_row =
      UnionRegions(gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                   pile_->tiling().TileBounds(5, 5)),
                   gfx::UnionRects(pile_->tiling().TileBounds(0, 5),
                                   pile_->tiling().TileBounds(5, 5)));
  EXPECT_FALSE(expected_invalidation.Contains(right_column_and_bottom_row));
  // We invalidated the entire old right column and the old bottom row.
  expected_invalidation.Union(right_column_and_bottom_row);
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation, base_tiling_size, gfx::Rect());

  // We should have lost the recordings that are now outside the tiling only.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_EQ(i < 6 && j < 6, it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  expected_invalidation.Clear();
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();
}

TEST_P(PicturePileResizeCornerTest, SmallResizePileOutsideInterestRect) {
  Corner corner = GetParam();

  // This size chosen to be larger than the interest rect size, which is
  // at least kPixelDistanceToRecord * 2 in each dimension.
  int tile_size = 100000;
  // The small number subtracted keeps the last tile in each axis larger than
  // the interest rect also.
  int offset = -100;
  gfx::Size base_tiling_size(6 * tile_size + offset, 6 * tile_size + offset);
  gfx::Size grow_down_tiling_size(6 * tile_size + offset,
                                  6 * tile_size + offset + 5);
  gfx::Size grow_right_tiling_size(6 * tile_size + offset + 5,
                                   6 * tile_size + offset);
  gfx::Size grow_both_tiling_size(6 * tile_size + offset + 5,
                                  6 * tile_size + offset + 5);

  Region invalidation;
  Region expected_invalidation;

  pile_->tiling().SetMaxTextureSize(gfx::Size(tile_size, tile_size));
  SetTilingSize(base_tiling_size);

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  UpdateAndExpandInvalidation(
      &invalidation,
      grow_down_tiling_size,
      CornerSinglePixelRect(corner, grow_down_tiling_size));

  // We should have lost the recordings in the bottom row that do not intersect
  // the interest rect.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      bool expect_tile;
      switch (corner) {
        case TOP_LEFT:
        case TOP_RIGHT:
          expect_tile = j < 5;
          break;
        case BOTTOM_LEFT:
          // The interest rect in the bottom left tile means we'll record it.
          expect_tile = j < 5 || (j == 5 && i == 0);
          break;
        case BOTTOM_RIGHT:
          // The interest rect in the bottom right tile means we'll record it.
          expect_tile = j < 5 || (j == 5 && i == 5);
          break;
      }
      EXPECT_EQ(expect_tile, it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the bottom row outside the new interest rect. The tile that
  // insects the interest rect in invalidated only on its new pixels.
  switch (corner) {
    case TOP_LEFT:
    case TOP_RIGHT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(0, 5),
                                              pile_->tiling().TileBounds(5, 5));
      break;
    case BOTTOM_LEFT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(1, 5),
                                              pile_->tiling().TileBounds(5, 5));
      expected_invalidation.Union(SubtractRects(
          pile_->tiling().TileBounds(0, 5), gfx::Rect(base_tiling_size)));
      break;
    case BOTTOM_RIGHT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(0, 5),
                                              pile_->tiling().TileBounds(4, 5));
      expected_invalidation.Union(SubtractRects(
          pile_->tiling().TileBounds(5, 5), gfx::Rect(base_tiling_size)));
      break;
  }
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation,
                              base_tiling_size,
                              CornerSinglePixelRect(corner, base_tiling_size));

  // We should have lost nothing.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated nothing.
  expected_invalidation.Clear();
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation,
      grow_right_tiling_size,
      CornerSinglePixelRect(corner, grow_right_tiling_size));

  // We should have lost the recordings in the right column.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      bool expect_tile;
      switch (corner) {
        case TOP_LEFT:
        case BOTTOM_LEFT:
          expect_tile = i < 5;
          break;
        case TOP_RIGHT:
          // The interest rect in the top right tile means we'll record it.
          expect_tile = i < 5 || (j == 0 && i == 5);
          break;
        case BOTTOM_RIGHT:
          // The interest rect in the bottom right tile means we'll record it.
          expect_tile = i < 5 || (j == 5 && i == 5);
          break;
      }
      EXPECT_EQ(expect_tile, it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the right column outside the new interest rect. The tile
  // that insects the interest rect in invalidated only on its new pixels.
  switch (corner) {
    case TOP_LEFT:
    case BOTTOM_LEFT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                              pile_->tiling().TileBounds(5, 5));
      break;
    case TOP_RIGHT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(5, 1),
                                              pile_->tiling().TileBounds(5, 5));
      expected_invalidation.Union(SubtractRects(
          pile_->tiling().TileBounds(5, 0), gfx::Rect(base_tiling_size)));
      break;
    case BOTTOM_RIGHT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                              pile_->tiling().TileBounds(5, 4));
      expected_invalidation.Union(SubtractRects(
          pile_->tiling().TileBounds(5, 5), gfx::Rect(base_tiling_size)));
      break;
  }
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation,
                              base_tiling_size,
                              CornerSinglePixelRect(corner, base_tiling_size));

  // We should have lost nothing.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated nothing.
  expected_invalidation.Clear();
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation,
      grow_both_tiling_size,
      CornerSinglePixelRect(corner, grow_both_tiling_size));

  // We should have lost the recordings in the right column and bottom row. The
  // tile that insects the interest rect in invalidated only on its new pixels.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      bool expect_tile;
      switch (corner) {
        case TOP_LEFT:
          expect_tile = i < 5 && j < 5;
          break;
        case TOP_RIGHT:
          // The interest rect in the top right tile means we'll record it.
          expect_tile = (i < 5 && j < 5) || (j == 0 && i == 5);
          break;
        case BOTTOM_LEFT:
          // The interest rect in the bottom left tile means we'll record it.
          expect_tile = (i < 5 && j < 5) || (j == 5 && i == 0);
          break;
        case BOTTOM_RIGHT:
          // The interest rect in the bottom right tile means we'll record it.
          expect_tile = (i < 5 && j < 5) || (j == 5 && i == 5);
          break;
      }
      EXPECT_EQ(expect_tile, it != map.end() && it->second.GetPicture())
          << i << "," << j;
    }
  }

  // We invalidated the right column and the bottom row outside the new interest
  // rect. The tile that insects the interest rect in invalidated only on its
  // new pixels.
  switch (corner) {
    case TOP_LEFT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                              pile_->tiling().TileBounds(5, 5));
      expected_invalidation.Union(gfx::UnionRects(
          pile_->tiling().TileBounds(0, 5), pile_->tiling().TileBounds(5, 5)));
      break;
    case TOP_RIGHT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(5, 1),
                                              pile_->tiling().TileBounds(5, 5));
      expected_invalidation.Union(gfx::UnionRects(
          pile_->tiling().TileBounds(0, 5), pile_->tiling().TileBounds(5, 5)));
      expected_invalidation.Union(SubtractRects(
          pile_->tiling().TileBounds(5, 0), gfx::Rect(base_tiling_size)));
      break;
    case BOTTOM_LEFT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                              pile_->tiling().TileBounds(5, 5));
      expected_invalidation.Union(gfx::UnionRects(
          pile_->tiling().TileBounds(1, 5), pile_->tiling().TileBounds(5, 5)));
      expected_invalidation.Union(SubtractRects(
          pile_->tiling().TileBounds(0, 5), gfx::Rect(base_tiling_size)));
      break;
    case BOTTOM_RIGHT:
      expected_invalidation = gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                              pile_->tiling().TileBounds(5, 4));
      expected_invalidation.Union(gfx::UnionRects(
          pile_->tiling().TileBounds(0, 5), pile_->tiling().TileBounds(4, 5)));
      expected_invalidation.Union(SubtractRegions(
          pile_->tiling().TileBounds(5, 5), gfx::Rect(base_tiling_size)));
      break;
  }
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation,
                              base_tiling_size,
                              CornerSinglePixelRect(corner, base_tiling_size));

  // We should have lost nothing.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated nothing.
  expected_invalidation.Clear();
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();
}

INSTANTIATE_TEST_CASE_P(
    PicturePileResizeCornerTests,
    PicturePileResizeCornerTest,
    ::testing::Values(TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT));

TEST_F(PicturePileTest, ResizePileInsideInterestRect) {
  // This size chosen to be small enough that all the rects below fit inside the
  // the interest rect, so they are smaller than kPixelDistanceToRecord in each
  // dimension.
  int tile_size = 100;
  gfx::Size base_tiling_size(5 * tile_size, 5 * tile_size);
  gfx::Size grow_down_tiling_size(5 * tile_size, 7 * tile_size);
  gfx::Size grow_right_tiling_size(7 * tile_size, 5 * tile_size);
  gfx::Size grow_both_tiling_size(7 * tile_size, 7 * tile_size);

  Region invalidation;
  Region expected_invalidation;

  pile_->tiling().SetMaxTextureSize(gfx::Size(tile_size, tile_size));
  SetTilingSize(base_tiling_size);

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  UpdateAndExpandInvalidation(
      &invalidation, grow_down_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(8, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the newly exposed pixels on the bottom row of tiles.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_down_tiling_size),
                                          gfx::Rect(base_tiling_size));
  Region bottom_row_new_pixels =
      SubtractRegions(gfx::UnionRects(pile_->tiling().TileBounds(0, 5),
                                      pile_->tiling().TileBounds(5, 5)),
                      gfx::Rect(base_tiling_size));
  EXPECT_TRUE(expected_invalidation.Contains(bottom_row_new_pixels));
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation, base_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation, grow_right_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(8, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the newly exposed pixels on the right column of tiles.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_right_tiling_size),
                                          gfx::Rect(base_tiling_size));
  Region right_column_new_pixels =
      SubtractRegions(gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                      pile_->tiling().TileBounds(5, 5)),
                      gfx::Rect(base_tiling_size));
  EXPECT_TRUE(expected_invalidation.Contains(right_column_new_pixels));
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation, base_tiling_size, gfx::Rect(1, 1));

  // We should have lost the recordings that are now outside the tiling only.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation, grow_both_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(8, pile_->tiling().num_tiles_x());
  EXPECT_EQ(8, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the newly exposed pixels on the bottom row and right column
  // of tiles.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_both_tiling_size),
                                          gfx::Rect(base_tiling_size));
  Region bottom_row_and_right_column_new_pixels = SubtractRegions(
      UnionRegions(gfx::UnionRects(pile_->tiling().TileBounds(0, 5),
                                   pile_->tiling().TileBounds(5, 5)),
                   gfx::UnionRects(pile_->tiling().TileBounds(5, 0),
                                   pile_->tiling().TileBounds(5, 5))),
      gfx::Rect(base_tiling_size));
  EXPECT_TRUE(
      expected_invalidation.Contains(bottom_row_and_right_column_new_pixels));
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation, base_tiling_size, gfx::Rect());

  // We should have lost the recordings that are now outside the tiling only.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
  invalidation.Clear();
}

TEST_F(PicturePileTest, SmallResizePileInsideInterestRect) {
  // This size chosen to be small enough that all the rects below fit inside the
  // the interest rect, so they are smaller than kPixelDistanceToRecord in each
  // dimension.
  int tile_size = 100;
  gfx::Size base_tiling_size(5 * tile_size, 5 * tile_size);
  gfx::Size grow_down_tiling_size(5 * tile_size, 5 * tile_size + 5);
  gfx::Size grow_right_tiling_size(5 * tile_size + 5, 5 * tile_size);
  gfx::Size grow_both_tiling_size(5 * tile_size + 5, 5 * tile_size + 5);

  Region invalidation;
  Region expected_invalidation;

  pile_->tiling().SetMaxTextureSize(gfx::Size(tile_size, tile_size));
  SetTilingSize(base_tiling_size);

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  UpdateAndExpandInvalidation(
      &invalidation, grow_down_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the newly exposed pixels.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_down_tiling_size),
                                          gfx::Rect(base_tiling_size));
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation, base_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation, grow_right_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the newly exposed pixels.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_right_tiling_size),
                                          gfx::Rect(base_tiling_size));
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation, base_tiling_size, gfx::Rect(1, 1));

  // We should have lost the recordings that are now outside the tiling only.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(
      &invalidation, grow_both_tiling_size, gfx::Rect(1, 1));

  // We should have a recording for every tile.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // We invalidated the newly exposed pixels.
  expected_invalidation = SubtractRegions(gfx::Rect(grow_both_tiling_size),
                                          gfx::Rect(base_tiling_size));
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
  invalidation.Clear();

  UpdateWholePile();
  UpdateAndExpandInvalidation(&invalidation, base_tiling_size, gfx::Rect());

  // We should have lost the recordings that are now outside the tiling only.
  EXPECT_EQ(6, pile_->tiling().num_tiles_x());
  EXPECT_EQ(6, pile_->tiling().num_tiles_y());
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureMapKey key(i, j);
      TestPicturePile::PictureMap& map = pile_->picture_map();
      TestPicturePile::PictureMap::iterator it = map.find(key);
      EXPECT_TRUE(it != map.end() && it->second.GetPicture());
    }
  }

  // No invalidation when shrinking.
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
  invalidation.Clear();
}

TEST_F(PicturePileTest, SolidRectangleIsSolid) {
  // If the client has no contents, the solid state will be true.
  Region invalidation1(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation1, tiling_size(), tiling_rect());
  EXPECT_TRUE(pile_->is_solid_color());
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), pile_->solid_color());

  // If there is a single rect that covers the view, the solid
  // state will be true.
  SkPaint paint;
  paint.setColor(SK_ColorCYAN);
  client_.add_draw_rect(tiling_rect(), paint);
  Region invalidation2(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation2, tiling_size(), tiling_rect());
  EXPECT_TRUE(pile_->is_solid_color());
  EXPECT_EQ(SK_ColorCYAN, pile_->solid_color());

  // If a second smaller rect is draw that doesn't cover the viewport
  // completely, the solid state will be false.
  gfx::Rect smallRect = tiling_rect();
  smallRect.Inset(10, 10, 10, 10);
  client_.add_draw_rect(smallRect, paint);
  Region invalidation3(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation3, tiling_size(), tiling_rect());
  EXPECT_FALSE(pile_->is_solid_color());

  // If a third rect is drawn over everything, we should be solid again.
  paint.setColor(SK_ColorRED);
  client_.add_draw_rect(tiling_rect(), paint);
  Region invalidation4(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation4, tiling_size(), tiling_rect());
  EXPECT_TRUE(pile_->is_solid_color());
  EXPECT_EQ(SK_ColorRED, pile_->solid_color());

  // If we draw too many, we don't bother doing the analysis and we should no
  // longer be in a solid state.  There are 8 rects, two clips and a translate.
  client_.add_draw_rect(tiling_rect(), paint);
  client_.add_draw_rect(tiling_rect(), paint);
  client_.add_draw_rect(tiling_rect(), paint);
  client_.add_draw_rect(tiling_rect(), paint);
  client_.add_draw_rect(tiling_rect(), paint);
  Region invalidation5(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation5, tiling_size(), tiling_rect());
  EXPECT_FALSE(pile_->is_solid_color());
}

}  // namespace
}  // namespace cc
