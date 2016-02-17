// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_decode_controller.h"

#include "cc/playback/draw_image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

skia::RefPtr<SkImage> CreateImage(int width, int height) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  return skia::AdoptRef(SkImage::NewFromBitmap(bitmap));
}

TEST(ImageDecodeControllerTest, ImageKeyLowQuality) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality qualities[] = {kNone_SkFilterQuality, kLow_SkFilterQuality};
  for (auto quality : qualities) {
    DrawImage draw_image(
        image.get(), SkIRect::MakeWH(image->width(), image->height()),
        SkSize::Make(0.5f, 1.5f), quality, has_perspective, is_decomposable);

    auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
    EXPECT_EQ(image->uniqueID(), key.image_id());
    EXPECT_EQ(quality, key.filter_quality());
    EXPECT_EQ(50, key.target_size().width());
    EXPECT_EQ(150, key.target_size().height());
    EXPECT_EQ(50u * 150u * 4u, key.target_bytes());
  }
}

TEST(ImageDecodeControllerTest, ImageKeyMediumQuality) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 1.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(quality, key.filter_quality());
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(150, key.target_size().height());
  EXPECT_EQ(50u * 150u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyMediumQualityEvenWithPerspective) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = true;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 1.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(quality, key.filter_quality());
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(150, key.target_size().height());
  EXPECT_EQ(50u * 150u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyMediumQualityDropToLowIfEnlarging) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(1.5f, 1.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(150, key.target_size().width());
  EXPECT_EQ(150, key.target_size().height());
  EXPECT_EQ(150u * 150u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyMediumQualityDropToLowIfIdentity) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(1.f, 1.f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(100u * 100u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest,
     ImageKeyMediumQualityDropToLowIfNearlyIdentity) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(1.001f, 1.001f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(100u * 100u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest,
     ImageKeyMediumQualityDropToLowIfNearlyIdentity2) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.999f, 0.999f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(100u * 100u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest,
     ImageKeyMediumQualityDropToLowIfNotDecomposable) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = false;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 1.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(150, key.target_size().height());
  EXPECT_EQ(50u * 150u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyHighQuality) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 1.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(quality, key.filter_quality());
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(150, key.target_size().height());
  EXPECT_EQ(50u * 150u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest,
     ImageKeyHighQualityDropToMediumWithPerspective) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = true;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 1.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kMedium_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(150, key.target_size().height());
  EXPECT_EQ(50u * 150u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyHighQualityDropToMediumIfTooLarge) {
  // Just over 64MB when scaled.
  skia::RefPtr<SkImage> image = CreateImage(4555, 2048);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // At least one dimension should scale down, so that medium quality doesn't
  // become low.
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.9f, 2.f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kMedium_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(4100, key.target_size().width());
  EXPECT_EQ(4096, key.target_size().height());
  EXPECT_EQ(4100u * 4096u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyHighQualityDropToLowIfNotDecomposable) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = false;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 1.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(50, key.target_size().width());
  EXPECT_EQ(150, key.target_size().height());
  EXPECT_EQ(50u * 150u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyHighQualityDropToLowIfIdentity) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(1.f, 1.f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(100u * 100u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyHighQualityDropToLowIfNearlyIdentity) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(1.001f, 1.001f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(100u * 100u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageKeyHighQualityDropToLowIfNearlyIdentity2) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.999f, 0.999f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(100, key.target_size().width());
  EXPECT_EQ(100, key.target_size().height());
  EXPECT_EQ(100u * 100u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageRectDoesNotContainSrcRect) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeXYWH(25, 35, image->width(), image->height()),
      SkSize::Make(1.f, 1.f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kLow_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(75, key.target_size().width());
  EXPECT_EQ(65, key.target_size().height());
  EXPECT_EQ(gfx::Rect(25, 35, 75, 65), key.src_rect());
  EXPECT_EQ(75u * 65u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, ImageRectDoesNotContainSrcRectWithScale) {
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(
      image.get(), SkIRect::MakeXYWH(20, 30, image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);

  auto key = ImageDecodeControllerKey::FromDrawImage(draw_image);
  EXPECT_EQ(image->uniqueID(), key.image_id());
  EXPECT_EQ(kHigh_SkFilterQuality, key.filter_quality());
  EXPECT_EQ(40, key.target_size().width());
  EXPECT_EQ(35, key.target_size().height());
  EXPECT_EQ(gfx::Rect(20, 30, 80, 70), key.src_rect());
  EXPECT_EQ(40u * 35u * 4u, key.target_bytes());
}

TEST(ImageDecodeControllerTest, GetTaskForImageSameImage) {
  ImageDecodeController controller;
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  uint64_t prepare_tiles_id = 1;

  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  DrawImage another_draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      another_draw_image, prepare_tiles_id, &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task.get() == another_task.get());

  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest, GetTaskForImageSameImageDifferentQuality) {
  ImageDecodeController controller;
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;

  DrawImage high_quality_draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), kHigh_SkFilterQuality, has_perspective,
      is_decomposable);
  scoped_refptr<ImageDecodeTask> high_quality_task;
  bool need_unref = controller.GetTaskForImageAndRef(
      high_quality_draw_image, prepare_tiles_id, &high_quality_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(high_quality_task);

  DrawImage medium_quality_draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), kMedium_SkFilterQuality, has_perspective,
      is_decomposable);
  scoped_refptr<ImageDecodeTask> medium_quality_task;
  need_unref = controller.GetTaskForImageAndRef(
      medium_quality_draw_image, prepare_tiles_id, &medium_quality_task);
  // Medium quality isn't handled by the controller, so it won't ref it. Note
  // that this will change when medium quality is handled and will need to be
  // updated.
  EXPECT_FALSE(need_unref);
  EXPECT_TRUE(medium_quality_task);
  EXPECT_TRUE(high_quality_task.get() != medium_quality_task.get());

  DrawImage low_quality_draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), kLow_SkFilterQuality, has_perspective,
      is_decomposable);
  scoped_refptr<ImageDecodeTask> low_quality_task;
  need_unref = controller.GetTaskForImageAndRef(
      low_quality_draw_image, prepare_tiles_id, &low_quality_task);
  // Note that when we pin the original decode, we might ref low quality images
  // too, but we don't support that right now.
  EXPECT_FALSE(need_unref);
  EXPECT_TRUE(low_quality_task);
  EXPECT_TRUE(high_quality_task.get() != low_quality_task.get());
  EXPECT_TRUE(medium_quality_task.get() != low_quality_task.get());

  controller.UnrefImage(high_quality_draw_image);
}

TEST(ImageDecodeControllerTest, GetTaskForImageSameImageDifferentSize) {
  ImageDecodeController controller;
  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage half_size_draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> half_size_task;
  bool need_unref = controller.GetTaskForImageAndRef(
      half_size_draw_image, prepare_tiles_id, &half_size_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(half_size_task);

  DrawImage quarter_size_draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.25f, 0.25f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> quarter_size_task;
  need_unref = controller.GetTaskForImageAndRef(
      quarter_size_draw_image, prepare_tiles_id, &quarter_size_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(quarter_size_task);
  EXPECT_TRUE(half_size_task.get() != quarter_size_task.get());

  controller.UnrefImage(half_size_draw_image);
  controller.UnrefImage(quarter_size_draw_image);
}

TEST(ImageDecodeControllerTest, GetTaskForImageDifferentImage) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> first_image = CreateImage(100, 100);
  DrawImage first_draw_image(
      first_image.get(),
      SkIRect::MakeWH(first_image->width(), first_image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> first_task;
  bool need_unref = controller.GetTaskForImageAndRef(
      first_draw_image, prepare_tiles_id, &first_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(first_task);

  skia::RefPtr<SkImage> second_image = CreateImage(100, 100);
  DrawImage second_draw_image(
      second_image.get(),
      SkIRect::MakeWH(second_image->width(), second_image->height()),
      SkSize::Make(0.25f, 0.25f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> second_task;
  need_unref = controller.GetTaskForImageAndRef(second_draw_image,
                                                prepare_tiles_id, &second_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(second_task);
  EXPECT_TRUE(first_task.get() != second_task.get());

  controller.UnrefImage(first_draw_image);
  controller.UnrefImage(second_draw_image);
}

TEST(ImageDecodeControllerTest, GetTaskForImageAlreadyDecoded) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();
  task->RunOnWorkerThread();

  scoped_refptr<ImageDecodeTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id,
                                                &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_FALSE(another_task);

  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest, GetTaskForImageAlreadyPrerolled) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kLow_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(1.f, 1.f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  // We're not currently locking original scale or low quality images.
  EXPECT_FALSE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();
  task->RunOnWorkerThread();

  scoped_refptr<ImageDecodeTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id,
                                                &another_task);
  EXPECT_FALSE(need_unref);
  EXPECT_FALSE(another_task);

  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  scoped_refptr<ImageDecodeTask> third_task;
  need_unref = controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id,
                                                &third_task);
  EXPECT_FALSE(need_unref);
  EXPECT_FALSE(third_task);
}

TEST(ImageDecodeControllerTest, GetTaskForImageCanceledGetsNewTask) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();

  scoped_refptr<ImageDecodeTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id,
                                                &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(another_task.get() == task.get());

  // Didn't run the task, complete it (it was canceled).
  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  // Fully cancel everything (so the raster would unref things).
  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);

  // Here a new task is created.
  scoped_refptr<ImageDecodeTask> third_task;
  need_unref = controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id,
                                                &third_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(third_task);
  EXPECT_FALSE(third_task.get() == task.get());

  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest, GetTaskForImageCanceledWhileReffedGetsNewTask) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();

  scoped_refptr<ImageDecodeTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id,
                                                &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(another_task.get() == task.get());

  // Didn't run the task, complete it (it was canceled).
  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  // Note that here, everything is reffed, but a new task is created. This is
  // possible with repeated schedule/cancel operations.
  scoped_refptr<ImageDecodeTask> third_task;
  need_unref = controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id,
                                                &third_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(third_task);
  EXPECT_FALSE(third_task.get() == task.get());

  // 3 Unrefs!
  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest, GetDecodedImageForDraw) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();

  task->RunOnWorkerThread();

  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest, GetDecodedImageForDrawWithNonContainedSrcRect) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeXYWH(20, 30, image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);
  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();

  task->RunOnWorkerThread();

  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(40, decoded_draw_image.image()->width());
  EXPECT_EQ(35, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest, GetDecodedImageForDrawAtRasterDecode) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(ImageDecodeControllerTest,
     GetDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());

  DecodedDrawImage another_decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  controller.DrawWithImageFinished(draw_image, another_decoded_draw_image);
}

TEST(ImageDecodeControllerTest,
     GetDecodedImageForDrawAtRasterDecodeDoesNotPreventTasks) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());

  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();

  task->RunOnWorkerThread();

  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  DecodedDrawImage another_decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  // This should get the new decoded/locked image, not the one we're using at
  // raster.
  // TODO(vmpstr): We can possibly optimize this so that the decode simply moves
  // the image to the right spot.
  EXPECT_NE(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());
  EXPECT_FALSE(another_decoded_draw_image.is_at_raster_decode());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  controller.DrawWithImageFinished(draw_image, another_decoded_draw_image);
  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest,
     GetDecodedImageForDrawAtRasterDecodeIsUsedForLockedCache) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.5f, 0.5f), quality, has_perspective, is_decomposable);

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(50, decoded_draw_image.image()->width());
  EXPECT_EQ(50, decoded_draw_image.image()->height());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().width());
  EXPECT_FLOAT_EQ(0.5f, decoded_draw_image.scale_adjustment().height());
  EXPECT_EQ(kLow_SkFilterQuality, decoded_draw_image.filter_quality());
  EXPECT_FALSE(decoded_draw_image.is_scale_adjustment_identity());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());

  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  task->WillSchedule();
  task->ScheduleOnOriginThread(nullptr);
  task->DidSchedule();

  // If we finish the draw here, then we will use it for the locked decode
  // instead of decoding again.
  controller.DrawWithImageFinished(draw_image, decoded_draw_image);

  task->RunOnWorkerThread();

  task->WillComplete();
  task->CompleteOnOriginThread(nullptr);
  task->DidComplete();

  DecodedDrawImage another_decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  // This should get the decoded/locked image which we originally decoded at
  // raster time, since it's now in the locked cache.
  EXPECT_EQ(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());
  EXPECT_FALSE(another_decoded_draw_image.is_at_raster_decode());

  controller.DrawWithImageFinished(draw_image, another_decoded_draw_image);
  controller.UnrefImage(draw_image);
}

TEST(ImageDecodeControllerTest, ZeroSizedImagesAreSkipped) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeWH(image->width(), image->height()),
      SkSize::Make(0.f, 0.f), quality, has_perspective, is_decomposable);

  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_FALSE(task);
  EXPECT_FALSE(need_unref);

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(ImageDecodeControllerTest, NonOverlappingSrcRectImagesAreSkipped) {
  ImageDecodeController controller;
  bool has_perspective = false;
  bool is_decomposable = true;
  uint64_t prepare_tiles_id = 1;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  skia::RefPtr<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image.get(), SkIRect::MakeXYWH(150, 150, image->width(), image->height()),
      SkSize::Make(1.f, 1.f), quality, has_perspective, is_decomposable);

  scoped_refptr<ImageDecodeTask> task;
  bool need_unref =
      controller.GetTaskForImageAndRef(draw_image, prepare_tiles_id, &task);
  EXPECT_FALSE(task);
  EXPECT_FALSE(need_unref);

  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
}

}  // namespace
}  // namespace cc
