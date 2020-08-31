// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/discardable_image_map.h"

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "cc/base/region.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_recorder.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {
using Rects = base::StackVector<gfx::Rect, 1>;

struct PositionScaleDrawImage {
  PositionScaleDrawImage(const PaintImage& image,
                         const gfx::Rect& image_rect,
                         const SkSize& scale)
      : image(image), image_rect(image_rect), scale(scale) {}
  PaintImage image;
  gfx::Rect image_rect;
  SkSize scale;
};

sk_sp<PaintOpBuffer> CreateRecording(const PaintImage& discardable_image,
                                     const gfx::Rect& visible_rect) {
  auto buffer = sk_make_sp<PaintOpBuffer>();
  buffer->push<DrawImageOp>(discardable_image, 0.f, 0.f, nullptr);
  return buffer;
}

}  // namespace

class DiscardableImageMapTest : public testing::Test {
 public:
  std::vector<PositionScaleDrawImage> GetDiscardableImagesInRect(
      const DiscardableImageMap& image_map,
      const gfx::Rect& rect) {
    std::vector<const DrawImage*> draw_image_ptrs;
    // Choose a not-SRGB-and-not-invalid target color space to verify that it
    // is passed correctly to the resulting DrawImages.
    gfx::ColorSpace target_color_space = gfx::ColorSpace::CreateXYZD50();
    image_map.GetDiscardableImagesInRect(rect, &draw_image_ptrs);
    std::vector<DrawImage> draw_images;
    for (const auto* image : draw_image_ptrs)
      draw_images.push_back(DrawImage(
          *image, 1.f, PaintImage::kDefaultFrameIndex, target_color_space));

    std::vector<PositionScaleDrawImage> position_draw_images;
    std::vector<DrawImage> results;
    image_map.images_rtree_.Search(rect, &results);
    for (DrawImage& image : results) {
      auto image_id = image.paint_image().stable_id();
      position_draw_images.push_back(PositionScaleDrawImage(
          image.paint_image(),
          ImageRectsToRegion(image_map.GetRectsForImage(image_id)).bounds(),
          image.scale()));
    }

    EXPECT_EQ(draw_images.size(), position_draw_images.size());
    for (size_t i = 0; i < draw_images.size(); ++i) {
      EXPECT_TRUE(draw_images[i].paint_image() ==
                  position_draw_images[i].image);
      EXPECT_EQ(draw_images[i].target_color_space(), target_color_space);
    }
    return position_draw_images;
  }

