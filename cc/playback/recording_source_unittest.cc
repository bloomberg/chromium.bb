// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "cc/playback/display_list_raster_source.h"
#include "cc/test/fake_display_list_recording_source.h"
#include "cc/test/fake_picture_pile.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

template <class T>
scoped_ptr<T> CreateRecordingSource(const gfx::Rect& viewport,
                                    const gfx::Size& grid_cell_size);

template <>
scoped_ptr<FakePicturePile> CreateRecordingSource<FakePicturePile>(
    const gfx::Rect& viewport,
    const gfx::Size& grid_cell_size) {
  return FakePicturePile::CreateFilledPile(grid_cell_size, viewport.size());
}

template <>
scoped_ptr<FakeDisplayListRecordingSource> CreateRecordingSource<
    FakeDisplayListRecordingSource>(const gfx::Rect& viewport,
                                    const gfx::Size& grid_cell_size) {
  gfx::Rect layer_rect(viewport.right(), viewport.bottom());
  scoped_ptr<FakeDisplayListRecordingSource> recording_source =
      FakeDisplayListRecordingSource::CreateRecordingSource(viewport,
                                                            layer_rect.size());
  recording_source->SetGridCellSize(grid_cell_size);

  return recording_source.Pass();
}

template <class T>
scoped_refptr<RasterSource> CreateRasterSource(T* recording_source);

template <>
scoped_refptr<RasterSource> CreateRasterSource(
    FakePicturePile* recording_source) {
  return FakePicturePileImpl::CreateFromPile(recording_source, nullptr);
}

template <>
scoped_refptr<RasterSource> CreateRasterSource(
    FakeDisplayListRecordingSource* recording_source) {
  bool can_use_lcd_text = true;
  return DisplayListRasterSource::CreateFromDisplayListRecordingSource(
      recording_source, can_use_lcd_text);
}

template <typename T>
class RecordingSourceTest : public testing::Test {};

using testing::Types;

typedef Types<FakePicturePile, FakeDisplayListRecordingSource>
    RecordingSourceImplementations;

TYPED_TEST_CASE(RecordingSourceTest, RecordingSourceImplementations);

TYPED_TEST(RecordingSourceTest, NoGatherImageEmptyImages) {
  gfx::Size grid_cell_size(128, 128);
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  scoped_ptr<TypeParam> recording_source =
      CreateRecordingSource<TypeParam>(recorded_viewport, grid_cell_size);
  recording_source->SetGatherDiscardableImages(false);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource<TypeParam>(recording_source.get());

  // If recording source do not gather images, raster source is not going to
  // get images.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(recorded_viewport, &images);
    EXPECT_TRUE(images.empty());
  }
}

TYPED_TEST(RecordingSourceTest, EmptyImages) {
  gfx::Size grid_cell_size(128, 128);
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  scoped_ptr<TypeParam> recording_source =
      CreateRecordingSource<TypeParam>(recorded_viewport, grid_cell_size);
  recording_source->SetGatherDiscardableImages(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource<TypeParam>(recording_source.get());

  // Tile sized iterators.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 128, 128), &images);
    EXPECT_TRUE(images.empty());
  }
  // Shifted tile sized iterators.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(140, 140, 128, 128),
                                           &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 256, 256), &images);
    EXPECT_TRUE(images.empty());
  }
}

TYPED_TEST(RecordingSourceTest, NoDiscardableImages) {
  gfx::Size grid_cell_size(128, 128);
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  scoped_ptr<TypeParam> recording_source =
      CreateRecordingSource<TypeParam>(recorded_viewport, grid_cell_size);

  SkPaint simple_paint;
  simple_paint.setColor(SkColorSetARGB(255, 12, 23, 34));

  SkBitmap non_discardable_bitmap;
  non_discardable_bitmap.allocN32Pixels(128, 128);
  non_discardable_bitmap.setImmutable();
  skia::RefPtr<SkImage> non_discardable_image =
      skia::AdoptRef(SkImage::NewFromBitmap(non_discardable_bitmap));

  recording_source->add_draw_rect_with_paint(gfx::Rect(0, 0, 256, 256),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(128, 128, 512, 512),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(512, 0, 256, 256),
                                             simple_paint);
  recording_source->add_draw_rect_with_paint(gfx::Rect(0, 512, 256, 256),
                                             simple_paint);
  recording_source->add_draw_image(non_discardable_image.get(),
                                   gfx::Point(128, 0));
  recording_source->add_draw_image(non_discardable_image.get(),
                                   gfx::Point(0, 128));
  recording_source->add_draw_image(non_discardable_image.get(),
                                   gfx::Point(150, 150));
  recording_source->SetGatherDiscardableImages(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource<TypeParam>(recording_source.get());

  // Tile sized iterators.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 128, 128), &images);
    EXPECT_TRUE(images.empty());
  }
  // Shifted tile sized iterators.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(140, 140, 128, 128),
                                           &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 256, 256), &images);
    EXPECT_TRUE(images.empty());
  }
}

