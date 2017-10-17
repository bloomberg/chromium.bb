// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/playback_image_provider.h"

#include "cc/paint/paint_image_builder.h"
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
    last_image_ = draw_image;
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
  const DrawImage& last_image() { return last_image_; }

 private:
  int refed_image_count_ = 0;
  int images_decoded_ = 0;
  DrawImage last_image_;
};

TEST(PlaybackImageProviderTest, SkipsAllImages) {
  MockDecodeCache cache;
  PlaybackImageProvider provider(&cache, gfx::ColorSpace(), base::nullopt);
  provider.BeginRaster();

  SkIRect rect = SkIRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();

  EXPECT_FALSE(provider.GetDecodedDrawImage(
      DrawImage(PaintImageBuilder::WithDefault()
                    .set_id(PaintImage::kNonLazyStableId)
                    .set_image(CreateRasterImage())
                    .TakePaintImage(),
                rect, kMedium_SkFilterQuality, matrix)));
  EXPECT_EQ(cache.images_decoded(), 0);

  EXPECT_FALSE(provider.GetDecodedDrawImage(
      CreateDiscardableDrawImage(gfx::Size(10, 10), nullptr, SkRect::Make(rect),
                                 kMedium_SkFilterQuality, matrix)));
  EXPECT_EQ(cache.images_decoded(), 0);
  provider.EndRaster();
}

TEST(PlaybackImageProviderTest, SkipsSomeImages) {
  MockDecodeCache cache;
  PaintImage skip_image = CreateDiscardablePaintImage(gfx::Size(10, 10));

  base::Optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  settings->images_to_skip = {skip_image.stable_id()};

  PlaybackImageProvider provider(&cache, gfx::ColorSpace(), settings);
  provider.BeginRaster();

  SkIRect rect = SkIRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();
  EXPECT_FALSE(provider.GetDecodedDrawImage(
      DrawImage(skip_image, rect, kMedium_SkFilterQuality, matrix)));
  EXPECT_EQ(cache.images_decoded(), 0);
  provider.EndRaster();
}

TEST(PlaybackImageProviderTest, RefAndUnrefDecode) {
  MockDecodeCache cache;

  base::Optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  PlaybackImageProvider provider(&cache, gfx::ColorSpace(), settings);
  provider.BeginRaster();

  {
    SkRect rect = SkRect::MakeWH(10, 10);
    SkMatrix matrix = SkMatrix::I();
    auto decode = provider.GetDecodedDrawImage(CreateDiscardableDrawImage(
        gfx::Size(10, 10), nullptr, rect, kMedium_SkFilterQuality, matrix));
    EXPECT_TRUE(decode);
    EXPECT_EQ(cache.refed_image_count(), 1);
  }

  // Destroying the decode unrefs the image from the cache.
  EXPECT_EQ(cache.refed_image_count(), 0);

  provider.EndRaster();
}

TEST(PlaybackImageProviderTest, AtRasterImages) {
  MockDecodeCache cache;

  SkRect rect = SkRect::MakeWH(10, 10);
  gfx::Size size(10, 10);
  SkMatrix matrix = SkMatrix::I();
  auto draw_image1 = CreateDiscardableDrawImage(
      size, nullptr, rect, kMedium_SkFilterQuality, matrix);
  auto draw_image2 = CreateDiscardableDrawImage(
      size, nullptr, rect, kMedium_SkFilterQuality, matrix);

  base::Optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  settings->at_raster_images = {draw_image1, draw_image2};

  PlaybackImageProvider provider(&cache, gfx::ColorSpace(), settings);

  EXPECT_EQ(cache.refed_image_count(), 0);
  provider.BeginRaster();
  EXPECT_EQ(cache.refed_image_count(), 2);
  EXPECT_EQ(cache.images_decoded(), 2);

  provider.EndRaster();
  EXPECT_EQ(cache.refed_image_count(), 0);
  EXPECT_EQ(cache.images_decoded(), 2);
}

TEST(PlaybackImageProviderTest, SwapsGivenFrames) {
  MockDecodeCache cache;
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};
  PaintImage image = CreateAnimatedImage(gfx::Size(10, 10), frames);

  base::flat_map<PaintImage::Id, size_t> image_to_frame;
  image_to_frame[image.stable_id()] = 1u;
  base::Optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  settings->image_to_current_frame_index = image_to_frame;

  PlaybackImageProvider provider(&cache, gfx::ColorSpace(), settings);
  provider.BeginRaster();

  SkIRect rect = SkIRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();
  DrawImage draw_image(image, rect, kMedium_SkFilterQuality, matrix);
  provider.GetDecodedDrawImage(draw_image);
  ASSERT_TRUE(cache.last_image().paint_image());
  ASSERT_EQ(cache.last_image().paint_image(), image);
  ASSERT_EQ(cache.last_image().frame_index(), 1u);

  provider.EndRaster();
}

TEST(PlaybackImageProviderTest, UsePaintImagesWithDifferentFrames) {
  MockDecodeCache cache;

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};
  // First image with 2 frames.
  auto image1 = CreateAnimatedImage(gfx::Size(10, 10), frames);
  // Second image with 3 frames.
  frames.push_back(FrameMetadata(true, base::TimeDelta::FromMilliseconds(4)));
  auto image2 =
      CreateAnimatedImage(gfx::Size(10, 10), frames, kAnimationLoopInfinite, 0u,
                          image1.stable_id());

  // Tell the provider to use the third frame.
  base::Optional<PlaybackImageProvider::Settings> settings;
  settings.emplace();
  settings->image_to_current_frame_index[image1.stable_id()] = 2u;

  PlaybackImageProvider provider(&cache, gfx::ColorSpace(), settings);
  provider.BeginRaster();

  SkIRect rect = SkIRect::MakeWH(10, 10);
  SkMatrix matrix = SkMatrix::I();
  DrawImage draw_image(image1, rect, kMedium_SkFilterQuality, matrix);
  provider.GetDecodedDrawImage(draw_image);
  EXPECT_EQ(cache.last_image().frame_index(), 1u);
  draw_image = DrawImage(image2, rect, kMedium_SkFilterQuality, matrix);
  provider.GetDecodedDrawImage(draw_image);
  EXPECT_EQ(cache.last_image().frame_index(), 2u);

  provider.EndRaster();
}

}  // namespace
}  // namespace cc
