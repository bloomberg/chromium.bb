// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/renderer_host/backing_store_manager.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/mock_render_process_host.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/render_view_host_manager.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/surface/transport_dib.h"

static const int kBitmapWidth = 100;
static const int kBitmapHeight = 100;

// TODO(brettw,satorux) enable this when GetThumbnailForBackingStore is
// implemented for Mac in thumbnail_generator.cc
//
// The test fails on Windows for the following error. Figure it out and fix.
//
// transport_dib_win.cc(71)] Check failed: !memory(). Mapped file twice in
// the same process.
//
#if !defined(OS_MACOSX) && !defined(OS_WIN)

// This test render widget host view uses BackingStoreSkia instead of
// TestBackingStore, so that basic operations like CopyFromBackingStore()
// works. The skia implementation doesn't have any hardware or system
// dependencies.
class TestRenderWidgetHostViewWithBackingStoreSkia
    : public TestRenderWidgetHostView {
 public:
  explicit TestRenderWidgetHostViewWithBackingStoreSkia(RenderWidgetHost* rwh)
      : TestRenderWidgetHostView(rwh), rwh_(rwh) {}

  BackingStore* AllocBackingStore(const gfx::Size& size) {
    return new BackingStoreSkia(rwh_, size);
  }

 private:
  RenderWidgetHost* rwh_;
  DISALLOW_COPY_AND_ASSIGN(TestRenderWidgetHostViewWithBackingStoreSkia);
};

class ThumbnailGeneratorTest : public testing::Test {
 public:
  ThumbnailGeneratorTest() {
    profile_.reset(new TestingProfile());
    process_ = new MockRenderProcessHost(profile_.get());
    widget_.reset(new RenderWidgetHost(process_, 1));
    view_.reset(new TestRenderWidgetHostViewWithBackingStoreSkia(
        widget_.get()));
    // Paiting will be skipped if there's no view.
    widget_->SetView(view_.get());

    // Need to send out a create notification for the RWH to get hooked. This is
    // a little scary in that we don't have a RenderView, but the only listener
    // will want a RenderWidget, so it works out OK.
    NotificationService::current()->Notify(
        content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
        Source<RenderViewHostManager>(NULL),
        Details<RenderViewHost>(reinterpret_cast<RenderViewHost*>(
            widget_.get())));

    transport_dib_.reset(TransportDIB::Create(kBitmapWidth * kBitmapHeight * 4,
                                              1));
  }

  ~ThumbnailGeneratorTest() {
    view_.reset();
    widget_.reset();
    process_ = NULL;
    profile_.reset();

    // Process all pending tasks to avoid leaks.
    message_loop_.RunAllPending();
  }

 protected:
  // Indicates what bitmap should be sent with the paint message. _OTHER will
  // only be retrned by CheckFirstPixel if the pixel is none of the others.
  enum TransportType { TRANSPORT_BLACK, TRANSPORT_WHITE, TRANSPORT_OTHER };

