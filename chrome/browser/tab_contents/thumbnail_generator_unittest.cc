// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/testing_profile.h"
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

typedef testing::Test ThumbnailGeneratorTest;

TEST_F(ThumbnailGeneratorTest, CalculateBoringScore_Empty) {
  SkBitmap bitmap;
  EXPECT_DOUBLE_EQ(1.0, ThumbnailGenerator::CalculateBoringScore(bitmap));
}

TEST_F(ThumbnailGeneratorTest, CalculateBoringScore_SingleColor) {
  const gfx::Size kSize(20, 10);
  gfx::Canvas canvas(kSize, true);
  // Fill all pixesl in black.
  canvas.FillRect(gfx::Rect(kSize), SK_ColorBLACK);

  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  // The thumbnail should deserve the highest boring score.
  EXPECT_DOUBLE_EQ(1.0, ThumbnailGenerator::CalculateBoringScore(bitmap));
}

TEST_F(ThumbnailGeneratorTest, CalculateBoringScore_TwoColors) {
  const gfx::Size kSize(20, 10);

  gfx::Canvas canvas(kSize, true);
  // Fill all pixesl in black.
  canvas.FillRect(gfx::Rect(kSize), SK_ColorBLACK);
  // Fill the left half pixels in white.
  canvas.FillRect(gfx::Rect(0, 0, kSize.width() / 2, kSize.height()),
                  SK_ColorWHITE);

  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  ASSERT_EQ(kSize.width(), bitmap.width());
  ASSERT_EQ(kSize.height(), bitmap.height());
  // The thumbnail should be less boring because two colors are used.
  EXPECT_DOUBLE_EQ(0.5, ThumbnailGenerator::CalculateBoringScore(bitmap));
}

TEST_F(ThumbnailGeneratorTest, GetClippedBitmap_TallerThanWide) {
  // The input bitmap is vertically long.
  gfx::Canvas canvas(gfx::Size(40, 90), true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailGenerator::ClipResult clip_result = ThumbnailGenerator::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was taller than wide.
  EXPECT_EQ(ThumbnailGenerator::kTallerThanWide, clip_result);
}

TEST_F(ThumbnailGeneratorTest, GetClippedBitmap_WiderThanTall) {
  // The input bitmap is horizontally long.
  gfx::Canvas canvas(gfx::Size(70, 40), true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailGenerator::ClipResult clip_result = ThumbnailGenerator::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was wider than tall.
  EXPECT_EQ(ThumbnailGenerator::kWiderThanTall, clip_result);
}

TEST_F(ThumbnailGeneratorTest, GetClippedBitmap_TooWiderThanTall) {
  // The input bitmap is horizontally very long.
  gfx::Canvas canvas(gfx::Size(90, 40), true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailGenerator::ClipResult clip_result = ThumbnailGenerator::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // The input was wider than tall.
  EXPECT_EQ(ThumbnailGenerator::kTooWiderThanTall, clip_result);
}

TEST_F(ThumbnailGeneratorTest, GetClippedBitmap_NotClipped) {
  // The input bitmap is square.
  gfx::Canvas canvas(gfx::Size(40, 40), true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is square.
  ThumbnailGenerator::ClipResult clip_result = ThumbnailGenerator::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
      bitmap, 10, 10, &clip_result);
  // The clipped bitmap should be square.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(40, clipped_bitmap.height());
  // There was no need to clip.
  EXPECT_EQ(ThumbnailGenerator::kNotClipped, clip_result);
}

TEST_F(ThumbnailGeneratorTest, GetClippedBitmap_NonSquareOutput) {
  // The input bitmap is square.
  gfx::Canvas canvas(gfx::Size(40, 40), true);
  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // The desired size is horizontally long.
  ThumbnailGenerator::ClipResult clip_result = ThumbnailGenerator::kNotClipped;
  SkBitmap clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
      bitmap, 20, 10, &clip_result);
  // The clipped bitmap should have the same aspect ratio of the desired size.
  EXPECT_EQ(40, clipped_bitmap.width());
  EXPECT_EQ(20, clipped_bitmap.height());
  // The input was taller than wide.
  EXPECT_EQ(ThumbnailGenerator::kTallerThanWide, clip_result);
}

// A mock version of TopSites, used for testing ShouldUpdateThumbnail().
class MockTopSites : public history::TopSites {
 public:
  explicit MockTopSites(Profile* profile)
      : history::TopSites(profile),
        capacity_(1) {
  }

  // history::TopSites overrides.
  virtual bool IsFull() {
    return known_url_map_.size() >= capacity_;
  }
  virtual bool IsKnownURL(const GURL& url) {
    return known_url_map_.find(url.spec()) != known_url_map_.end();
  }
  virtual bool GetPageThumbnailScore(const GURL& url, ThumbnailScore* score) {
    std::map<std::string, ThumbnailScore>::const_iterator iter =
        known_url_map_.find(url.spec());
    if (iter == known_url_map_.end()) {
      return false;
    } else {
      *score = iter->second;
      return true;
    }
  }

  // Adds a known URL with the associated thumbnail score.
  void AddKnownURL(const GURL& url, const ThumbnailScore& score) {
    known_url_map_[url.spec()] = score;
  }

 private:
  virtual ~MockTopSites() {}
  size_t capacity_;
  std::map<std::string, ThumbnailScore> known_url_map_;
};

TEST_F(ThumbnailGeneratorTest, ShouldUpdateThumbnail) {
  const GURL kGoodURL("http://www.google.com/");
  const GURL kBadURL("chrome://newtab");

  // Set up the profile.
  TestingProfile profile;

  // Set up the top sites service.
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<MockTopSites> top_sites(new MockTopSites(&profile));

  // Should be false because it's a bad URL.
  EXPECT_FALSE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kBadURL));

  // Should be true, as it's a good URL.
  EXPECT_TRUE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kGoodURL));

  // Should be false, if it's in the incognito mode.
  profile.set_incognito(true);
  EXPECT_FALSE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kGoodURL));

  // Should be true again, once turning off the incognito mode.
  profile.set_incognito(false);
  EXPECT_TRUE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kGoodURL));

  // Add a known URL. This makes the top sites data full.
  ThumbnailScore bad_score;
  bad_score.time_at_snapshot = base::Time::UnixEpoch();  // Ancient time stamp.
  top_sites->AddKnownURL(kGoodURL, bad_score);
  ASSERT_TRUE(top_sites->IsFull());

  // Should be false, as the top sites data is full, and the new URL is
  // not known.
  const GURL kAnotherGoodURL("http://www.youtube.com/");
  EXPECT_FALSE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kAnotherGoodURL));

  // Should be true, as the existing thumbnail is bad (i.e need a better one).
  EXPECT_TRUE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kGoodURL));

  // Replace the thumbnail score with a really good one.
  ThumbnailScore good_score;
  good_score.time_at_snapshot = base::Time::Now();  // Very new.
  good_score.at_top = true;
  good_score.good_clipping = true;
  good_score.boring_score = 0.0;
  good_score.load_completed = true;
  top_sites->AddKnownURL(kGoodURL, good_score);

  // Should be false, as the existing thumbnail is good enough (i.e. don't
  // need to replace the existing thumbnail which is new and good).
  EXPECT_FALSE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kGoodURL));
}
