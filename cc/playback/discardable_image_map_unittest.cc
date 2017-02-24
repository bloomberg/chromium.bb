// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/discardable_image_map.h"

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "cc/base/region.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

struct PositionScaleDrawImage {
  PositionScaleDrawImage(sk_sp<const SkImage> image,
                         const gfx::Rect& image_rect,
                         const SkSize& scale)
      : image(std::move(image)), image_rect(image_rect), scale(scale) {}
  sk_sp<const SkImage> image;
  gfx::Rect image_rect;
  SkSize scale;
};

}  // namespace

class DiscardableImageMapTest : public testing::Test {
 public:
  std::vector<PositionScaleDrawImage> GetDiscardableImagesInRect(
      const DiscardableImageMap& image_map,
      const gfx::Rect& rect) {
    std::vector<DrawImage> draw_images;
    image_map.GetDiscardableImagesInRect(rect, 1.f, &draw_images);

    std::vector<size_t> indices;
    image_map.images_rtree_.Search(rect, &indices);
    std::vector<PositionScaleDrawImage> position_draw_images;
    for (size_t index : indices) {
      position_draw_images.push_back(
          PositionScaleDrawImage(image_map.all_images_[index].first.image(),
                                 image_map.all_images_[index].second,
                                 image_map.all_images_[index].first.scale()));
    }

    EXPECT_EQ(draw_images.size(), position_draw_images.size());
    for (size_t i = 0; i < draw_images.size(); ++i)
      EXPECT_TRUE(draw_images[i].image() == position_draw_images[i].image);
    return position_draw_images;
  }
};

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectTest) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

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
  sk_sp<SkImage> discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] = CreateDiscardableImage(gfx::Size(500, 500));
        PaintFlags flags;
        content_layer_client.add_draw_image(
            discardable_image[y][x], gfx::Point(x * 512 + 6, y * 512 + 6),
            flags);
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    display_list->Raster(generator.canvas(), nullptr, gfx::Rect(), 1.f);
  }

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          image_map, gfx::Rect(x * 512, y * 512, 500, 500));
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image == discardable_image[y][x]) << x << " "
                                                                << y;
        EXPECT_EQ(gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500),
                  images[0].image_rect);
        EXPECT_EQ(images[0].image_rect,
                  image_map.GetRectForImage(images[0].image->uniqueID()));
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }

  // Capture 4 pixel refs.
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(512, 512, 2048, 2048));
  EXPECT_EQ(4u, images.size());

  EXPECT_TRUE(images[0].image == discardable_image[1][2]);
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 512 + 6, 500, 500), images[0].image_rect);
  EXPECT_EQ(images[0].image_rect,
            image_map.GetRectForImage(images[0].image->uniqueID()));

  EXPECT_TRUE(images[1].image == discardable_image[2][1]);
  EXPECT_EQ(gfx::Rect(512 + 6, 2 * 512 + 6, 500, 500), images[1].image_rect);
  EXPECT_EQ(images[1].image_rect,
            image_map.GetRectForImage(images[1].image->uniqueID()));

  EXPECT_TRUE(images[2].image == discardable_image[2][3]);
  EXPECT_EQ(gfx::Rect(3 * 512 + 6, 2 * 512 + 6, 500, 500),
            images[2].image_rect);
  EXPECT_EQ(images[2].image_rect,
            image_map.GetRectForImage(images[2].image->uniqueID()));

  EXPECT_TRUE(images[3].image == discardable_image[3][2]);
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 3 * 512 + 6, 500, 500),
            images[3].image_rect);
  EXPECT_EQ(images[3].image_rect,
            image_map.GetRectForImage(images[3].image->uniqueID()));
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectNonZeroLayer) {
  gfx::Rect visible_rect(1024, 0, 2048, 2048);
  // Make sure visible rect fits into the layer size.
  gfx::Size layer_size(visible_rect.right(), visible_rect.bottom());
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(layer_size);

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
  sk_sp<SkImage> discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] = CreateDiscardableImage(gfx::Size(500, 500));
        PaintFlags flags;
        content_layer_client.add_draw_image(
            discardable_image[y][x],
            gfx::Point(1024 + x * 512 + 6, y * 512 + 6), flags);
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           layer_size);
    display_list->Raster(generator.canvas(), nullptr, gfx::Rect(), 1.f);
  }

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          image_map, gfx::Rect(1024 + x * 512, y * 512, 500, 500));
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image == discardable_image[y][x]) << x << " "
                                                                << y;
        EXPECT_EQ(gfx::Rect(1024 + x * 512 + 6, y * 512 + 6, 500, 500),
                  images[0].image_rect);
        EXPECT_EQ(images[0].image_rect,
                  image_map.GetRectForImage(images[0].image->uniqueID()));
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
        image_map, gfx::Rect(1024 + 512, 512, 2048, 2048));
    EXPECT_EQ(4u, images.size());

    EXPECT_TRUE(images[0].image == discardable_image[1][2]);
    EXPECT_EQ(gfx::Rect(1024 + 2 * 512 + 6, 512 + 6, 500, 500),
              images[0].image_rect);
    EXPECT_EQ(images[0].image_rect,
              image_map.GetRectForImage(images[0].image->uniqueID()));

    EXPECT_TRUE(images[1].image == discardable_image[2][1]);
    EXPECT_EQ(gfx::Rect(1024 + 512 + 6, 2 * 512 + 6, 500, 500),
              images[1].image_rect);
    EXPECT_EQ(images[1].image_rect,
              image_map.GetRectForImage(images[1].image->uniqueID()));

    EXPECT_TRUE(images[2].image == discardable_image[2][3]);
    EXPECT_EQ(gfx::Rect(1024 + 3 * 512 + 6, 2 * 512 + 6, 500, 500),
              images[2].image_rect);
    EXPECT_EQ(images[2].image_rect,
              image_map.GetRectForImage(images[2].image->uniqueID()));

    EXPECT_TRUE(images[3].image == discardable_image[3][2]);
    EXPECT_EQ(gfx::Rect(1024 + 2 * 512 + 6, 3 * 512 + 6, 500, 500),
              images[3].image_rect);
    EXPECT_EQ(images[3].image_rect,
              image_map.GetRectForImage(images[3].image->uniqueID()));
  }

  // Non intersecting rects
  {
    std::vector<PositionScaleDrawImage> images =
        GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }
  {
    std::vector<PositionScaleDrawImage> images =
        GetDiscardableImagesInRect(image_map, gfx::Rect(3500, 0, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }
  {
    std::vector<PositionScaleDrawImage> images =
        GetDiscardableImagesInRect(image_map, gfx::Rect(0, 1100, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }
  {
    std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
        image_map, gfx::Rect(3500, 1100, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }

  // Image not present in the list.
  {
    sk_sp<SkImage> image = CreateDiscardableImage(gfx::Size(500, 500));
    EXPECT_EQ(gfx::Rect(), image_map.GetRectForImage(image->uniqueID()));
  }
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectOnePixelQuery) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

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
  sk_sp<SkImage> discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] = CreateDiscardableImage(gfx::Size(500, 500));
        PaintFlags flags;
        content_layer_client.add_draw_image(
            discardable_image[y][x], gfx::Point(x * 512 + 6, y * 512 + 6),
            flags);
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    display_list->Raster(generator.canvas(), nullptr, gfx::Rect(), 1.f);
  }

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          image_map, gfx::Rect(x * 512 + 256, y * 512 + 256, 1, 1));
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image == discardable_image[y][x]) << x << " "
                                                                << y;
        EXPECT_EQ(gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500),
                  images[0].image_rect);
        EXPECT_EQ(images[0].image_rect,
                  image_map.GetRectForImage(images[0].image->uniqueID()));
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectMassiveImage) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  sk_sp<SkImage> discardable_image =
      CreateDiscardableImage(gfx::Size(1 << 25, 1 << 25));
  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image, gfx::Point(0, 0),
                                      flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    display_list->Raster(generator.canvas(), nullptr, gfx::Rect(), 1.f);
  }
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image);
  EXPECT_EQ(gfx::Rect(0, 0, 2048, 2048), images[0].image_rect);
  EXPECT_EQ(images[0].image_rect,
            image_map.GetRectForImage(images[0].image->uniqueID()));
}

