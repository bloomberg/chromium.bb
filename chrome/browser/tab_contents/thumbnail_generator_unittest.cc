// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/surface/transport_dib.h"
#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/testing_profile.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/canvas_skia.h"

static const int kBitmapWidth = 100;
static const int kBitmapHeight = 100;

// TODO(brettw) enable this when GetThumbnailForBackingStore is implemented
// for other platforms in thumbnail_generator.cc
// #if defined(OS_WIN)
// TODO(brettw) enable this on Windows after we clobber a build to see if the
// failures of this on the buildbot can be resolved.
#if 0

class ThumbnailGeneratorTest : public testing::Test {
 public:
  ThumbnailGeneratorTest()
      : profile_(),
        process_(new MockRenderProcessHost(&profile_)),
        widget_(process_, 1),
        view_(&widget_) {
    // Paiting will be skipped if there's no view.
    widget_.set_view(&view_);

    // Need to send out a create notification for the RWH to get hooked. This is
    // a little scary in that we don't have a RenderView, but the only listener
    // will want a RenderWidget, so it works out OK.
    NotificationService::current()->Notify(
        NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
        Source<RenderViewHostManager>(NULL),
        Details<RenderViewHost>(reinterpret_cast<RenderViewHost*>(&widget_)));

    transport_dib_.reset(TransportDIB::Create(kBitmapWidth * kBitmapHeight * 4,
                                              1));

    // We don't want to be sensitive to timing.
    generator_.StartThumbnailing();
    generator_.set_no_timeout(true);
  }

 protected:
  // Indicates what bitmap should be sent with the paint message. _OTHER will
  // only be retrned by CheckFirstPixel if the pixel is none of the others.
  enum TransportType { TRANSPORT_BLACK, TRANSPORT_WHITE, TRANSPORT_OTHER };

  void SendPaint(TransportType type) {
    ViewHostMsg_PaintRect_Params params;
    params.bitmap_rect = gfx::Rect(0, 0, kBitmapWidth, kBitmapHeight);
    params.view_size = params.bitmap_rect.size();
    params.flags = 0;

    scoped_ptr<skia::PlatformCanvas> canvas(
        transport_dib_->GetPlatformCanvas(kBitmapWidth, kBitmapHeight));
    switch (type) {
      case TRANSPORT_BLACK:
        canvas->getTopPlatformDevice().accessBitmap(true).eraseARGB(
            0xFF, 0, 0, 0);
        break;
      case TRANSPORT_WHITE:
        canvas->getTopPlatformDevice().accessBitmap(true).eraseARGB(
            0xFF, 0xFF, 0xFF, 0xFF);
        break;
      case TRANSPORT_OTHER:
      default:
        NOTREACHED();
        break;
    }

    params.bitmap = transport_dib_->id();

    ViewHostMsg_PaintRect msg(1, params);
    widget_.OnMessageReceived(msg);
  }

  TransportType ClassifyFirstPixel(const SkBitmap& bitmap) {
    // Returns the color of the first pixel of the bitmap. The bitmap must be
    // non-empty.
    SkAutoLockPixels lock(bitmap);
    uint32 pixel = *bitmap.getAddr32(0, 0);

    if (SkGetPackedA32(pixel) != 0xFF)
      return TRANSPORT_OTHER;  // All values expect an opqaue alpha channel

    if (SkGetPackedR32(pixel) == 0 &&
        SkGetPackedG32(pixel) == 0 &&
        SkGetPackedB32(pixel) == 0)
      return TRANSPORT_BLACK;

    if (SkGetPackedR32(pixel) == 0xFF &&
        SkGetPackedG32(pixel) == 0xFF &&
        SkGetPackedB32(pixel) == 0xFF)
      return TRANSPORT_WHITE;

    EXPECT_TRUE(false) << "Got weird color: " << pixel;
    return TRANSPORT_OTHER;
  }

  MessageLoopForUI message_loop_;

  TestingProfile profile_;

  // This will get deleted when the last RHWH associated with it is destroyed.
  MockRenderProcessHost* process_;

  RenderWidgetHost widget_;
  TestRenderWidgetHostView view_;
  ThumbnailGenerator generator_;

  scoped_ptr<TransportDIB> transport_dib_;

 private:
  // testing::Test implementation.
  void SetUp() {
  }
  void TearDown() {
  }
};

TEST_F(ThumbnailGeneratorTest, NoThumbnail) {
  // This is the case where there is no thumbnail available on the tab and
  // there is no backing store. There should be no image returned.
  SkBitmap result = generator_.GetThumbnailForRenderer(&widget_);
  EXPECT_TRUE(result.isNull());
}

// Tests basic thumbnail generation when a backing store is discarded.
TEST_F(ThumbnailGeneratorTest, DiscardBackingStore) {
  // First set up a backing store and then discard it.
  SendPaint(TRANSPORT_BLACK);
  widget_.WasHidden();
  ASSERT_TRUE(BackingStoreManager::ExpireBackingStoreForTest(&widget_));
  ASSERT_FALSE(widget_.GetBackingStore(false, false));

  // The thumbnail generator should have stashed a thumbnail of the page.
  SkBitmap result = generator_.GetThumbnailForRenderer(&widget_);
  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(TRANSPORT_BLACK, ClassifyFirstPixel(result));
}

