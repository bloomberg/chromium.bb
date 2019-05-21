// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_request_handler.h"

#include "base/bind.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/mock_callback.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_request_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace favicon {
namespace {

using testing::_;
using testing::Invoke;

const char kDummyPageUrl[] = "https://www.example.com";
const int kDesiredSizeInPixel = 16;
const SkColor kTestColor = SK_ColorRED;
base::CancelableTaskTracker::TaskId kDummyTaskId = 1;
const FaviconRequestOrigin kOrigin = FaviconRequestOrigin::UNKNOWN;

scoped_refptr<base::RefCountedBytes> CreateTestBitmapBytes() {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(kTestColor);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());
  return data;
}

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult() {
  favicon_base::FaviconRawBitmapResult result;
  result.bitmap_data = CreateTestBitmapBytes();
  return result;
}

favicon_base::FaviconImageResult CreateTestImageResult() {
  favicon_base::FaviconImageResult result;
  result.image = gfx::Image::CreateFrom1xPNGBytes(CreateTestBitmapBytes());
  return result;
}

ACTION_P(ReturnFaviconFromSync, should_return_valid) {
  if (should_return_valid) {
    *arg1 = CreateTestBitmapBytes();
  }
  return should_return_valid;
}

void StoreBitmap(favicon_base::FaviconRawBitmapResult* destination,
                 const favicon_base::FaviconRawBitmapResult& result) {
  *destination = result;
}

void StoreImage(favicon_base::FaviconImageResult* destination,
                const favicon_base::FaviconImageResult& result) {
  *destination = result;
}

class FaviconRequestHandlerTest : public ::testing::Test {
 public:
  FaviconRequestHandlerTest() = default;

 protected:
  testing::NiceMock<MockFaviconService> mock_favicon_service_;
  FaviconRequestHandler favicon_request_handler_;
  base::MockCallback<FaviconRequestHandler::SyncedFaviconGetter>
      synced_favicon_getter_;
  base::CancelableTaskTracker tracker_;
};

TEST_F(FaviconRequestHandlerTest, ShouldGetEmptyBitmap) {
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl), _))
      .WillOnce(ReturnFaviconFromSync(/*should_return_valid=*/false));
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindRepeating(&StoreBitmap, &result), kOrigin,
      &mock_favicon_service_, synced_favicon_getter_.Get(), &tracker_);
  EXPECT_FALSE(result.is_valid());
}

TEST_F(FaviconRequestHandlerTest, ShouldGetSyncBitmap) {
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl), _))
      .WillOnce(ReturnFaviconFromSync(/*should_return_valid=*/true));
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindRepeating(&StoreBitmap, &result), kOrigin,
      &mock_favicon_service_, synced_favicon_getter_.Get(), &tracker_);
  EXPECT_TRUE(result.is_valid());
}

TEST_F(FaviconRequestHandlerTest, ShouldGetLocalBitmap) {
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(CreateTestBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl), _)).Times(0);
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindRepeating(&StoreBitmap, &result), kOrigin,
      &mock_favicon_service_, synced_favicon_getter_.Get(), &tracker_);
  EXPECT_TRUE(result.is_valid());
}

TEST_F(FaviconRequestHandlerTest, ShouldGetEmptyImage) {
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl), _))
      .WillOnce(ReturnFaviconFromSync(/*should_return_valid=*/false));
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindRepeating(&StoreImage, &result), kOrigin,
      &mock_favicon_service_, synced_favicon_getter_.Get(), &tracker_);
  EXPECT_TRUE(result.image.IsEmpty());
}

TEST_F(FaviconRequestHandlerTest, ShouldGetSyncImage) {
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl), _))
      .WillOnce(ReturnFaviconFromSync(/*should_return_valid=*/true));
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindRepeating(&StoreImage, &result), kOrigin,
      &mock_favicon_service_, synced_favicon_getter_.Get(), &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
}

TEST_F(FaviconRequestHandlerTest, ShouldGetLocalImage) {
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(CreateTestImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl), _)).Times(0);
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindRepeating(&StoreImage, &result), kOrigin,
      &mock_favicon_service_, synced_favicon_getter_.Get(), &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
}

}  // namespace
}  // namespace favicon