TEST_F(DiscardableImageMapTest, PaintDestroyedWhileImageIsDrawn) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  sk_sp<SkImage> discardable_image = CreateDiscardableImage(gfx::Size(10, 10));

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    {
      std::unique_ptr<SkPaint> paint(new SkPaint());
      generator.canvas()->saveLayer(gfx::RectToSkRect(visible_rect),
                                    paint.get());
    }
    generator.canvas()->drawImage(discardable_image, 0, 0, nullptr);
    generator.canvas()->restore();
  }

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image);
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectMaxImage) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  int dimension = std::numeric_limits<int>::max();
  sk_sp<SkImage> discardable_image =
      CreateDiscardableImage(gfx::Size(dimension, dimension));
  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image, gfx::Point(42, 42),
                                      flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    display_list->Raster(generator.canvas(), nullptr, visible_rect, 1.f);
  }
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(42, 42, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image);
  EXPECT_EQ(gfx::Rect(42, 42, 2006, 2006), images[0].image_rect);
  EXPECT_EQ(images[0].image_rect,
            image_map.GetRectForImage(images[0].image->uniqueID()));
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectMaxImageMaxLayer) {
  // At large values of integer x, x != static_cast<int>(static_cast<float>(x)).
  // So, make sure the dimension can be converted back and forth for the
  // purposes of the unittest. Also, at near max int values, Skia seems to skip
  // some draw calls, so we subtract 64 since we only care about "really large"
  // values, not necessarily max int values.
  int dimension = static_cast<int>(
      static_cast<float>(std::numeric_limits<int>::max() - 64));
  gfx::Rect visible_rect(dimension, dimension);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  sk_sp<SkImage> discardable_image =
      CreateDiscardableImage(gfx::Size(dimension, dimension));
  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image, gfx::Point(0, 0),
                                      flags);
  content_layer_client.add_draw_image(discardable_image, gfx::Point(10000, 0),
                                      flags);
  content_layer_client.add_draw_image(discardable_image,
                                      gfx::Point(-10000, 500), flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    display_list->Raster(generator.canvas(), nullptr, visible_rect, 1.f);
  }
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), images[0].image_rect);

  images = GetDiscardableImagesInRect(image_map, gfx::Rect(10000, 0, 1, 1));
  EXPECT_EQ(2u, images.size());
  EXPECT_EQ(gfx::Rect(10000, 0, dimension - 10000, dimension),
            images[1].image_rect);
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), images[0].image_rect);

  // Since we adjust negative offsets before using ToEnclosingRect, the expected
  // width will be converted to float, which means that we lose some precision.
  // The expected value is whatever the value is converted to float and then
  // back to int.
  int expected10k = static_cast<int>(static_cast<float>(dimension - 10000));
  images = GetDiscardableImagesInRect(image_map, gfx::Rect(0, 500, 1, 1));
  EXPECT_EQ(2u, images.size());
  EXPECT_EQ(gfx::Rect(0, 500, expected10k, dimension - 500),
            images[1].image_rect);
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), images[0].image_rect);

  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension),
            image_map.GetRectForImage(discardable_image->uniqueID()));
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesRectInBounds) {
  gfx::Rect visible_rect(1000, 1000);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  sk_sp<SkImage> discardable_image =
      CreateDiscardableImage(gfx::Size(100, 100));
  sk_sp<SkImage> long_discardable_image =
      CreateDiscardableImage(gfx::Size(10000, 100));

  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image, gfx::Point(-10, -11),
                                      flags);
  content_layer_client.add_draw_image(discardable_image, gfx::Point(950, 951),
                                      flags);
  content_layer_client.add_draw_image(long_discardable_image,
                                      gfx::Point(-100, 500), flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    display_list->Raster(generator.canvas(), nullptr, visible_rect, 1.f);
  }
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 0, 90, 89), images[0].image_rect);

  images = GetDiscardableImagesInRect(image_map, gfx::Rect(999, 999, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(950, 951, 50, 49), images[0].image_rect);

  images = GetDiscardableImagesInRect(image_map, gfx::Rect(0, 500, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 500, 1000, 100), images[0].image_rect);

  gfx::Rect discardable_image_rect;
  discardable_image_rect.Union(gfx::Rect(0, 0, 90, 89));
  discardable_image_rect.Union(gfx::Rect(950, 951, 50, 49));
  EXPECT_EQ(discardable_image_rect,
            image_map.GetRectForImage(discardable_image->uniqueID()));

  EXPECT_EQ(gfx::Rect(0, 500, 1000, 100),
            image_map.GetRectForImage(long_discardable_image->uniqueID()));
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInShader) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

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
  sk_sp<SkImage> discardable_image[4][4];

  // Skia doesn't allow shader instantiation with non-invertible local
  // transforms, so we can't let the scale drop all the way to 0.
  static constexpr float kMinScale = 0.1f;

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] = CreateDiscardableImage(gfx::Size(500, 500));
        SkMatrix scale = SkMatrix::MakeScale(std::max(x * 0.5f, kMinScale),
                                             std::max(y * 0.5f, kMinScale));
        PaintFlags flags;
        flags.setShader(discardable_image[y][x]->makeShader(
            SkShader::kClamp_TileMode, SkShader::kClamp_TileMode, &scale));
        content_layer_client.add_draw_rect(
            gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500), flags);
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);

  DiscardableImageMap image_map;
  {
    DiscardableImageMap::ScopedMetadataGenerator generator(&image_map,
                                                           visible_rect.size());
    display_list->Raster(generator.canvas(), nullptr, gfx::Rect(), 1.f);
  }

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          image_map, gfx::Rect(x * 512, y * 512, 500, 500));
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image == discardable_image[y][x]) << x << " "
                                                                << y;
        EXPECT_EQ(gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500),
                  images[0].image_rect);
        EXPECT_EQ(std::max(x * 0.5f, kMinScale), images[0].scale.fWidth);
        EXPECT_EQ(std::max(y * 0.5f, kMinScale), images[0].scale.fHeight);
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }

  // Capture 4 pixel refs.
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(512, 512, 2048, 2048));
  EXPECT_EQ(4u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image[1][2]);
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 512 + 6, 500, 500), images[0].image_rect);
  EXPECT_TRUE(images[1].image == discardable_image[2][1]);
  EXPECT_EQ(gfx::Rect(512 + 6, 2 * 512 + 6, 500, 500), images[1].image_rect);
  EXPECT_TRUE(images[2].image == discardable_image[2][3]);
  EXPECT_EQ(gfx::Rect(3 * 512 + 6, 2 * 512 + 6, 500, 500),
            images[2].image_rect);
  EXPECT_TRUE(images[3].image == discardable_image[3][2]);
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 3 * 512 + 6, 500, 500),
            images[3].image_rect);
}

}  // namespace cc