  // Note that the image rtree outsets the images by 1, see the comment in
  // DiscardableImagesMetadataCanvas::AddImage.
  std::vector<gfx::Rect> InsetImageRects(
      const std::vector<PositionScaleDrawImage>& images) {
    std::vector<gfx::Rect> result;
    for (auto& image : images) {
      result.push_back(image.image_rect);
      result.back().Inset(1, 1, 1, 1);
    }
    return result;
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
  PaintImage discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            CreateDiscardablePaintImage(gfx::Size(500, 500));
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
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          image_map, gfx::Rect(x * 512, y * 512, 500, 500));
      std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image == discardable_image[y][x])
            << x << " " << y;
        EXPECT_EQ(gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500),
                  inset_rects[0]);
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }

  // Capture 4 pixel refs.
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(512, 512, 2048, 2048));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(4u, images.size());

  EXPECT_TRUE(images[0].image == discardable_image[1][2]);
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 512 + 6, 500, 500), inset_rects[0]);

  EXPECT_TRUE(images[1].image == discardable_image[2][1]);
  EXPECT_EQ(gfx::Rect(512 + 6, 2 * 512 + 6, 500, 500), inset_rects[1]);

  EXPECT_TRUE(images[2].image == discardable_image[2][3]);
  EXPECT_EQ(gfx::Rect(3 * 512 + 6, 2 * 512 + 6, 500, 500), inset_rects[2]);

  EXPECT_TRUE(images[3].image == discardable_image[3][2]);
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 3 * 512 + 6, 500, 500), inset_rects[3]);
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
  PaintImage discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            CreateDiscardablePaintImage(gfx::Size(500, 500));
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
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          image_map, gfx::Rect(1024 + x * 512, y * 512, 500, 500));
      std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image == discardable_image[y][x])
            << x << " " << y;
        EXPECT_EQ(gfx::Rect(1024 + x * 512 + 6, y * 512 + 6, 500, 500),
                  inset_rects[0]);
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
        image_map, gfx::Rect(1024 + 512, 512, 2048, 2048));
    std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
    EXPECT_EQ(4u, images.size());

    EXPECT_TRUE(images[0].image == discardable_image[1][2]);
    EXPECT_EQ(gfx::Rect(1024 + 2 * 512 + 6, 512 + 6, 500, 500), inset_rects[0]);

    EXPECT_TRUE(images[1].image == discardable_image[2][1]);
    EXPECT_EQ(gfx::Rect(1024 + 512 + 6, 2 * 512 + 6, 500, 500), inset_rects[1]);

    EXPECT_TRUE(images[2].image == discardable_image[2][3]);
    EXPECT_EQ(gfx::Rect(1024 + 3 * 512 + 6, 2 * 512 + 6, 500, 500),
              inset_rects[2]);

    EXPECT_TRUE(images[3].image == discardable_image[3][2]);
    EXPECT_EQ(gfx::Rect(1024 + 2 * 512 + 6, 3 * 512 + 6, 500, 500),
              inset_rects[3]);
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
    PaintImage image = CreateDiscardablePaintImage(gfx::Size(500, 500));
    EXPECT_EQ(image_map.GetRectsForImage(image.stable_id())->size(), 0u);
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
  PaintImage discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            CreateDiscardablePaintImage(gfx::Size(500, 500));
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
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          image_map, gfx::Rect(x * 512 + 256, y * 512 + 256, 1, 1));
      std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image == discardable_image[y][x])
            << x << " " << y;
        EXPECT_EQ(gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500),
                  inset_rects[0]);
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

  PaintImage discardable_image =
      CreateDiscardablePaintImage(gfx::Size(1 << 25, 1 << 25), nullptr,
                                  false /* allocate_encoded_memory */);
  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image, gfx::Point(0, 0),
                                      flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image);
  EXPECT_EQ(gfx::Rect(0, 0, 2048, 2048), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, PaintDestroyedWhileImageIsDrawn) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  PaintImage discardable_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  sk_sp<PaintRecord> record = CreateRecording(discardable_image, visible_rect);

  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  PaintFlags paint;
  display_list->StartPaint();
  SkRect visible_sk_rect(gfx::RectToSkRect(visible_rect));
  display_list->push<SaveLayerOp>(&visible_sk_rect, &paint);
  display_list->push<DrawRecordOp>(std::move(record));
  display_list->push<RestoreOp>();
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image);
}

// Check if SkNoDrawCanvas does not crash for large layers.
TEST_F(DiscardableImageMapTest, RestoreSavedBigLayers) {
  PaintFlags flags;
  SkRect rect = SkRect::MakeWH(INT_MAX, INT_MAX);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  display_list->StartPaint();
  display_list->push<DrawRectOp>(rect, flags);
  display_list->EndPaintOfUnpaired(gfx::Rect(INT_MAX, INT_MAX));
  display_list->Finalize();
  display_list->GenerateDiscardableImagesMetadata();
}

