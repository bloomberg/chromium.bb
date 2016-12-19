// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/favicon_view_provider.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "skia/ext/skia_utils_ios.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

@interface FaviconViewProvider (Testing)
@property(nonatomic, retain) UIImage* favicon;
@property(nonatomic, copy) NSString* fallbackText;
@property(nonatomic, retain) UIColor* fallbackBackgroundColor;
@property(nonatomic, retain) UIColor* fallbackTextColor;
@end

namespace {

// Dummy URL for the favicon case.
const char kTestFaviconURL[] = "http://test/favicon";
// Dummy URL for the fallback case.
const char kTestFallbackURL[] = "http://test/fallback";
// Dummy icon URL.
const char kTestFaviconIconURL[] = "http://test/icons/favicon";

// Size of dummy favicon image.
const CGFloat kTestFaviconSize = 57;

// Returns a dummy bitmap result for favicon test URL, and an empty result
// otherwise.
favicon_base::FaviconRawBitmapResult CreateTestBitmap(const GURL& url) {
  favicon_base::FaviconRawBitmapResult result;
  if (url == GURL(kTestFaviconURL)) {
    result.expired = false;
    base::FilePath favicon_path;
    PathService::Get(ios::DIR_TEST_DATA, &favicon_path);
    favicon_path =
        favicon_path.Append(FILE_PATH_LITERAL("favicon/test_favicon.png"));
    NSData* favicon_data = [NSData
        dataWithContentsOfFile:base::SysUTF8ToNSString(favicon_path.value())];
    scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes(
        static_cast<const unsigned char*>([favicon_data bytes]),
        [favicon_data length]));

    result.bitmap_data = data;
    CGFloat scaled_size = [UIScreen mainScreen].scale * kTestFaviconSize;
    result.pixel_size = gfx::Size(scaled_size, scaled_size);
    result.icon_url = GURL(kTestFaviconIconURL);
    result.icon_type = favicon_base::TOUCH_ICON;
    CHECK(result.is_valid());
  }
  return result;
}

// A mock FaviconService that emits pre-programmed response.
class MockFaviconService : public favicon::FaviconService {
 public:
  MockFaviconService() : FaviconService(nullptr, nullptr) {}

  ~MockFaviconService() override {}

  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<int>& icon_types,
      int minimum_size_in_pixels,
      const favicon_base::FaviconRawBitmapCallback& callback,
      base::CancelableTaskTracker* tracker) override {
    favicon_base::FaviconRawBitmapResult mock_result =
        CreateTestBitmap(page_url);
    return tracker->PostTask(base::ThreadTaskRunnerHandle::Get().get(),
                             FROM_HERE, base::Bind(callback, mock_result));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFaviconService);
};

// This class provides access to LargeIconService internals, using the current
// thread's task runner for testing.
class TestLargeIconService : public favicon::LargeIconService {
 public:
  explicit TestLargeIconService(MockFaviconService* mock_favicon_service)
      : LargeIconService(mock_favicon_service,
                         base::ThreadTaskRunnerHandle::Get()) {}
  ~TestLargeIconService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLargeIconService);
};

class FaviconViewProviderTest : public PlatformTest {
 protected:
  void SetUp() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    PlatformTest::SetUp();
    mock_favicon_service_.reset(new MockFaviconService());
    large_icon_service_.reset(
        new TestLargeIconService(mock_favicon_service_.get()));
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<MockFaviconService> mock_favicon_service_;
  std::unique_ptr<TestLargeIconService> large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;
};

// Tests that image is set when a favicon is returned from LargeIconService.
TEST_F(FaviconViewProviderTest, Favicon) {
  id mock_delegate =
      [OCMockObject mockForProtocol:@protocol(FaviconViewProviderDelegate)];
  base::scoped_nsobject<FaviconViewProvider> viewProvider(
      [[FaviconViewProvider alloc] initWithURL:GURL(kTestFaviconURL)
                                   faviconSize:kTestFaviconSize
                                minFaviconSize:kTestFaviconSize
                              largeIconService:large_icon_service_.get()
                                      delegate:mock_delegate]);
  void (^confirmationBlock)(NSInvocation*) = ^(NSInvocation* invocation) {
    FaviconViewProvider* viewProvider;
    [invocation getArgument:&viewProvider atIndex:2];
    EXPECT_NSNE(nil, viewProvider.favicon);
  };
  [[[mock_delegate stub] andDo:confirmationBlock]
      faviconViewProviderFaviconDidLoad:viewProvider];
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that fallback data is set when no favicon is returned from
// LargeIconService.
TEST_F(FaviconViewProviderTest, FallbackIcon) {
  id mock_delegate =
      [OCMockObject mockForProtocol:@protocol(FaviconViewProviderDelegate)];
  base::scoped_nsobject<FaviconViewProvider> item([[FaviconViewProvider alloc]
           initWithURL:GURL(kTestFallbackURL)
           faviconSize:kTestFaviconSize
        minFaviconSize:kTestFaviconSize
      largeIconService:large_icon_service_.get()
              delegate:mock_delegate]);

  // Confirm that fallback text and color have been set before delegate call.
  void (^confirmationBlock)(NSInvocation*) = ^(NSInvocation* invocation) {
    FaviconViewProvider* viewProvider;
    [invocation getArgument:&viewProvider atIndex:2];
    // Fallback text is the first letter of the URL.
    NSString* defaultText = @"T";
    // Default colors are defined in
    // components/favicon_base/fallback_icon_style.h.
    UIColor* defaultTextColor = skia::UIColorFromSkColor(SK_ColorWHITE);
    UIColor* defaultBackgroundColor =
        skia::UIColorFromSkColor(SkColorSetRGB(0x78, 0x78, 0x78));
    EXPECT_NSEQ(defaultText, viewProvider.fallbackText);
    EXPECT_NSEQ(defaultTextColor, viewProvider.fallbackTextColor);
    EXPECT_NSEQ(defaultBackgroundColor, viewProvider.fallbackBackgroundColor);
  };
  [[[mock_delegate stub] andDo:confirmationBlock]
      faviconViewProviderFaviconDidLoad:item];
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

}  // namespace