  void SendPaint(TransportType type) {
    ViewHostMsg_UpdateRect_Params params;
    params.bitmap_rect = gfx::Rect(0, 0, kBitmapWidth, kBitmapHeight);
    params.view_size = params.bitmap_rect.size();
    params.copy_rects.push_back(params.bitmap_rect);
    params.flags = 0;

    scoped_ptr<skia::PlatformCanvas> canvas(
        transport_dib_->GetPlatformCanvas(kBitmapWidth, kBitmapHeight));
    switch (type) {
      case TRANSPORT_BLACK:
        skia::GetTopDevice(*canvas)->accessBitmap(true).eraseARGB(
            0xFF, 0, 0, 0);
        break;
      case TRANSPORT_WHITE:
        skia::GetTopDevice(*canvas)->accessBitmap(true).eraseARGB(
            0xFF, 0xFF, 0xFF, 0xFF);
        break;
      case TRANSPORT_OTHER:
      default:
        NOTREACHED();
        break;
    }

    params.bitmap = transport_dib_->id();

    ViewHostMsg_UpdateRect msg(1, params);
    widget_->OnMessageReceived(msg);
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

  scoped_ptr<TestingProfile> profile_;

  // Deleted automatically by widget_, but the deletion is done by
  // DeleteSoon(), hence the message loop needs to run pending tasks.
  MockRenderProcessHost* process_;

  scoped_ptr<RenderWidgetHost> widget_;
  scoped_ptr<TestRenderWidgetHostViewWithBackingStoreSkia> view_;
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
  SkBitmap result = generator_.GetThumbnailForRenderer(widget_.get());
  EXPECT_TRUE(result.isNull());
}

// Tests basic thumbnail generation when a backing store is discarded.
TEST_F(ThumbnailGeneratorTest, DiscardBackingStore) {
  // First set up a backing store.
  SendPaint(TRANSPORT_BLACK);
  ASSERT_TRUE(widget_->GetBackingStore(false));

  // The thumbnail generator should be able to retrieve a thumbnail.
  SkBitmap result = generator_.GetThumbnailForRenderer(widget_.get());
  ASSERT_FALSE(result.isNull());
  // Valgrind reports MemoryCheck::Cond inside ClassifyFirstPixel(). With
  // --track-origins=yes, valgrind reports that the uninitialized value
  // originates from SkBitmap created in BackingStoreSkia's constructor.
  // However, the bitmap is set properly when we send bitmap data in
  // SendPaint() using TransportDIB (the test fails if the bitmap does not
  // starts with 0xFF, 0x00, 0x00, 0x00, which is unlikely to happen by
  // accident). Valgrind doesn't seem to be able to track data transfer
  // from TransportDIB probably because the data comes from shared
  // memory. This can explain why valgrind wrongly reports that the value
  // in the bitmap is uninitialized. See also crbug.com/80458.
  EXPECT_EQ(TRANSPORT_BLACK, ClassifyFirstPixel(result));

  // Discard the backing store.
  ASSERT_TRUE(BackingStoreManager::ExpireBackingStoreForTest(widget_.get()));
  ASSERT_FALSE(widget_->GetBackingStore(false));

  // The thumbnail generator should not be able to retrieve a thumbnail,
  // as the backing store is now gone.
  result = generator_.GetThumbnailForRenderer(widget_.get());
  ASSERT_TRUE(result.isNull());
}

#endif  // !defined(OS_MAC)

typedef testing::Test ThumbnailGeneratorSimpleTest;

TEST_F(ThumbnailGeneratorSimpleTest, CalculateBoringScore_Empty) {
  SkBitmap bitmap;
  EXPECT_DOUBLE_EQ(1.0, ThumbnailGenerator::CalculateBoringScore(&bitmap));
}

TEST_F(ThumbnailGeneratorSimpleTest, CalculateBoringScore_SingleColor) {
  const SkColor kBlack = SkColorSetRGB(0, 0, 0);
  const gfx::Size kSize(20, 10);
  gfx::CanvasSkia canvas(kSize.width(), kSize.height(), true);
  // Fill all pixesl in black.
  canvas.FillRectInt(kBlack, 0, 0, kSize.width(), kSize.height());

  SkBitmap bitmap = skia::GetTopDevice(canvas)->accessBitmap(false);
  // The thumbnail should deserve the highest boring score.
  EXPECT_DOUBLE_EQ(1.0, ThumbnailGenerator::CalculateBoringScore(&bitmap));
}

TEST_F(ThumbnailGeneratorSimpleTest, CalculateBoringScore_TwoColors) {
  const SkColor kBlack = SkColorSetRGB(0, 0, 0);
  const SkColor kWhite = SkColorSetRGB(0xFF, 0xFF, 0xFF);
  const gfx::Size kSize(20, 10);

  gfx::CanvasSkia canvas(kSize.width(), kSize.height(), true);
  // Fill all pixesl in black.
  canvas.FillRectInt(kBlack, 0, 0, kSize.width(), kSize.height());
  // Fill the left half pixels in white.
  canvas.FillRectInt(kWhite, 0, 0, kSize.width() / 2, kSize.height());

  SkBitmap bitmap = skia::GetTopDevice(canvas)->accessBitmap(false);
  ASSERT_EQ(kSize.width(), bitmap.width());
  ASSERT_EQ(kSize.height(), bitmap.height());
  // The thumbnail should be less boring because two colors are used.
  EXPECT_DOUBLE_EQ(0.5, ThumbnailGenerator::CalculateBoringScore(&bitmap));
}

TEST_F(ThumbnailGeneratorSimpleTest, GetClippedBitmap_TallerThanWide) {
  // The input bitmap is vertically long.
  gfx::CanvasSkia canvas(40, 90, true);
  const SkBitmap bitmap = skia::GetTopDevice(canvas)->accessBitmap(false);

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

TEST_F(ThumbnailGeneratorSimpleTest, GetClippedBitmap_WiderThanTall) {
  // The input bitmap is horizontally long.
  gfx::CanvasSkia canvas(90, 40, true);
  const SkBitmap bitmap = skia::GetTopDevice(canvas)->accessBitmap(false);

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

TEST_F(ThumbnailGeneratorSimpleTest, GetClippedBitmap_NotClipped) {
  // The input bitmap is square.
  gfx::CanvasSkia canvas(40, 40, true);
  const SkBitmap bitmap = skia::GetTopDevice(canvas)->accessBitmap(false);

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

TEST_F(ThumbnailGeneratorSimpleTest, GetClippedBitmap_NonSquareOutput) {
  // The input bitmap is square.
  gfx::CanvasSkia canvas(40, 40, true);
  const SkBitmap bitmap = skia::GetTopDevice(canvas)->accessBitmap(false);

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

TEST_F(ThumbnailGeneratorSimpleTest, ShouldUpdateThumbnail) {
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