// Test if SaveLayer and Restore work together.
// 1. Move cursor to (25, 25) draw a black rect of size 25x25.
// 2. save layer, move the cursor by (100, 100) or to point (125, 125), draw a
// red rect of size 25x25.
// 3. Restore layer, so the cursor moved back to (25, 25), move cursor by (100,
// 0) or at the point (125, 25), draw a yellow rect of size 25x25.
//  (25, 25)
//  +---+
//  |   |
//  +---+
//  (25, 125) (125, 125)
//  +---+     +---+
//  |   |     |   |
//  +---+     +---+
TEST_F(DiscardableImageMapTest, RestoreSavedTransformedLayers) {
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  PaintFlags paint;
  gfx::Rect visible_rect(200, 200);
  display_list->StartPaint();

  PaintImage discardable_image1 =
      CreateDiscardablePaintImage(gfx::Size(25, 25));
  PaintImage discardable_image2 =
      CreateDiscardablePaintImage(gfx::Size(25, 25));
  PaintImage discardable_image3 =
      CreateDiscardablePaintImage(gfx::Size(25, 25));
  display_list->push<TranslateOp>(25, 25);
  display_list->push<DrawImageOp>(discardable_image1, 0.f, 0.f, nullptr);
  display_list->push<SaveLayerOp>(nullptr, &paint);
  display_list->push<TranslateOp>(100, 100);
  display_list->push<DrawImageOp>(discardable_image2, 0.f, 0.f, nullptr);
  display_list->push<RestoreOp>();
  display_list->push<TranslateOp>(0, 100);
  display_list->push<DrawImageOp>(discardable_image3, 0.f, 0.f, nullptr);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ(3u, images.size());
  EXPECT_EQ(gfx::Rect(25, 25, 25, 25), InsetImageRects(images)[0]);
  EXPECT_EQ(gfx::Rect(125, 125, 25, 25), InsetImageRects(images)[1]);
  EXPECT_EQ(gfx::Rect(25, 125, 25, 25), InsetImageRects(images)[2]);
}

TEST_F(DiscardableImageMapTest, NullPaintOnSaveLayer) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  PaintImage discardable_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  sk_sp<PaintRecord> record = CreateRecording(discardable_image, visible_rect);

  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  display_list->StartPaint();
  SkRect visible_sk_rect(gfx::RectToSkRect(visible_rect));
  display_list->push<SaveLayerOp>(&visible_sk_rect, nullptr);
  display_list->push<DrawRecordOp>(std::move(record));
  display_list->push<RestoreOp>();
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();
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
  sk_sp<SkColorSpace> no_color_space;
  PaintImage discardable_image = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);
  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image, gfx::Point(42, 42),
                                      flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(42, 42, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image);
  EXPECT_EQ(gfx::Rect(42, 42, 2006, 2006), inset_rects[0]);
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

  sk_sp<SkColorSpace> no_color_space;
  PaintImage discardable_image1 = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);
  PaintImage discardable_image2 = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);
  PaintImage discardable_image3 = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);

  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image1, gfx::Point(0, 0),
                                      flags);
  content_layer_client.add_draw_image(discardable_image2, gfx::Point(10000, 0),
                                      flags);
  content_layer_client.add_draw_image(discardable_image3,
                                      gfx::Point(-10000, 500), flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), inset_rects[0]);

  images = GetDiscardableImagesInRect(image_map, gfx::Rect(10000, 0, 1, 1));
  inset_rects = InsetImageRects(images);
  EXPECT_EQ(2u, images.size());
  EXPECT_EQ(gfx::Rect(10000, 0, dimension - 10000, dimension), inset_rects[1]);
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), inset_rects[0]);

  // Since we adjust negative offsets before using ToEnclosingRect, the expected
  // width will be converted to float, which means that we lose some precision.
  // The expected value is whatever the value is converted to float and then
  // back to int.
  int expected10k = static_cast<int>(static_cast<float>(dimension - 10000));
  images = GetDiscardableImagesInRect(image_map, gfx::Rect(0, 500, 1, 1));
  inset_rects = InsetImageRects(images);
  EXPECT_EQ(2u, images.size());
  EXPECT_EQ(gfx::Rect(0, 500, expected10k, dimension - 500), inset_rects[1]);
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesRectInBounds) {
  gfx::Rect visible_rect(1000, 1000);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  PaintImage discardable_image1 =
      CreateDiscardablePaintImage(gfx::Size(100, 100));
  PaintImage discardable_image2 =
      CreateDiscardablePaintImage(gfx::Size(100, 100));
  PaintImage long_discardable_image =
      CreateDiscardablePaintImage(gfx::Size(10000, 100));

  PaintFlags flags;
  content_layer_client.add_draw_image(discardable_image1, gfx::Point(-10, -11),
                                      flags);
  content_layer_client.add_draw_image(discardable_image2, gfx::Point(950, 951),
                                      flags);
  content_layer_client.add_draw_image(long_discardable_image,
                                      gfx::Point(-100, 500), flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, gfx::Rect(0, 0, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);

  ASSERT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 0, 90, 89), inset_rects[0]);

  images = GetDiscardableImagesInRect(image_map, gfx::Rect(999, 999, 1, 1));
  inset_rects = InsetImageRects(images);
  ASSERT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(950, 951, 50, 49), inset_rects[0]);

  images = GetDiscardableImagesInRect(image_map, gfx::Rect(0, 500, 1, 1));
  inset_rects = InsetImageRects(images);
  ASSERT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 500, 1000, 100), inset_rects[0]);
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
  PaintImage discardable_image[4][4];

  // Skia doesn't allow shader instantiation with non-invertible local
  // transforms, so we can't let the scale drop all the way to 0.
  static constexpr float kMinScale = 0.1f;

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            PaintImageBuilder::WithDefault()
                .set_id(y * 4 + x)
                .set_paint_image_generator(
                    CreatePaintImageGenerator(gfx::Size(500, 500)))
                .TakePaintImage();
        SkMatrix scale = SkMatrix::MakeScale(std::max(x * 0.5f, kMinScale),
                                             std::max(y * 0.5f, kMinScale));
        PaintFlags flags;
        flags.setShader(PaintShader::MakeImage(discardable_image[y][x],
                                               SkTileMode::kClamp,
                                               SkTileMode::kClamp, &scale));
        content_layer_client.add_draw_rect(
            gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500), flags);
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<const DrawImage*> draw_images;
      image_map.GetDiscardableImagesInRect(
          gfx::Rect(x * 512, y * 512, 500, 500), &draw_images);
      if ((x + y) & 1) {
        EXPECT_EQ(1u, draw_images.size()) << x << " " << y;
        EXPECT_TRUE(draw_images[0]->paint_image() == discardable_image[y][x])
            << x << " " << y;
        EXPECT_EQ(std::max(x * 0.5f, kMinScale),
                  draw_images[0]->scale().fWidth);
        EXPECT_EQ(std::max(y * 0.5f, kMinScale),
                  draw_images[0]->scale().fHeight);
      } else {
        EXPECT_EQ(0u, draw_images.size()) << x << " " << y;
      }
    }
  }

  // Capture 4 pixel refs.
  std::vector<const DrawImage*> draw_images;
  image_map.GetDiscardableImagesInRect(gfx::Rect(512, 512, 2048, 2048),
                                       &draw_images);
  EXPECT_EQ(4u, draw_images.size());
  EXPECT_TRUE(draw_images[0]->paint_image() == discardable_image[1][2]);
  EXPECT_TRUE(draw_images[1]->paint_image() == discardable_image[2][1]);
  EXPECT_TRUE(draw_images[2]->paint_image() == discardable_image[2][3]);
  EXPECT_TRUE(draw_images[3]->paint_image() == discardable_image[3][2]);
}

