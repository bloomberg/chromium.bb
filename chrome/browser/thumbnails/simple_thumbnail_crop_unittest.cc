// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"

#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/canvas.h"
#include "ui/surface/transport_dib.h"

using content::WebContents;
using thumbnails::SimpleThumbnailCrop;

typedef testing::Test SimpleThumbnailCropTest;

TEST_F(SimpleThumbnailCropTest, CalculateBoringScore_Empty) {
  SkBitmap bitmap;
  EXPECT_DOUBLE_EQ(1.0, SimpleThumbnailCrop::CalculateBoringScore(bitmap));
}

TEST_F(SimpleThumbnailCropTest, CalculateBoringScore_SingleColor) {
  const gfx::Size kSize(20, 10);
  gfx::Canvas canvas(kSize, ui::SCALE_FACTOR_100P, true);
  // Fill all pixels in black.
  canvas.FillRect(gfx::Rect(kSize), SK_ColorBLACK);

  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  // The thumbnail should deserve the highest boring score.
  EXPECT_DOUBLE_EQ(1.0, SimpleThumbnailCrop::CalculateBoringScore(bitmap));
}

TEST_F(SimpleThumbnailCropTest, CalculateBoringScore_TwoColors) {
  const gfx::Size kSize(20, 10);

  gfx::Canvas canvas(kSize, ui::SCALE_FACTOR_100P, true);
  // Fill all pixels in black.
  canvas.FillRect(gfx::Rect(kSize), SK_ColorBLACK);
  // Fill the left half pixels in white.
  canvas.FillRect(gfx::Rect(0, 0, kSize.width() / 2, kSize.height()),
                  SK_ColorWHITE);

  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  ASSERT_EQ(kSize.width(), bitmap.width());
  ASSERT_EQ(kSize.height(), bitmap.height());
  // The thumbnail should be less boring because two colors are used.
  EXPECT_DOUBLE_EQ(0.5, SimpleThumbnailCrop::CalculateBoringScore(bitmap));
}

TEST_F(SimpleThumbnailCropTest, GetClippedBitmap_TallerThanWide) {
  // The input bitmap is vertically long.
  gfx::Canvas canvas(gfx::Size(40, 90), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  SkBitmap clipped_bitmap = SimpleThumbnailCrop::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was taller than wide.
  EXPECT_EQ(thumbnails::CLIP_RESULT_TALLER_THAN_WIDE, clip_result);
}

TEST_F(SimpleThumbnailCropTest, GetClippedBitmap_WiderThanTall) {
  // The input bitmap is horizontally long.
  gfx::Canvas canvas(gfx::Size(70, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  SkBitmap clipped_bitmap = SimpleThumbnailCrop::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was wider than tall.
  EXPECT_EQ(thumbnails::CLIP_RESULT_WIDER_THAN_TALL, clip_result);
}

TEST_F(SimpleThumbnailCropTest, GetClippedBitmap_TooWiderThanTall) {
  // The input bitmap is horizontally very long.
  gfx::Canvas canvas(gfx::Size(90, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  SkBitmap clipped_bitmap = SimpleThumbnailCrop::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was wider than tall.
  EXPECT_EQ(thumbnails::CLIP_RESULT_MUCH_WIDER_THAN_TALL, clip_result);
}

TEST_F(SimpleThumbnailCropTest, GetClippedBitmap_NotClipped) {
  // The input bitmap is square.
  gfx::Canvas canvas(gfx::Size(40, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  SkBitmap clipped_bitmap = SimpleThumbnailCrop::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // There was no need to clip.
  EXPECT_EQ(thumbnails::CLIP_RESULT_NOT_CLIPPED, clip_result);
}

TEST_F(SimpleThumbnailCropTest, GetClippedBitmap_NonSquareOutput) {
  // The input bitmap is square.
  gfx::Canvas canvas(gfx::Size(40, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is horizontally long.
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  SkBitmap clipped_bitmap = SimpleThumbnailCrop::GetClippedBitmap(
      bitmap, 20, 10, &clip_result);
  // The clipped bitmap should have the same aspect ratio of the desired size.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(20, clipped_bitmap.height());
  // The input was taller than wide.
  EXPECT_EQ(thumbnails::CLIP_RESULT_TALLER_THAN_WIDE, clip_result);
}

TEST_F(SimpleThumbnailCropTest, GetCanvasCopyInfo) {
  gfx::Size thumbnail_size(200, 120);
  float desired_aspect =
      static_cast<float>(thumbnail_size.width()) / thumbnail_size.height();
  scoped_refptr<thumbnails::ThumbnailingAlgorithm> algorithm(
      new SimpleThumbnailCrop(thumbnail_size));
  gfx::Rect clipping_rect_result;
  gfx::Size target_size_result;

  thumbnails::ClipResult clip_result = algorithm->GetCanvasCopyInfo(
      gfx::Size(400, 210),
      ui::SCALE_FACTOR_200P,
      &clipping_rect_result,
      &target_size_result);
  gfx::Size clipping_size = clipping_rect_result.size();
  float clip_aspect =
      static_cast<float>(clipping_size.width()) / clipping_size.height();
  EXPECT_EQ(thumbnails::CLIP_RESULT_WIDER_THAN_TALL, clip_result);
  EXPECT_EQ(thumbnail_size, target_size_result);
  EXPECT_NEAR(desired_aspect, clip_aspect, 0.01);

  clip_result = algorithm->GetCanvasCopyInfo(
      gfx::Size(600, 200),
      ui::SCALE_FACTOR_200P,
      &clipping_rect_result,
      &target_size_result);
  clipping_size = clipping_rect_result.size();
  clip_aspect =
      static_cast<float>(clipping_size.width()) / clipping_size.height();
  EXPECT_EQ(thumbnails::CLIP_RESULT_MUCH_WIDER_THAN_TALL, clip_result);
  EXPECT_EQ(thumbnail_size, target_size_result);
  EXPECT_NEAR(desired_aspect, clip_aspect, 0.01);

  clip_result = algorithm->GetCanvasCopyInfo(
      gfx::Size(300, 600),
      ui::SCALE_FACTOR_200P,
      &clipping_rect_result,
      &target_size_result);
  clipping_size = clipping_rect_result.size();
  clip_aspect =
      static_cast<float>(clipping_size.width()) / clipping_size.height();
  EXPECT_EQ(thumbnails::CLIP_RESULT_TALLER_THAN_WIDE, clip_result);
  EXPECT_EQ(thumbnail_size, target_size_result);
  EXPECT_NEAR(desired_aspect, clip_aspect, 0.01);

  clip_result = algorithm->GetCanvasCopyInfo(
      gfx::Size(200, 100),
      ui::SCALE_FACTOR_200P,
      &clipping_rect_result,
      &target_size_result);
  EXPECT_EQ(thumbnails::CLIP_RESULT_SOURCE_IS_SMALLER, clip_result);
  EXPECT_EQ(thumbnail_size, target_size_result);
}
