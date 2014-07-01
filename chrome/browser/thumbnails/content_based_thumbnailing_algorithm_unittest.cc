// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/thumbnails/content_based_thumbnailing_algorithm.h"
#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scrollbar_size.h"

namespace thumbnails {

typedef testing::Test ContentBasedThumbnailingAlgorithmTest;

class ConsumerCallbackCatcher {
 public:
  ConsumerCallbackCatcher()
      : called_back_(false), clip_result_(CLIP_RESULT_UNPROCESSED) {
  }

  void UiThreadCallback(const ThumbnailingContext& context,
                        const SkBitmap& bitmap) {
    called_back_ = true;
    captured_bitmap_ = bitmap;
    clip_result_ = context.clip_result;
    score_ = context.score;
  }

  bool called_back() const {
    return called_back_;
  }

  const SkBitmap& captured_bitmap() const {
    return captured_bitmap_;
  }

  ClipResult clip_result() const {
    return clip_result_;
  }

  const ThumbnailScore& score() const {
    return score_;
  }

 private:
  SkBitmap captured_bitmap_;
  bool called_back_;
  ClipResult clip_result_;
  ThumbnailScore score_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerCallbackCatcher);
};

TEST_F(ContentBasedThumbnailingAlgorithmTest, GetCanvasCopyInfo) {
  // We will want to use the entirety of the image as the source. Usually,
  // an image in its original size should be requested, except for reakky large
  // canvas. In that case, image will be shrunk but wit aspect ratio preserved.
  const gfx::Size thumbnail_size(312, 165);
  scoped_refptr<ThumbnailingAlgorithm> algorithm(
      new ContentBasedThumbnailingAlgorithm(thumbnail_size));

  gfx::Rect clipping_rect;
  gfx::Size target_size;
  gfx::Size source_size(1000, 600);

  ClipResult clip_result = algorithm->GetCanvasCopyInfo(
      source_size, ui::SCALE_FACTOR_100P, &clipping_rect, &target_size);
  EXPECT_EQ(CLIP_RESULT_SOURCE_SAME_AS_TARGET, clip_result);
  EXPECT_EQ(source_size.ToString(), clipping_rect.size().ToString());
  EXPECT_EQ(gfx::Point(0, 0).ToString(), clipping_rect.origin().ToString());
  EXPECT_EQ(source_size, target_size);

  source_size.SetSize(6000, 3000);
  clip_result = algorithm->GetCanvasCopyInfo(
      source_size, ui::SCALE_FACTOR_100P, &clipping_rect, &target_size);
  EXPECT_EQ(CLIP_RESULT_NOT_CLIPPED, clip_result);
  EXPECT_EQ(source_size.ToString(), clipping_rect.size().ToString());
  EXPECT_EQ(gfx::Point(0, 0).ToString(), clipping_rect.origin().ToString());
  EXPECT_LT(target_size.width(), source_size.width());
  EXPECT_LT(target_size.height(), source_size.height());
  EXPECT_NEAR(static_cast<float>(target_size.width()) / target_size.height(),
              static_cast<float>(source_size.width()) / source_size.height(),
              0.1f);
  source_size.SetSize(300, 200);
  clip_result = algorithm->GetCanvasCopyInfo(
      source_size, ui::SCALE_FACTOR_100P, &clipping_rect, &target_size);
  EXPECT_EQ(CLIP_RESULT_SOURCE_IS_SMALLER, clip_result);
  EXPECT_EQ(clipping_rect.size().ToString(),
            SimpleThumbnailCrop::GetCopySizeForThumbnail(
                ui::SCALE_FACTOR_100P, thumbnail_size).ToString());
  EXPECT_EQ(gfx::Point(0, 0).ToString(), clipping_rect.origin().ToString());
}

TEST_F(ContentBasedThumbnailingAlgorithmTest, PrepareSourceBitmap) {
  const gfx::Size thumbnail_size(312, 165);
  const gfx::Size copy_size(400, 200);
  scoped_refptr<ThumbnailingContext> context(
      ThumbnailingContext::CreateThumbnailingContextForTest());
  context->requested_copy_size = copy_size;

  // This calls for exercising two distinct paths: with prior clipping and
  // without.
  SkBitmap source;
  source.allocN32Pixels(800, 600);
  source.eraseARGB(255, 50, 150, 200);
  SkBitmap result = ContentBasedThumbnailingAlgorithm::PrepareSourceBitmap(
      source, thumbnail_size, context.get());
  EXPECT_EQ(CLIP_RESULT_SOURCE_SAME_AS_TARGET, context->clip_result);
  EXPECT_GE(result.width(), copy_size.width());
  EXPECT_GE(result.height(), copy_size.height());
  EXPECT_LT(result.width(), source.width());
  EXPECT_LT(result.height(), source.height());
  // The check below is a bit of a side effect: since the image was clipped
  // by scrollbar_size, it cannot be shrunk and thus what we get below is
  // true.
  EXPECT_NEAR(result.width(), source.width(), gfx::scrollbar_size());
  EXPECT_NEAR(result.height(), source.height(), gfx::scrollbar_size());

  result = ContentBasedThumbnailingAlgorithm::PrepareSourceBitmap(
      source, thumbnail_size, context.get());
  EXPECT_EQ(CLIP_RESULT_SOURCE_SAME_AS_TARGET, context->clip_result);
  EXPECT_GE(result.width(), copy_size.width());
  EXPECT_GE(result.height(), copy_size.height());
  EXPECT_LT(result.width(), source.width());
  EXPECT_LT(result.height(), source.height());
}

TEST_F(ContentBasedThumbnailingAlgorithmTest, CreateRetargetedThumbnail) {
  // This tests the invocation of the main thumbnail-making apparatus.
  // The actual content is not really of concern here, just check the plumbing.
  const gfx::Size image_size(1200, 800);
  gfx::Canvas canvas(image_size, 1.0f, true);

  // The image consists of vertical non-overlapping stripes 150 pixels wide.
  canvas.FillRect(gfx::Rect(200, 200, 800, 400), SkColorSetRGB(255, 255, 255));
  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  ConsumerCallbackCatcher catcher;
  const gfx::Size thumbnail_size(432, 284);
  scoped_refptr<ThumbnailingContext> context(
      ThumbnailingContext::CreateThumbnailingContextForTest());
  context->requested_copy_size = image_size;
  context->clip_result = CLIP_RESULT_SOURCE_SAME_AS_TARGET;

  base::MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  ContentBasedThumbnailingAlgorithm::CreateRetargetedThumbnail(
      source,
      thumbnail_size,
      context,
      base::Bind(&ConsumerCallbackCatcher::UiThreadCallback,
                 base::Unretained(&catcher)));
  message_loop.RunUntilIdle();
  ASSERT_TRUE(catcher.called_back());
  EXPECT_TRUE(catcher.score().good_clipping);
  EXPECT_FALSE(catcher.captured_bitmap().empty());
  EXPECT_LT(catcher.captured_bitmap().width(), source.width());
  EXPECT_LT(catcher.captured_bitmap().height(), source.height());
}

}  // namespace thumbnails