TEST_F(DiscardableImageMapTest, ClipsImageRects) {
  gfx::Rect visible_rect(500, 500);

  PaintImage discardable_image =
      CreateDiscardablePaintImage(gfx::Size(500, 500));
  sk_sp<PaintRecord> record = CreateRecording(discardable_image, visible_rect);

  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;

  display_list->StartPaint();
  display_list->push<ClipRectOp>(gfx::RectToSkRect(gfx::Rect(250, 250)),
                                 SkClipOp::kIntersect, false);
  display_list->push<DrawRecordOp>(std::move(record));
  display_list->EndPaintOfUnpaired(gfx::Rect(250, 250));

  display_list->Finalize();

  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image == discardable_image);
  EXPECT_EQ(gfx::Rect(250, 250), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, GathersDiscardableImagesFromNestedOps) {
  // This |discardable_image| is in a PaintOpBuffer that gets added to
  // the root buffer.
  auto internal_record = sk_make_sp<PaintOpBuffer>();
  PaintImage discardable_image =
      CreateDiscardablePaintImage(gfx::Size(100, 100));
  internal_record->push<DrawImageOp>(discardable_image, 0.f, 0.f, nullptr);

  // This |discardable_image2| is in a DisplayItemList that gets added
  // to the root buffer.
  PaintImage discardable_image2 =
      CreateDiscardablePaintImage(gfx::Size(100, 100));

  scoped_refptr<DisplayItemList> display_list =
      new DisplayItemList(DisplayItemList::kToBeReleasedAsPaintOpBuffer);
  display_list->StartPaint();
  display_list->push<DrawImageOp>(discardable_image2, 100.f, 100.f, nullptr);
  display_list->EndPaintOfUnpaired(gfx::Rect(100, 100, 100, 100));
  display_list->Finalize();

  sk_sp<PaintRecord> record2 = display_list->ReleaseAsRecord();

  PaintOpBuffer root_buffer;
  root_buffer.push<DrawRecordOp>(internal_record);
  root_buffer.push<DrawRecordOp>(record2);
  DiscardableImageMap image_map;
  image_map.Generate(&root_buffer, gfx::Rect(200, 200));

  std::vector<const DrawImage*> images;
  image_map.GetDiscardableImagesInRect(gfx::Rect(0, 0, 5, 95), &images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(discardable_image == images[0]->paint_image());

  images.clear();
  image_map.GetDiscardableImagesInRect(gfx::Rect(105, 105, 5, 95), &images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(discardable_image2 == images[0]->paint_image());
}

TEST_F(DiscardableImageMapTest, GathersAnimatedImages) {
  gfx::Rect visible_rect(1000, 1000);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};

  gfx::Size image_size(100, 100);
  PaintImage static_image = CreateDiscardablePaintImage(image_size);
  PaintImage animated_loop_none =
      CreateAnimatedImage(image_size, frames, kAnimationNone);
  PaintImage animation_loop_infinite =
      CreateAnimatedImage(image_size, frames, 1u);

  PaintFlags flags;
  content_layer_client.add_draw_image(static_image, gfx::Point(0, 0), flags);
  content_layer_client.add_draw_image(animated_loop_none, gfx::Point(100, 100),
                                      flags);
  content_layer_client.add_draw_image(animation_loop_infinite,
                                      gfx::Point(200, 200), flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const auto& animated_images_metadata =
      display_list->discardable_image_map().animated_images_metadata();

  ASSERT_EQ(animated_images_metadata.size(), 1u);
  EXPECT_EQ(animated_images_metadata[0].paint_image_id,
            animation_loop_infinite.stable_id());
  EXPECT_EQ(animated_images_metadata[0].completion_state,
            animation_loop_infinite.completion_state());
  EXPECT_EQ(animated_images_metadata[0].frames,
            animation_loop_infinite.GetFrameMetadata());
  EXPECT_EQ(animated_images_metadata[0].repetition_count,
            animation_loop_infinite.repetition_count());

  std::vector<const DrawImage*> images;
  display_list->discardable_image_map().GetDiscardableImagesInRect(visible_rect,
                                                                   &images);
  ASSERT_EQ(images.size(), 3u);
  EXPECT_EQ(images[0]->paint_image(), static_image);
  EXPECT_DCHECK_DEATH(images[0]->frame_index());
  EXPECT_EQ(images[1]->paint_image(), animated_loop_none);
  EXPECT_DCHECK_DEATH(images[1]->frame_index());
  EXPECT_EQ(images[2]->paint_image(), animation_loop_infinite);
  EXPECT_DCHECK_DEATH(images[2]->frame_index());
}

TEST_F(DiscardableImageMapTest, GathersPaintWorklets) {
  gfx::Rect visible_rect(1000, 1000);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  gfx::Size image_size(100, 100);
  PaintImage static_image = CreateDiscardablePaintImage(image_size);
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(image_size));
  PaintImage paint_worklet_image = CreatePaintWorkletPaintImage(input);

  PaintFlags flags;
  content_layer_client.add_draw_image(static_image, gfx::Point(0, 0), flags);
  content_layer_client.add_draw_image(paint_worklet_image, gfx::Point(100, 100),
                                      flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();

  const auto& paint_worklet_inputs =
      display_list->discardable_image_map().paint_worklet_inputs();
  ASSERT_EQ(paint_worklet_inputs.size(), 1u);
  EXPECT_EQ(paint_worklet_inputs[0].first, input);

  // PaintWorklets are not considered discardable images.
  std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
      display_list->discardable_image_map(), visible_rect);
  ASSERT_EQ(images.size(), 1u);
  EXPECT_EQ(images[0].image, static_image);
}

TEST_F(DiscardableImageMapTest, CapturesImagesInPaintRecordShaders) {
  // Create the record to use in the shader.
  auto shader_record = sk_make_sp<PaintOpBuffer>();
  shader_record->push<ScaleOp>(2.0f, 2.0f);

  PaintImage static_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  shader_record->push<DrawImageOp>(static_image, 0.f, 0.f, nullptr);

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1))};
  PaintImage animated_image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  shader_record->push<DrawImageOp>(animated_image, 0.f, 0.f, nullptr);

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  display_list->push<ScaleOp>(2.0f, 2.0f);
  PaintFlags flags;
  SkRect tile = SkRect::MakeWH(100, 100);
  flags.setShader(PaintShader::MakePaintRecord(
      shader_record, tile, SkTileMode::kClamp, SkTileMode::kClamp, nullptr));
  display_list->push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  EXPECT_EQ(flags.getShader()->image_analysis_state(),
            ImageAnalysisState::kNoAnalysis);
  display_list->GenerateDiscardableImagesMetadata();
  EXPECT_EQ(flags.getShader()->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);
  const auto& image_map = display_list->discardable_image_map();

  // The image rect is set to the rect for the DrawRectOp, and only animated
  // images in a shader are tracked.
  std::vector<PositionScaleDrawImage> draw_images =
      GetDiscardableImagesInRect(image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(draw_images);
  ASSERT_EQ(draw_images.size(), 1u);
  EXPECT_EQ(draw_images[0].image, animated_image);
  // The position of the image is the position of the DrawRectOp that uses the
  // shader.
  EXPECT_EQ(gfx::Rect(400, 400), inset_rects[0]);
  // The scale of the image includes the scale at which the shader record is
  // rasterized.
  EXPECT_EQ(SkSize::Make(4.f, 4.f), draw_images[0].scale);
}