TYPED_TEST(RecordingSourceTest, DiscardableImages) {
  gfx::Size grid_cell_size(128, 128);
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  scoped_ptr<TypeParam> recording_source =
      CreateRecordingSource<TypeParam>(recorded_viewport, grid_cell_size);

  skia::RefPtr<SkImage> discardable_image[2][2];
  discardable_image[0][0] = CreateDiscardableImage(gfx::Size(32, 32));
  discardable_image[1][0] = CreateDiscardableImage(gfx::Size(32, 32));
  discardable_image[1][1] = CreateDiscardableImage(gfx::Size(32, 32));

  // Discardable images are found in the following cells:
  // |---|---|
  // | x |   |
  // |---|---|
  // | x | x |
  // |---|---|
  recording_source->add_draw_image(discardable_image[0][0].get(),
                                   gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[1][0].get(),
                                   gfx::Point(0, 130));
  recording_source->add_draw_image(discardable_image[1][1].get(),
                                   gfx::Point(140, 140));
  recording_source->SetGatherDiscardableImages(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource<TypeParam>(recording_source.get());

  // Tile sized iterators. These should find only one image.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 128, 128), &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image == discardable_image[0][0].get());
    EXPECT_EQ(gfx::RectF(32, 32).ToString(),
              gfx::SkRectToRectF(images[0].image_rect).ToString());
  }

  // Shifted tile sized iterators. These should find only one image.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(140, 140, 128, 128),
                                           &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image == discardable_image[1][1].get());
    EXPECT_EQ(gfx::RectF(140, 140, 32, 32).ToString(),
              gfx::SkRectToRectF(images[0].image_rect).ToString());
  }

  // Ensure there's no discardable images in the empty cell
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(140, 0, 128, 128),
                                           &images);
    EXPECT_TRUE(images.empty());
  }

  // Layer sized iterators. These should find all 3 images.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 256, 256), &images);
    EXPECT_EQ(3u, images.size());
    EXPECT_TRUE(images[0].image == discardable_image[0][0].get());
    EXPECT_TRUE(images[1].image == discardable_image[1][0].get());
    EXPECT_TRUE(images[2].image == discardable_image[1][1].get());
    EXPECT_EQ(gfx::RectF(32, 32).ToString(),
              gfx::SkRectToRectF(images[0].image_rect).ToString());
    EXPECT_EQ(gfx::RectF(0, 130, 32, 32).ToString(),
              gfx::SkRectToRectF(images[1].image_rect).ToString());
    EXPECT_EQ(gfx::RectF(140, 140, 32, 32).ToString(),
              gfx::SkRectToRectF(images[2].image_rect).ToString());
  }
}

TYPED_TEST(RecordingSourceTest, DiscardableImagesBaseNonDiscardable) {
  gfx::Size grid_cell_size(256, 256);
  gfx::Rect recorded_viewport(0, 0, 512, 512);

  scoped_ptr<TypeParam> recording_source =
      CreateRecordingSource<TypeParam>(recorded_viewport, grid_cell_size);

  SkBitmap non_discardable_bitmap;
  non_discardable_bitmap.allocN32Pixels(512, 512);
  non_discardable_bitmap.setImmutable();
  skia::RefPtr<SkImage> non_discardable_image =
      skia::AdoptRef(SkImage::NewFromBitmap(non_discardable_bitmap));

  skia::RefPtr<SkImage> discardable_image[2][2];
  discardable_image[0][0] = CreateDiscardableImage(gfx::Size(128, 128));
  discardable_image[0][1] = CreateDiscardableImage(gfx::Size(128, 128));
  discardable_image[1][1] = CreateDiscardableImage(gfx::Size(128, 128));

  // One large non-discardable image covers the whole grid.
  // Discardable images are found in the following cells:
  // |---|---|
  // | x | x |
  // |---|---|
  // |   | x |
  // |---|---|
  recording_source->add_draw_image(non_discardable_image.get(),
                                   gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[0][0].get(),
                                   gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[0][1].get(),
                                   gfx::Point(260, 0));
  recording_source->add_draw_image(discardable_image[1][1].get(),
                                   gfx::Point(260, 260));
  recording_source->SetGatherDiscardableImages(true);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      CreateRasterSource<TypeParam>(recording_source.get());

  // Tile sized iterators. These should find only one image.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 256, 256), &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image == discardable_image[0][0].get());
    EXPECT_EQ(gfx::RectF(128, 128).ToString(),
              gfx::SkRectToRectF(images[0].image_rect).ToString());
  }
  // Shifted tile sized iterators. These should find only one image.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(260, 260, 256, 256),
                                           &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0].image == discardable_image[1][1].get());
    EXPECT_EQ(gfx::RectF(260, 260, 128, 128).ToString(),
              gfx::SkRectToRectF(images[0].image_rect).ToString());
  }
  // Ensure there's no discardable images in the empty cell
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 256, 256, 256),
                                           &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators. These should find three images.
  {
    std::vector<skia::PositionImage> images;
    raster_source->GatherDiscardableImages(gfx::Rect(0, 0, 512, 512), &images);
    EXPECT_EQ(3u, images.size());
    EXPECT_TRUE(images[0].image == discardable_image[0][0].get());
    EXPECT_TRUE(images[1].image == discardable_image[0][1].get());
    EXPECT_TRUE(images[2].image == discardable_image[1][1].get());
    EXPECT_EQ(gfx::RectF(128, 128).ToString(),
              gfx::SkRectToRectF(images[0].image_rect).ToString());
    EXPECT_EQ(gfx::RectF(260, 0, 128, 128).ToString(),
              gfx::SkRectToRectF(images[1].image_rect).ToString());
    EXPECT_EQ(gfx::RectF(260, 260, 128, 128).ToString(),
              gfx::SkRectToRectF(images[2].image_rect).ToString());
  }
}

}  // namespace
}  // namespace cc
