// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/discardable_image_map.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/base/region.h"
#include "cc/playback/raster_source.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_display_list_recording_source.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

class TestImageGenerator : public SkImageGenerator {
 public:
  explicit TestImageGenerator(const SkImageInfo& info)
      : SkImageGenerator(info) {}
};

skia::RefPtr<SkImage> CreateDiscardableImage(const gfx::Size& size) {
  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(size.width(), size.height());
  return skia::AdoptRef(
      SkImage::NewFromGenerator(new TestImageGenerator(info)));
}

TEST(DiscardableImageMapTest, DiscardableImageMapIterator) {
  gfx::Rect visible_rect(2048, 2048);

  gfx::Size tile_grid_size(512, 512);

  FakeContentLayerClient content_layer_client;

  // Discardable pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  skia::RefPtr<SkImage> discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] = CreateDiscardableImage(gfx::Size(500, 500));
        SkPaint paint;
        content_layer_client.add_draw_image(
            discardable_image[y][x].get(), gfx::Point(x * 512 + 6, y * 512 + 6),
            paint);
      }
    }
  }

  FakeDisplayListRecordingSource recording_source(tile_grid_size);
  Region invalidation(visible_rect);
  recording_source.SetGatherDiscardableImages(true);
  recording_source.UpdateAndExpandInvalidation(
      &content_layer_client, &invalidation, visible_rect.size(), visible_rect,
      1, RecordingSource::RECORD_NORMALLY);
  DisplayItemList* display_list = recording_source.display_list();

  // Default iterator does not have any pixel refs.
  {
    DiscardableImageMap::Iterator iterator;
    EXPECT_FALSE(iterator);
  }

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      DiscardableImageMap::Iterator iterator(
          gfx::Rect(x * 512, y * 512, 500, 500), display_list);
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(iterator->image == discardable_image[y][x].get())
            << x << " " << y;
        EXPECT_EQ(gfx::RectF(x * 512 + 6, y * 512 + 6, 500, 500).ToString(),
                  gfx::SkRectToRectF(iterator->image_rect).ToString());
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }

  // Capture 4 pixel refs.
  {
    DiscardableImageMap::Iterator iterator(gfx::Rect(512, 512, 2048, 2048),
                                           display_list);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(iterator->image == discardable_image[1][2].get());
    EXPECT_EQ(gfx::RectF(2 * 512 + 6, 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][1].get());
    EXPECT_EQ(gfx::RectF(512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][3].get());
    EXPECT_EQ(gfx::RectF(3 * 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[3][2].get());
    EXPECT_EQ(gfx::RectF(2 * 512 + 6, 3 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_FALSE(++iterator);
  }

  {
    // Copy test.
    DiscardableImageMap::Iterator iterator(gfx::Rect(512, 512, 2048, 2048),
                                           display_list);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(iterator->image == discardable_image[1][2].get());
    EXPECT_EQ(gfx::RectF(2 * 512 + 6, 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][1].get());
    EXPECT_EQ(gfx::RectF(512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());

    // copy now points to the same spot as iterator,
    // but both can be incremented independently.
    DiscardableImageMap::Iterator copy = iterator;
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][3].get());
    EXPECT_EQ(gfx::RectF(3 * 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[3][2].get());
    EXPECT_EQ(gfx::RectF(2 * 512 + 6, 3 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_FALSE(++iterator);

    EXPECT_TRUE(copy);
    EXPECT_TRUE(copy->image == discardable_image[2][1].get());
    EXPECT_EQ(gfx::RectF(512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(copy->image_rect).ToString());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(copy->image == discardable_image[2][3].get());
    EXPECT_EQ(gfx::RectF(3 * 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(copy->image_rect).ToString());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(copy->image == discardable_image[3][2].get());
    EXPECT_EQ(gfx::RectF(2 * 512 + 6, 3 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(copy->image_rect).ToString());
    EXPECT_FALSE(++copy);
  }
}

TEST(DiscardableImageMapTest, DiscardableImageMapIteratorNonZeroLayer) {
  gfx::Rect visible_rect(1024, 0, 2048, 2048);
  // Make sure visible rect fits into the layer size.
  gfx::Size layer_size(visible_rect.right(), visible_rect.bottom());

  gfx::Size tile_grid_size(512, 512);

  FakeContentLayerClient content_layer_client;

  // Discardable pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  skia::RefPtr<SkImage> discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] = CreateDiscardableImage(gfx::Size(500, 500));
        SkPaint paint;
        content_layer_client.add_draw_image(
            discardable_image[y][x].get(),
            gfx::Point(1024 + x * 512 + 6, y * 512 + 6), paint);
      }
    }
  }

  FakeDisplayListRecordingSource recording_source(tile_grid_size);
  Region invalidation(visible_rect);
  recording_source.set_pixel_record_distance(0);
  recording_source.SetGatherDiscardableImages(true);
  recording_source.UpdateAndExpandInvalidation(
      &content_layer_client, &invalidation, layer_size, visible_rect, 1,
      RecordingSource::RECORD_NORMALLY);
  DisplayItemList* display_list = recording_source.display_list();

  // Default iterator does not have any pixel refs.
  {
    DiscardableImageMap::Iterator iterator;
    EXPECT_FALSE(iterator);
  }

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      DiscardableImageMap::Iterator iterator(
          gfx::Rect(1024 + x * 512, y * 512, 500, 500), display_list);
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(iterator->image == discardable_image[y][x].get());
        EXPECT_EQ(
            gfx::RectF(1024 + x * 512 + 6, y * 512 + 6, 500, 500).ToString(),
            gfx::SkRectToRectF(iterator->image_rect).ToString());
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    DiscardableImageMap::Iterator iterator(
        gfx::Rect(1024 + 512, 512, 2048, 2048), display_list);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(iterator->image == discardable_image[1][2].get());
    EXPECT_EQ(gfx::RectF(1024 + 2 * 512 + 6, 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][1].get());
    EXPECT_EQ(gfx::RectF(1024 + 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][3].get());
    EXPECT_EQ(gfx::RectF(1024 + 3 * 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[3][2].get());
    EXPECT_EQ(gfx::RectF(1024 + 2 * 512 + 6, 3 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_FALSE(++iterator);
  }

  // Copy test.
  {
    DiscardableImageMap::Iterator iterator(
        gfx::Rect(1024 + 512, 512, 2048, 2048), display_list);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(iterator->image == discardable_image[1][2].get());
    EXPECT_EQ(gfx::RectF(1024 + 2 * 512 + 6, 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][1].get());
    EXPECT_EQ(gfx::RectF(1024 + 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());

    // copy now points to the same spot as iterator,
    // but both can be incremented independently.
    DiscardableImageMap::Iterator copy = iterator;
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[2][3].get());
    EXPECT_EQ(gfx::RectF(1024 + 3 * 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(iterator->image == discardable_image[3][2].get());
    EXPECT_EQ(gfx::RectF(1024 + 2 * 512 + 6, 3 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(iterator->image_rect).ToString());
    EXPECT_FALSE(++iterator);

    EXPECT_TRUE(copy);
    EXPECT_TRUE(copy->image == discardable_image[2][1].get());
    EXPECT_EQ(gfx::RectF(1024 + 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(copy->image_rect).ToString());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(copy->image == discardable_image[2][3].get());
    EXPECT_EQ(gfx::RectF(1024 + 3 * 512 + 6, 2 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(copy->image_rect).ToString());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(copy->image == discardable_image[3][2].get());
    EXPECT_EQ(gfx::RectF(1024 + 2 * 512 + 6, 3 * 512 + 6, 500, 500).ToString(),
              gfx::SkRectToRectF(copy->image_rect).ToString());
    EXPECT_FALSE(++copy);
  }

  // Non intersecting rects
  {
    DiscardableImageMap::Iterator iterator(gfx::Rect(0, 0, 1000, 1000),
                                           display_list);
    EXPECT_FALSE(iterator);
  }
  {
    DiscardableImageMap::Iterator iterator(gfx::Rect(3500, 0, 1000, 1000),
                                           display_list);
    EXPECT_FALSE(iterator);
  }
  {
    DiscardableImageMap::Iterator iterator(gfx::Rect(0, 1100, 1000, 1000),
                                           display_list);
    EXPECT_FALSE(iterator);
  }
  {
    DiscardableImageMap::Iterator iterator(gfx::Rect(3500, 1100, 1000, 1000),
                                           display_list);
    EXPECT_FALSE(iterator);
  }
}

TEST(DiscardableImageMapTest, DiscardableImageMapIteratorOnePixelQuery) {
  gfx::Rect visible_rect(2048, 2048);

  gfx::Size tile_grid_size(512, 512);

  FakeContentLayerClient content_layer_client;

  // Discardable pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  skia::RefPtr<SkImage> discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] = CreateDiscardableImage(gfx::Size(500, 500));
        SkPaint paint;
        content_layer_client.add_draw_image(
            discardable_image[y][x].get(), gfx::Point(x * 512 + 6, y * 512 + 6),
            paint);
      }
    }
  }

  FakeDisplayListRecordingSource recording_source(tile_grid_size);
  Region invalidation(visible_rect);
  recording_source.SetGatherDiscardableImages(true);
  recording_source.UpdateAndExpandInvalidation(
      &content_layer_client, &invalidation, visible_rect.size(), visible_rect,
      1, RecordingSource::RECORD_NORMALLY);
  DisplayItemList* display_list = recording_source.display_list();

  // Default iterator does not have any pixel refs.
  {
    DiscardableImageMap::Iterator iterator;
    EXPECT_FALSE(iterator);
  }

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      DiscardableImageMap::Iterator iterator(
          gfx::Rect(x * 512, y * 512 + 256, 1, 1), display_list);
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(iterator->image == discardable_image[y][x].get());
        EXPECT_EQ(gfx::RectF(x * 512 + 6, y * 512 + 6, 500, 500).ToString(),
                  gfx::SkRectToRectF(iterator->image_rect).ToString());
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
}

TEST(DiscardableImageMapTest, DiscardableImageMapIteratorMassiveImage) {
  gfx::Rect visible_rect(2048, 2048);
  gfx::Size tile_grid_size(512, 512);
  FakeContentLayerClient content_layer_client;

  skia::RefPtr<SkImage> discardable_image;
  discardable_image = CreateDiscardableImage(gfx::Size(1 << 25, 1 << 25));
  SkPaint paint;
  content_layer_client.add_draw_image(discardable_image.get(), gfx::Point(0, 0),
                                      paint);

  FakeDisplayListRecordingSource recording_source(tile_grid_size);
  Region invalidation(visible_rect);
  recording_source.SetGatherDiscardableImages(true);
  recording_source.UpdateAndExpandInvalidation(
      &content_layer_client, &invalidation, visible_rect.size(), visible_rect,
      1, RecordingSource::RECORD_NORMALLY);
  DisplayItemList* display_list = recording_source.display_list();

  DiscardableImageMap::Iterator iterator(gfx::Rect(0, 0, 1, 1), display_list);
  EXPECT_TRUE(iterator);
  EXPECT_TRUE(iterator->image == discardable_image.get());
  EXPECT_EQ(gfx::RectF(0, 0, 1 << 25, 1 << 25).ToString(),
            gfx::SkRectToRectF(iterator->image_rect).ToString());
  EXPECT_FALSE(++iterator);
}

}  // namespace
}  // namespace cc
