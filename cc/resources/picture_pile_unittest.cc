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

  PictureMap& picture_map() { return picture_map_; }

  typedef PicturePile::PictureInfo PictureInfo;
  typedef PicturePile::PictureMapKey PictureMapKey;
  typedef PicturePile::PictureMap PictureMap;

 protected:
    virtual ~TestPicturePile() {}
};

class PicturePileTest : public testing::Test {
 public:
  PicturePileTest()
      : pile_(new TestPicturePile()),
        background_color_(SK_ColorBLUE),
        min_scale_(0.125),
        frame_number_(0),
        contents_opaque_(false) {
    pile_->Resize(pile_->tiling().max_texture_size());
    pile_->SetTileGridSize(gfx::Size(1000, 1000));
    pile_->SetMinContentsScale(min_scale_);
  }

  gfx::Rect layer_rect() const { return gfx::Rect(pile_->size()); }

  bool Update(const Region& invalidation, const gfx::Rect& visible_layer_rect) {
    frame_number_++;
    return pile_->Update(&client_,
                         background_color_,
                         contents_opaque_,
                         invalidation,
                         visible_layer_rect,
                         frame_number_,
                         &stats_instrumentation_);
  }

  bool UpdateWholeLayer() { return Update(layer_rect(), layer_rect()); }

  FakeContentLayerClient client_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  scoped_refptr<TestPicturePile> pile_;
  SkColor background_color_;
  float min_scale_;
  int frame_number_;
  bool contents_opaque_;
};

TEST_F(PicturePileTest, SmallInvalidateInflated) {
  UpdateWholeLayer();

  // Invalidate something inside a tile.
  gfx::Rect invalidate_rect(50, 50, 1, 1);
  Update(invalidate_rect, layer_rect());

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
  UpdateWholeLayer();

  // Invalidate something inside a tile.
  gfx::Rect invalidate_rect(50, 50, 100, 100);
  Update(invalidate_rect, layer_rect());

  EXPECT_EQ(1, pile_->tiling().num_tiles_x());
  EXPECT_EQ(1, pile_->tiling().num_tiles_y());

  TestPicturePile::PictureInfo& picture_info =
      pile_->picture_map().find(TestPicturePile::PictureMapKey(0, 0))->second;
  EXPECT_TRUE(!!picture_info.GetPicture());

  int expected_inflation = pile_->buffer_pixels();

  Picture* base_picture = picture_info.GetPicture();
  gfx::Rect base_picture_rect(pile_->size());
  base_picture_rect.Inset(-expected_inflation, -expected_inflation);
  EXPECT_EQ(base_picture_rect.ToString(),
            base_picture->LayerRect().ToString());
}

TEST_F(PicturePileTest, InvalidateOnTileBoundaryInflated) {
  gfx::Size layer_size = gfx::ToFlooredSize(gfx::ScaleSize(pile_->size(), 2.f));
  pile_->Resize(layer_size);

  // Due to border pixels, we should have 3 tiles.
  EXPECT_EQ(3, pile_->tiling().num_tiles_x());
  EXPECT_EQ(3, pile_->tiling().num_tiles_y());

  // We should have 1/.125 - 1 = 7 border pixels.
  EXPECT_EQ(7, pile_->buffer_pixels());
  EXPECT_EQ(7, pile_->tiling().border_texels());

  // Update the whole layer to create initial pictures.
  UpdateWholeLayer();

  // Invalidate everything again to have a non zero invalidation
  // frequency.
  UpdateWholeLayer();

  // Invalidate something just over a tile boundary by a single pixel.
  // This will invalidate the tile (1, 1), as well as 1 row of pixels in (1, 0).
  gfx::Rect invalidate_rect(
      pile_->tiling().TileBoundsWithBorder(0, 0).right(),
      pile_->tiling().TileBoundsWithBorder(0, 0).bottom() - 1,
      50,
      50);
  Update(invalidate_rect, layer_rect());

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
  gfx::Size layer_size = gfx::ToFlooredSize(gfx::ScaleSize(pile_->size(), 4.f));
  pile_->Resize(layer_size);

  gfx::Rect viewport(0, 0, layer_size.width(), 1);

  // Update the whole layer until the invalidation frequency is high.
  for (int frame = 0; frame < 33; ++frame) {
    UpdateWholeLayer();
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

  // Update once more with a small viewport 0,0 layer_width by 1
  Update(layer_rect(), viewport);

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

  // Now update with no invalidation and full viewport
  Update(gfx::Rect(), layer_rect());

  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;
      // Expect the invalidation frequency to be less than 1, since we just
      // updated with no invalidations.
      float expected_frequency =
          1.0f -
          1.0f / TestPicturePile::PictureInfo::INVALIDATION_FRAMES_TRACKED;

      EXPECT_FLOAT_EQ(expected_frequency,
                      picture_info.GetInvalidationFrequencyForTesting());

      // We expect that there are pictures everywhere now.
      EXPECT_TRUE(picture_info.GetPicture()) << "i " << i << " j " << j;
    }
  }
}

}  // namespace
}  // namespace cc
