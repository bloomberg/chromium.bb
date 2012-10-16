// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"

#include "base/basictypes.h"
#include "base/stringprintf.h"
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

typedef testing::Test ThumbnailTabHelperTest;

TEST_F(ThumbnailTabHelperTest, CalculateBoringScore_Empty) {
  SkBitmap bitmap;
  EXPECT_DOUBLE_EQ(1.0, ThumbnailTabHelper::CalculateBoringScore(bitmap));
}

TEST_F(ThumbnailTabHelperTest, CalculateBoringScore_SingleColor) {
  const gfx::Size kSize(20, 10);
  gfx::Canvas canvas(kSize, ui::SCALE_FACTOR_100P, true);
  // Fill all pixels in black.
  canvas.FillRect(gfx::Rect(kSize), SK_ColorBLACK);

  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  // The thumbnail should deserve the highest boring score.
  EXPECT_DOUBLE_EQ(1.0, ThumbnailTabHelper::CalculateBoringScore(bitmap));
}

TEST_F(ThumbnailTabHelperTest, CalculateBoringScore_TwoColors) {
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
  EXPECT_DOUBLE_EQ(0.5, ThumbnailTabHelper::CalculateBoringScore(bitmap));
}

TEST_F(ThumbnailTabHelperTest, GetClippedBitmap_TallerThanWide) {
  // The input bitmap is vertically long.
  gfx::Canvas canvas(gfx::Size(40, 90), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailTabHelper::ClipResult clip_result = ThumbnailTabHelper::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailTabHelper::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was taller than wide.
  EXPECT_EQ(ThumbnailTabHelper::kTallerThanWide, clip_result);
}

TEST_F(ThumbnailTabHelperTest, GetClippedBitmap_WiderThanTall) {
  // The input bitmap is horizontally long.
  gfx::Canvas canvas(gfx::Size(70, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailTabHelper::ClipResult clip_result = ThumbnailTabHelper::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailTabHelper::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was wider than tall.
  EXPECT_EQ(ThumbnailTabHelper::kWiderThanTall, clip_result);
}

TEST_F(ThumbnailTabHelperTest, GetClippedBitmap_TooWiderThanTall) {
  // The input bitmap is horizontally very long.
  gfx::Canvas canvas(gfx::Size(90, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailTabHelper::ClipResult clip_result = ThumbnailTabHelper::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailTabHelper::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was wider than tall.
  EXPECT_EQ(ThumbnailTabHelper::kTooWiderThanTall, clip_result);
}

TEST_F(ThumbnailTabHelperTest, GetClippedBitmap_NotClipped) {
  // The input bitmap is square.
  gfx::Canvas canvas(gfx::Size(40, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailTabHelper::ClipResult clip_result = ThumbnailTabHelper::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailTabHelper::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // There was no need to clip.
  EXPECT_EQ(ThumbnailTabHelper::kNotClipped, clip_result);
}

TEST_F(ThumbnailTabHelperTest, GetClippedBitmap_NonSquareOutput) {
  // The input bitmap is square.
  gfx::Canvas canvas(gfx::Size(40, 40), ui::SCALE_FACTOR_100P, true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is horizontally long.
  ThumbnailTabHelper::ClipResult clip_result = ThumbnailTabHelper::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailTabHelper::GetClippedBitmap(
      bitmap, 20, 10, &clip_result);
  // The clipped bitmap should have the same aspect ratio of the desired size.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(20, clipped_bitmap.height());
  // The input was taller than wide.
  EXPECT_EQ(ThumbnailTabHelper::kTallerThanWide, clip_result);
}