TEST_F(DiscardableImageMapTest, CapturesImagesInPaintFilters) {
  // Create the record to use in the filter.
  auto filter_record = sk_make_sp<PaintOpBuffer>();

  PaintImage static_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  filter_record->push<DrawImageOp>(static_image, 0.f, 0.f, nullptr);

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1))};
  PaintImage animated_image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  filter_record->push<DrawImageOp>(animated_image, 0.f, 0.f, nullptr);

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  PaintFlags flags;
  flags.setImageFilter(sk_make_sp<RecordPaintFilter>(
      filter_record, SkRect::MakeWH(100.f, 100.f)));
  display_list->push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  EXPECT_EQ(flags.getImageFilter()->image_analysis_state(),
            ImageAnalysisState::kNoAnalysis);
  display_list->GenerateDiscardableImagesMetadata();
  EXPECT_EQ(flags.getImageFilter()->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);
  const auto& image_map = display_list->discardable_image_map();

  // The image rect is set to the rect for the DrawRectOp, and only animated
  // images in a filter are tracked.
  std::vector<PositionScaleDrawImage> draw_images =
      GetDiscardableImagesInRect(image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(draw_images);
  ASSERT_EQ(draw_images.size(), 1u);
  EXPECT_EQ(draw_images[0].image, animated_image);
  // The position of the image is the position of the DrawRectOp that uses the
  // filter.
  EXPECT_EQ(gfx::Rect(200, 200), inset_rects[0]);
  // Images in a filter are decoded at the original size.
  EXPECT_EQ(SkSize::Make(1.f, 1.f), draw_images[0].scale);
}

