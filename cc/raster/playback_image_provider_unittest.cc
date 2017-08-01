// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/playback_image_provider.h"

#include "cc/test/skia_common.h"
#include "cc/test/stub_decode_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

sk_sp<SkImage> CreateRasterImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  return SkImage::MakeFromBitmap(bitmap);
}

DecodedDrawImage CreateDecode() {
  return DecodedDrawImage(CreateRasterImage(), SkSize::MakeEmpty(),
                          SkSize::Make(1.0f, 1.0f), kMedium_SkFilterQuality);
}

class MockDecodeCache : public StubDecodeCache {
 public:
  MockDecodeCache() = default;
  ~MockDecodeCache() override { EXPECT_EQ(refed_image_count_, 0); }

  DecodedDrawImage GetDecodedImageForDraw(
      const DrawImage& draw_image) override {
    images_decoded_++;
    refed_image_count_++;
    return CreateDecode();
  }

  void DrawWithImageFinished(
      const DrawImage& draw_image,
      const DecodedDrawImage& decoded_draw_image) override {
    refed_image_count_--;
    EXPECT_GE(refed_image_count_, 0);
  }

  int refed_image_count() const { return refed_image_count_; }
  int images_decoded() const { return images_decoded_; }

 private:
  int refed_image_count_ = 0;
  int images_decoded_ = 0;
};

TEST(PlaybackImageProviderTest, SkipsAllImages) {
  MockDecodeCache cache;
  PlaybackImageProvider provider(true, {}, &cache, gfx::ColorSpace());

  SkRect rect = SkRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();

  EXPECT_FALSE(provider.GetDecodedDrawImage(
      PaintImage(PaintImage::kNonLazyStableId, CreateRasterImage()), rect,
      kMedium_SkFilterQuality, matrix));
  EXPECT_EQ(cache.images_decoded(), 0);

  EXPECT_FALSE(provider.GetDecodedDrawImage(
      PaintImage(PaintImage::GetNextId(),
                 CreateDiscardableImage(gfx::Size(10, 10))),
      rect, kMedium_SkFilterQuality, matrix));
  EXPECT_EQ(cache.images_decoded(), 0);
}

TEST(PlaybackImageProviderTest, SkipsSomeImages) {
  MockDecodeCache cache;
  PaintImage skip_image = PaintImage(PaintImage::GetNextId(),
                                     CreateDiscardableImage(gfx::Size(10, 10)));
  PlaybackImageProvider provider(false, {skip_image.stable_id()}, &cache,
                                 gfx::ColorSpace());

  SkRect rect = SkRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();
  EXPECT_FALSE(provider.GetDecodedDrawImage(skip_image, rect,
                                            kMedium_SkFilterQuality, matrix));
  EXPECT_EQ(cache.images_decoded(), 0);
}

TEST(PlaybackImageProviderTest, RefAndUnrefDecode) {
  MockDecodeCache cache;
  PlaybackImageProvider provider(false, {}, &cache, gfx::ColorSpace());

  {
    SkRect rect = SkRect::MakeWH(10, 10);
    SkMatrix matrix = SkMatrix::I();
    auto decode = provider.GetDecodedDrawImage(
        PaintImage(PaintImage::GetNextId(),
                   CreateDiscardableImage(gfx::Size(10, 10))),
        rect, kMedium_SkFilterQuality, matrix);
    EXPECT_TRUE(decode);
    EXPECT_EQ(cache.refed_image_count(), 1);
  }

  // Destroying the decode unrefs the image from the cache.
  EXPECT_EQ(cache.refed_image_count(), 0);
}

}  // namespace
}  // namespace cc
