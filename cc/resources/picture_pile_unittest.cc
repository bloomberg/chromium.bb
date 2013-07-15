// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  PictureListMap& picture_list_map() { return picture_list_map_; }

  typedef PicturePile::PictureList PictureList;
  typedef PicturePile::PictureListMapKey PictureListMapKey;
  typedef PicturePile::PictureListMap PictureListMap;

 protected:
    virtual ~TestPicturePile() {}
};

TEST(PicturePileTest, SmallInvalidateInflated) {
  FakeContentLayerClient client;
  FakeRenderingStatsInstrumentation stats_instrumentation;
  scoped_refptr<TestPicturePile> pile = new TestPicturePile;
  SkColor background_color = SK_ColorBLUE;

  float min_scale = 0.125;
  gfx::Size base_picture_size = pile->tiling().max_texture_size();

  gfx::Size layer_size = base_picture_size;
  pile->Resize(layer_size);
  pile->SetTileGridSize(gfx::Size(1000, 1000));
  pile->SetMinContentsScale(min_scale);

  // Update the whole layer.
  pile->Update(&client,
               background_color,
               false,
               gfx::Rect(layer_size),
               gfx::Rect(layer_size),
               &stats_instrumentation);

  // Invalidate something inside a tile.
  gfx::Rect invalidate_rect(50, 50, 1, 1);
  pile->Update(&client,
               background_color,
               false,
               invalidate_rect,
               gfx::Rect(layer_size),
               &stats_instrumentation);

  EXPECT_EQ(1, pile->tiling().num_tiles_x());
  EXPECT_EQ(1, pile->tiling().num_tiles_y());

  TestPicturePile::PictureList& picture_list =
      pile->picture_list_map().find(
          TestPicturePile::PictureListMapKey(0, 0))->second;
  EXPECT_EQ(2u, picture_list.size());
  for (TestPicturePile::PictureList::iterator it = picture_list.begin();
       it != picture_list.end();
       ++it) {
    scoped_refptr<Picture> picture = *it;
    gfx::Rect picture_rect =
        gfx::ScaleToEnclosedRect(picture->LayerRect(), min_scale);

    // The invalidation in each tile should have been made large enough
    // that scaling it never makes a rect smaller than 1 px wide or tall.
    EXPECT_FALSE(picture_rect.IsEmpty()) << "Picture rect " <<
        picture_rect.ToString();
  }
}

TEST(PicturePileTest, LargeInvalidateInflated) {
  FakeContentLayerClient client;
  FakeRenderingStatsInstrumentation stats_instrumentation;
  scoped_refptr<TestPicturePile> pile = new TestPicturePile;
  SkColor background_color = SK_ColorBLUE;

  float min_scale = 0.125;
  gfx::Size base_picture_size = pile->tiling().max_texture_size();

  gfx::Size layer_size = base_picture_size;
  pile->Resize(layer_size);
  pile->SetTileGridSize(gfx::Size(1000, 1000));
  pile->SetMinContentsScale(min_scale);

  // Update the whole layer.
  pile->Update(&client,
               background_color,
               false,
               gfx::Rect(layer_size),
               gfx::Rect(layer_size),
               &stats_instrumentation);

  // Invalidate something inside a tile.
  gfx::Rect invalidate_rect(50, 50, 100, 100);
  pile->Update(&client,
               background_color,
               false,
               invalidate_rect,
               gfx::Rect(layer_size),
               &stats_instrumentation);

  EXPECT_EQ(1, pile->tiling().num_tiles_x());
  EXPECT_EQ(1, pile->tiling().num_tiles_y());

  TestPicturePile::PictureList& picture_list =
      pile->picture_list_map().find(
          TestPicturePile::PictureListMapKey(0, 0))->second;
  EXPECT_EQ(2u, picture_list.size());

  int expected_inflation = pile->buffer_pixels();

  scoped_refptr<Picture> base_picture = *picture_list.begin();
  gfx::Rect base_picture_rect(layer_size);
  base_picture_rect.Inset(-expected_inflation, -expected_inflation);
  EXPECT_EQ(base_picture_rect.ToString(),
            base_picture->LayerRect().ToString());

  scoped_refptr<Picture> picture = *(++picture_list.begin());
  gfx::Rect picture_rect(invalidate_rect);
  picture_rect.Inset(-expected_inflation, -expected_inflation);
  EXPECT_EQ(picture_rect.ToString(),
            picture->LayerRect().ToString());
}

TEST(PicturePileTest, InvalidateOnTileBoundaryInflated) {
  FakeContentLayerClient client;
  FakeRenderingStatsInstrumentation stats_instrumentation;
  scoped_refptr<TestPicturePile> pile = new TestPicturePile;
  SkColor background_color = SK_ColorBLUE;

  float min_scale = 0.125;
  gfx::Size base_picture_size = pile->tiling().max_texture_size();

  gfx::Size layer_size =
      gfx::ToFlooredSize(gfx::ScaleSize(base_picture_size, 2.f));
  pile->Resize(layer_size);
  pile->SetTileGridSize(gfx::Size(1000, 1000));
  pile->SetMinContentsScale(min_scale);

  // Due to border pixels, we should have 3 tiles.
  EXPECT_EQ(3, pile->tiling().num_tiles_x());
  EXPECT_EQ(3, pile->tiling().num_tiles_y());

  // We should have 1/.125 - 1 = 7 border pixels.
  EXPECT_EQ(7, pile->buffer_pixels());
  EXPECT_EQ(7, pile->tiling().border_texels());

  // Update the whole layer.
  pile->Update(&client,
               background_color,
               false,
               gfx::Rect(layer_size),
               gfx::Rect(layer_size),
               &stats_instrumentation);

  // Invalidate something just over a tile boundary by a single pixel.
  // This will invalidate the tile (1, 1), as well as 1 row of pixels in (1, 0).
  gfx::Rect invalidate_rect(
      pile->tiling().TileBoundsWithBorder(0, 0).right(),
      pile->tiling().TileBoundsWithBorder(0, 0).bottom() - 1,
      50,
      50);
  pile->Update(&client,
               background_color,
               false,
               invalidate_rect,
               gfx::Rect(layer_size),
               &stats_instrumentation);

  for (int i = 0; i < pile->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile->tiling().num_tiles_y(); ++j) {
      // (1, 0) and (1, 1) should be invalidated partially.
      bool expect_invalidated = i == 1 && (j == 0 || j == 1);

      TestPicturePile::PictureList& picture_list =
          pile->picture_list_map().find(
              TestPicturePile::PictureListMapKey(i, j))->second;
      if (!expect_invalidated) {
        EXPECT_EQ(1u, picture_list.size()) << "For i,j " << i << "," << j;
        continue;
      }

      EXPECT_EQ(2u, picture_list.size()) << "For i,j " << i << "," << j;
      for (TestPicturePile::PictureList::iterator it = picture_list.begin();
           it != picture_list.end();
           ++it) {
        scoped_refptr<Picture> picture = *it;
        gfx::Rect picture_rect =
            gfx::ScaleToEnclosedRect(picture->LayerRect(), min_scale);

        // The invalidation in each tile should have been made large enough
        // that scaling it never makes a rect smaller than 1 px wide or tall.
        EXPECT_FALSE(picture_rect.IsEmpty()) << "Picture rect " <<
            picture_rect.ToString();
      }
    }
  }
}

}  // namespace
}  // namespace cc