TEST_F(DiscardableImageMapTest, CapturesImagesInSaveLayers) {
  PaintFlags flags;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  flags.setShader(PaintShader::MakeImage(image, SkTileMode::kClamp,
                                         SkTileMode::kClamp, nullptr));

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  display_list->push<SaveLayerOp>(nullptr, &flags);
  display_list->push<DrawColorOp>(SK_ColorBLUE, SkBlendMode::kSrc);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  display_list->GenerateDiscardableImagesMetadata();
  const auto& image_map = display_list->discardable_image_map();
  std::vector<PositionScaleDrawImage> draw_images =
      GetDiscardableImagesInRect(image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(draw_images);
  ASSERT_EQ(draw_images.size(), 1u);
  EXPECT_EQ(draw_images[0].image, image);
  EXPECT_EQ(gfx::Rect(500, 500), inset_rects[0]);
  EXPECT_EQ(SkSize::Make(1.f, 1.f), draw_images[0].scale);
}

TEST_F(DiscardableImageMapTest, EmbeddedShaderWithAnimatedImages) {
  // Create the record with animated image to use in the shader.
  SkRect tile = SkRect::MakeWH(100, 100);
  auto shader_record = sk_make_sp<PaintOpBuffer>();
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1))};
  PaintImage animated_image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  shader_record->push<DrawImageOp>(animated_image, 0.f, 0.f, nullptr);
  auto shader_with_image = PaintShader::MakePaintRecord(
      shader_record, tile, SkTileMode::kClamp, SkTileMode::kClamp, nullptr);

  // Create a second shader which uses the shader above.
  auto second_shader_record = sk_make_sp<PaintOpBuffer>();
  PaintFlags flags;
  flags.setShader(shader_with_image);
  second_shader_record->push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  auto shader_with_shader_with_image = PaintShader::MakePaintRecord(
      second_shader_record, tile, SkTileMode::kClamp, SkTileMode::kClamp,
      nullptr);

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  flags.setShader(shader_with_shader_with_image);
  display_list->push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();
  display_list->GenerateDiscardableImagesMetadata();
  EXPECT_EQ(shader_with_image->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);
  EXPECT_EQ(shader_with_shader_with_image->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);
}

