// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/playback_image_provider.h"

#include "cc/test/skia_common.h"
#include "cc/test/stub_decode_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using testing::_;
using testing::Return;

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
  ~MockDecodeCache() override = default;

  MOCK_METHOD1(GetDecodedImageForDraw, DecodedDrawImage(const DrawImage&));
  MOCK_METHOD2(DrawWithImageFinished,
               void(const DrawImage&, const DecodedDrawImage&));
};

TEST(PlaybackImageProviderTest, SkipsAllImages) {
  testing::StrictMock<MockDecodeCache> cache;
  PlaybackImageProvider provider(true, {}, &cache, gfx::ColorSpace());

  SkRect rect = SkRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();
  EXPECT_EQ(provider.GetDecodedImage(
                PaintImage(PaintImage::kNonLazyStableId, CreateRasterImage()),
                rect, kMedium_SkFilterQuality, matrix),
            nullptr);
  EXPECT_EQ(provider.GetDecodedImage(
                PaintImage(PaintImage::GetNextId(),
                           CreateDiscardableImage(gfx::Size(10, 10))),
                rect, kMedium_SkFilterQuality, matrix),
            nullptr);
}

TEST(PlaybackImageProviderTest, SkipsSomeImages) {
  testing::StrictMock<MockDecodeCache> cache;
  PaintImage skip_image = PaintImage(PaintImage::GetNextId(),
                                     CreateDiscardableImage(gfx::Size(10, 10)));
  PlaybackImageProvider provider(false, {skip_image.stable_id()}, &cache,
                                 gfx::ColorSpace());

  SkRect rect = SkRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();
  EXPECT_EQ(provider.GetDecodedImage(skip_image, rect, kMedium_SkFilterQuality,
                                     matrix),
            nullptr);
}

TEST(PlaybackImageProviderTest, RefAndUnrefDecode) {
  testing::StrictMock<MockDecodeCache> cache;
  PlaybackImageProvider provider(false, {}, &cache, gfx::ColorSpace());

  EXPECT_CALL(cache, GetDecodedImageForDraw(_))
      .WillOnce(Return(CreateDecode()));
  SkRect rect = SkRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();
  auto decode = provider.GetDecodedImage(
      PaintImage(PaintImage::GetNextId(),
                 CreateDiscardableImage(gfx::Size(10, 10))),
      rect, kMedium_SkFilterQuality, matrix);
  EXPECT_NE(decode, nullptr);

  EXPECT_CALL(cache, DrawWithImageFinished(_, _));
  decode.reset();
}

}  // namespace
}  // namespace cc