TEST_F(ThumbnailGeneratorTest, QuickShow) {
  // Set up a hidden widget with a black cached thumbnail and an expired
  // backing store.
  SendPaint(TRANSPORT_BLACK);
  widget_.WasHidden();
  ASSERT_TRUE(BackingStoreManager::ExpireBackingStoreForTest(&widget_));
  ASSERT_FALSE(widget_.GetBackingStore(false, false));

  // Now show the widget and paint white.
  widget_.WasRestored();
  SendPaint(TRANSPORT_WHITE);

  // The black thumbnail should still be cached because it hasn't processed the
  // timer message yet.
  SkBitmap result = generator_.GetThumbnailForRenderer(&widget_);
  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(TRANSPORT_BLACK, ClassifyFirstPixel(result));

  // Running the message loop will process the timer, which should expire the
  // cached thumbnail. Asking again should give us a new one computed from the
  // backing store.
  message_loop_.RunAllPending();
  result = generator_.GetThumbnailForRenderer(&widget_);
  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(TRANSPORT_WHITE, ClassifyFirstPixel(result));
}

#endif

TEST(ThumbnailGeneratorSimpleTest, CalculateBoringScore_Empty) {
  SkBitmap bitmap;
  EXPECT_DOUBLE_EQ(1.0, ThumbnailGenerator::CalculateBoringScore(&bitmap));
}

TEST(ThumbnailGeneratorSimpleTest, CalculateBoringScore_SingleColor) {
  const SkColor kBlack = SkColorSetRGB(0, 0, 0);
  const gfx::Size kSize(20, 10);
  gfx::CanvasSkia canvas(kSize.width(), kSize.height(), true);
  // Fill all pixesl in black.
  canvas.FillRectInt(kBlack, 0, 0, kSize.width(), kSize.height());

  SkBitmap bitmap = canvas.getTopPlatformDevice().accessBitmap(false);
  // The thumbnail should deserve the highest boring score.
  EXPECT_DOUBLE_EQ(1.0, ThumbnailGenerator::CalculateBoringScore(&bitmap));
}

TEST(ThumbnailGeneratorSimpleTest, CalculateBoringScore_TwoColors) {
  const SkColor kBlack = SkColorSetRGB(0, 0, 0);
  const SkColor kWhite = SkColorSetRGB(0xFF, 0xFF, 0xFF);
  const gfx::Size kSize(20, 10);

  gfx::CanvasSkia canvas(kSize.width(), kSize.height(), true);
  // Fill all pixesl in black.
  canvas.FillRectInt(kBlack, 0, 0, kSize.width(), kSize.height());
  // Fill the left half pixels in white.
  canvas.FillRectInt(kWhite, 0, 0, kSize.width() / 2, kSize.height());

  SkBitmap bitmap = canvas.getTopPlatformDevice().accessBitmap(false);
  ASSERT_EQ(kSize.width(), bitmap.width());
  ASSERT_EQ(kSize.height(), bitmap.height());
  // The thumbnail should be less boring because two colors are used.
  EXPECT_DOUBLE_EQ(0.5, ThumbnailGenerator::CalculateBoringScore(&bitmap));
}

TEST(ThumbnailGeneratorSimpleTest, GetClippedBitmap_TallerThanWide) {
  // The input bitmap is vertically long.
  gfx::CanvasSkia canvas(40, 90, true);
  const SkBitmap bitmap = canvas.getTopPlatformDevice().accessBitmap(false);

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

TEST(ThumbnailGeneratorSimpleTest, GetClippedBitmap_WiderThanTall) {
  // The input bitmap is horizontally long.
  gfx::CanvasSkia canvas(90, 40, true);
  const SkBitmap bitmap = canvas.getTopPlatformDevice().accessBitmap(false);

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

TEST(ThumbnailGeneratorSimpleTest, GetClippedBitmap_NotClipped) {
  // The input bitmap is square.
  gfx::CanvasSkia canvas(40, 40, true);
  const SkBitmap bitmap = canvas.getTopPlatformDevice().accessBitmap(false);

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

TEST(ThumbnailGeneratorSimpleTest, GetClippedBitmap_NonSquareOutput) {
  // The input bitmap is square.
  gfx::CanvasSkia canvas(40, 40, true);
  const SkBitmap bitmap = canvas.getTopPlatformDevice().accessBitmap(false);

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
  MockTopSites(Profile* profile)
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

TEST(ThumbnailGeneratorSimpleTest, ShouldUpdateThumbnail) {
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

  // Should be false, if it's in the off-the-record mode.
  profile.set_off_the_record(true);
  EXPECT_FALSE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kGoodURL));

  // Should be true again, once turning off the off-the-record mode.
  profile.set_off_the_record(false);
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
  top_sites->AddKnownURL(kGoodURL, good_score);

  // Should be false, as the existing thumbnail is good enough (i.e. don't
  // need to replace the existing thumbnail which is new and good).
  EXPECT_FALSE(ThumbnailGenerator::ShouldUpdateThumbnail(
      &profile, top_sites.get(), kGoodURL));
}
