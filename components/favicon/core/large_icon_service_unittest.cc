// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service.h"

#include <deque>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace favicon {
namespace {

using testing::_;

const char kDummyUrl[] = "http://www.example.com";
const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";
const SkColor kTestColor = SK_ColorRED;

favicon_base::FaviconRawBitmapResult CreateTestBitmap(
    int w, int h, SkColor color) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  // Create bitmap and fill with |color|.
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(w, h);
  bitmap.eraseColor(color);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());
  result.bitmap_data = data;

  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::TOUCH_ICON;
  CHECK(result.is_valid());
  return result;
}

class LargeIconServiceTest : public testing::Test {
 public:
  LargeIconServiceTest()
      : large_icon_service_(&mock_favicon_service_,
                            base::ThreadTaskRunnerHandle::Get()),
        is_callback_invoked_(false) {}

  ~LargeIconServiceTest() override {
  }

  void ResultCallback(const favicon_base::LargeIconResult& result) {
    is_callback_invoked_ = true;

    // Checking presence and absence of results.
    EXPECT_EQ(expected_bitmap_.is_valid(), result.bitmap.is_valid());
    EXPECT_EQ(expected_fallback_icon_style_ != nullptr,
              result.fallback_icon_style != nullptr);

    if (expected_bitmap_.is_valid()) {
      EXPECT_EQ(expected_bitmap_.pixel_size, result.bitmap.pixel_size);
      // Not actually checking bitmap content.
    }
    if (expected_fallback_icon_style_.get()) {
      EXPECT_EQ(*expected_fallback_icon_style_,
                *result.fallback_icon_style);
    }
  }

  void InjectMockResult(
      const GURL& page_url,
      const favicon_base::FaviconRawBitmapResult& mock_result) {
    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(page_url, _, _, _, _))
        .WillOnce(PostReply<5>(mock_result));
  }

 protected:
  base::MessageLoopForIO loop_;

  testing::StrictMock<MockFaviconService> mock_favicon_service_;
  LargeIconService large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;

  favicon_base::FaviconRawBitmapResult expected_bitmap_;
  std::unique_ptr<favicon_base::FallbackIconStyle>
      expected_fallback_icon_style_;

  bool is_callback_invoked_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LargeIconServiceTest);
};

TEST_F(LargeIconServiceTest, SameSize) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmap(24, 24, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      24,  // |min_source_size_in_pixel|
      24,  // |desired_size_in_pixel|
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, ScaleDown) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmap(32, 32, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl), 24, 24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, ScaleUp) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmap(16, 16, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      14,  // Lowered requirement so stored bitmap is admitted.
      24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

// |desired_size_in_pixel| == 0 means retrieve original image without scaling.
TEST_F(LargeIconServiceTest, NoScale) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmap(24, 24, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl), 16, 0,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconTooSmall) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmap(16, 16, kTestColor));
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  expected_fallback_icon_style_->background_color = kTestColor;
  expected_fallback_icon_style_->is_default_background_color = false;
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl), 24, 24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconNotSquare) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmap(24, 32, kTestColor));
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  expected_fallback_icon_style_->background_color = kTestColor;
  expected_fallback_icon_style_->is_default_background_color = false;
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl), 24, 24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconMissing) {
  InjectMockResult(GURL(kDummyUrl), favicon_base::FaviconRawBitmapResult());
  // Expect default fallback style, including background.
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl), 24, 24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconMissingNoScale) {
  InjectMockResult(GURL(kDummyUrl), favicon_base::FaviconRawBitmapResult());
  // Expect default fallback style, including background.
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl), 24, 0,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

// Oddball case where we demand a high resolution icon to scale down. Generates
// fallback even though an icon with the final size is available.
TEST_F(LargeIconServiceTest, FallbackSinceTooPicky) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmap(24, 24, kTestColor));
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  expected_fallback_icon_style_->background_color = kTestColor;
  expected_fallback_icon_style_->is_default_background_color = false;
  large_icon_service_.GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl), 32, 24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

}  // namespace
}  // namespace favicon