TEST_F(DiscardableImageMapTest, DecodingModeHintsBasic) {
  gfx::Rect visible_rect(100, 100);
  PaintImage unspecified_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(1)
          .set_decoding_mode(PaintImage::DecodingMode::kUnspecified)
          .TakePaintImage();
  PaintImage async_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(2)
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();
  PaintImage sync_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(3)
          .set_decoding_mode(PaintImage::DecodingMode::kSync)
          .TakePaintImage();

  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());
  content_layer_client.add_draw_image(unspecified_image, gfx::Point(0, 0),
                                      PaintFlags());
  content_layer_client.add_draw_image(async_image, gfx::Point(10, 10),
                                      PaintFlags());
  content_layer_client.add_draw_image(sync_image, gfx::Point(20, 20),
                                      PaintFlags());
  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  auto decode_hints = display_list->TakeDecodingModeMap();
  ASSERT_EQ(decode_hints.size(), 3u);
  ASSERT_TRUE(decode_hints.find(1) != decode_hints.end());
  ASSERT_TRUE(decode_hints.find(2) != decode_hints.end());
  ASSERT_TRUE(decode_hints.find(3) != decode_hints.end());
  EXPECT_EQ(decode_hints[1], PaintImage::DecodingMode::kUnspecified);
  EXPECT_EQ(decode_hints[2], PaintImage::DecodingMode::kAsync);
  EXPECT_EQ(decode_hints[3], PaintImage::DecodingMode::kSync);

  decode_hints = display_list->TakeDecodingModeMap();
  EXPECT_EQ(decode_hints.size(), 0u);
}

