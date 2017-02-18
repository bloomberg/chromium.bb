// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cc/playback/image_hijack_canvas.h"

#include "cc/test/skia_common.h"
#include "cc/tiles/image_decode_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPath.h"

namespace cc {
namespace {

class MockImageDecodeCache : public ImageDecodeCache {
 public:
  MOCK_METHOD3(GetTaskForImageAndRef,
               bool(const DrawImage& image,
                    const TracingInfo& tracing_info,
                    scoped_refptr<TileTask>* task));
  MOCK_METHOD1(UnrefImage, void(const DrawImage& image));
  MOCK_METHOD1(GetDecodedImageForDraw,
               DecodedDrawImage(const DrawImage& image));
  MOCK_METHOD2(DrawWithImageFinished,
               void(const DrawImage& image,
                    const DecodedDrawImage& decoded_image));
  MOCK_METHOD0(ReduceCacheUsage, void());
  MOCK_METHOD1(SetShouldAggressivelyFreeResources,
               void(bool aggressively_free_resources));
  MOCK_METHOD2(GetOutOfRasterDecodeTaskForImageAndRef,
               bool(const DrawImage& image, scoped_refptr<TileTask>* task));
};

TEST(ImageHijackCanvasTest, NonLazyImagesSkipped) {
  // Use a strict mock so that if *any* ImageDecodeCache methods are called, we
  // will hit an error.
  testing::StrictMock<MockImageDecodeCache> image_decode_cache;
  ImageIdFlatSet images_to_skip;
  ImageHijackCanvas canvas(100, 100, &image_decode_cache, &images_to_skip);

  // Use an SkBitmap backed image to ensure that the image is not
  // lazy-generated.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10, true);
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);

  SkPaint paint;
  canvas.drawImage(image, 0, 0, &paint);
  canvas.drawImageRect(image, SkRect::MakeXYWH(0, 0, 10, 10),
                       SkRect::MakeXYWH(10, 10, 10, 10), &paint);

  SkPaint image_paint;
  image_paint.setShader(
      image->makeShader(SkShader::kClamp_TileMode, SkShader::kClamp_TileMode));
  SkRect paint_rect = SkRect::MakeXYWH(0, 0, 100, 100);
  canvas.drawRect(paint_rect, image_paint);
  SkPath path;
  path.addRect(paint_rect, SkPath::kCW_Direction);
  canvas.drawPath(path, image_paint);
  canvas.drawOval(paint_rect, image_paint);
  canvas.drawArc(paint_rect, 0, 40, true, image_paint);
  canvas.drawRRect(SkRRect::MakeRect(paint_rect), image_paint);
}

TEST(ImageHijackCanvasTest, ImagesToSkipAreSkipped) {
  // Use a strict mock so that if *any* ImageDecodeCache methods are called, we
  // will hit an error.
  testing::StrictMock<MockImageDecodeCache> image_decode_cache;
  ImageIdFlatSet images_to_skip;
  sk_sp<SkImage> image = CreateDiscardableImage(gfx::Size(10, 10));
  images_to_skip.insert(image->uniqueID());
  ImageHijackCanvas canvas(100, 100, &image_decode_cache, &images_to_skip);

  SkPaint paint;
  canvas.drawImage(image, 0, 0, &paint);
  canvas.drawImageRect(image, SkRect::MakeXYWH(0, 0, 10, 10),
                       SkRect::MakeXYWH(10, 10, 10, 10), &paint);
  paint.setShader(image->makeShader(SkShader::kClamp_TileMode,
                                    SkShader::kClamp_TileMode, nullptr));
  canvas.drawRect(SkRect::MakeXYWH(10, 10, 10, 10), paint);
}

}  // namespace

}  // namespace cc