TEST_F(DiscardableImageMapTest, DecodingModeHintsDuplicates) {
  gfx::Rect visible_rect(100, 100);
  PaintImage unspecified_image1 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(1)
          .set_decoding_mode(PaintImage::DecodingMode::kUnspecified)
          .TakePaintImage();
  PaintImage async_image1 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(1)
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();

  PaintImage unspecified_image2 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(2)
          .set_decoding_mode(PaintImage::DecodingMode::kUnspecified)
          .TakePaintImage();
  PaintImage sync_image2 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(2)
          .set_decoding_mode(PaintImage::DecodingMode::kSync)
          .TakePaintImage();

  PaintImage async_image3 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(3)
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();
  PaintImage sync_image3 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(3)
          .set_decoding_mode(PaintImage::DecodingMode::kSync)
          .TakePaintImage();

  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());
  content_layer_client.add_draw_image(unspecified_image1, gfx::Point(0, 0),
                                      PaintFlags());
  content_layer_client.add_draw_image(async_image1, gfx::Point(10, 10),
                                      PaintFlags());
  content_layer_client.add_draw_image(unspecified_image2, gfx::Point(20, 20),
                                      PaintFlags());
  content_layer_client.add_draw_image(sync_image2, gfx::Point(30, 30),
                                      PaintFlags());
  content_layer_client.add_draw_image(async_image3, gfx::Point(40, 40),
                                      PaintFlags());
  content_layer_client.add_draw_image(sync_image3, gfx::Point(50, 50),
                                      PaintFlags());
  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();

  auto decode_hints = display_list->TakeDecodingModeMap();
  ASSERT_EQ(decode_hints.size(), 3u);
  ASSERT_TRUE(decode_hints.find(1) != decode_hints.end());
  ASSERT_TRUE(decode_hints.find(2) != decode_hints.end());
  ASSERT_TRUE(decode_hints.find(3) != decode_hints.end());
  // 1 was unspecified and async, so the result should be unspecified.
  EXPECT_EQ(decode_hints[1], PaintImage::DecodingMode::kUnspecified);
  // 2 was unspecified and sync, so the result should be sync.
  EXPECT_EQ(decode_hints[2], PaintImage::DecodingMode::kSync);
  // 3 was async and sync, so the result should be sync
  EXPECT_EQ(decode_hints[3], PaintImage::DecodingMode::kSync);

  decode_hints = display_list->TakeDecodingModeMap();
  EXPECT_EQ(decode_hints.size(), 0u);
}

TEST_F(DiscardableImageMapTest, TracksImageRegions) {
  gfx::Rect visible_rect(500, 500);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(1)),
  };
  auto image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  PaintFlags flags;
  content_layer_client.add_draw_image(image, gfx::Point(0, 0), flags);
  content_layer_client.add_draw_image(image, gfx::Point(400, 400), flags);

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const auto& image_map = display_list->discardable_image_map();

  std::vector<gfx::Rect> rects = {gfx::Rect(100, 100),
                                  gfx::Rect(400, 400, 100, 100)};
  Region expected_region;
  for (auto& rect : rects) {
    rect.Inset(-1, -1);
    expected_region.Union(rect);
  }

  EXPECT_EQ(ImageRectsToRegion(image_map.GetRectsForImage(image.stable_id())),
            expected_region);
}

class DiscardableImageMapColorSpaceTest
    : public DiscardableImageMapTest,
      public testing::WithParamInterface<gfx::ColorSpace> {};

TEST_P(DiscardableImageMapColorSpaceTest, ColorSpace) {
  const gfx::ColorSpace image_color_space = GetParam();
  gfx::Rect visible_rect(500, 500);
  PaintImage discardable_image = CreateDiscardablePaintImage(
      gfx::Size(500, 500), image_color_space.ToSkColorSpace());

  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map = display_list->discardable_image_map();

  EXPECT_TRUE(image_map.contains_only_srgb_images());

  content_layer_client.add_draw_image(discardable_image, gfx::Point(0, 0),
                                      PaintFlags());
  display_list = content_layer_client.PaintContentsToDisplayList(
      ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->GenerateDiscardableImagesMetadata();
  const DiscardableImageMap& image_map2 = display_list->discardable_image_map();

  if (!image_color_space.IsValid())
    EXPECT_TRUE(image_map2.contains_only_srgb_images());
  else if (image_color_space == gfx::ColorSpace::CreateSRGB())
    EXPECT_TRUE(image_map2.contains_only_srgb_images());
  else
    EXPECT_FALSE(image_map2.contains_only_srgb_images());
}

gfx::ColorSpace test_color_spaces[] = {
    gfx::ColorSpace(), gfx::ColorSpace::CreateSRGB(),
    gfx::ColorSpace::CreateDisplayP3D65(),
};

INSTANTIATE_TEST_SUITE_P(ColorSpace,
                         DiscardableImageMapColorSpaceTest,
                         testing::ValuesIn(test_color_spaces));

}  // namespace cc
