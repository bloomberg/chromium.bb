// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/favicon_url.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

namespace favicon {
namespace {

using testing::Mock;
using testing::Return;

class ContentFaviconDriverTest : public content::RenderViewHostTestHarness {
 protected:
  ContentFaviconDriverTest() {}

  ~ContentFaviconDriverTest() override {}

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ContentFaviconDriver::CreateForWebContents(
        web_contents(), &favicon_service_, nullptr, nullptr);
  }

  testing::StrictMock<MockFaviconService> favicon_service_;
};

// Test that Favicon is not requested repeatedly during the same session if
// server returns HTTP 404 status.
TEST_F(ContentFaviconDriverTest, UnableToDownloadFavicon) {
  const GURL missing_icon_url("http://www.google.com/favicon.ico");

  ContentFaviconDriver* content_favicon_driver =
      ContentFaviconDriver::FromWebContents(web_contents());

  std::vector<SkBitmap> empty_icons;
  std::vector<gfx::Size> empty_icon_sizes;
  int download_id = 0;

  // Try to download missing icon.
  EXPECT_CALL(favicon_service_, WasUnableToDownloadFavicon(missing_icon_url))
      .WillOnce(Return(false));
  download_id = content_favicon_driver->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);

  // Report download failure with HTTP 503 status.
  content_favicon_driver->DidDownloadFavicon(download_id, 503, missing_icon_url,
                                             empty_icons, empty_icon_sizes);
  Mock::VerifyAndClearExpectations(&favicon_service_);

  // Try to download again.
  EXPECT_CALL(favicon_service_, WasUnableToDownloadFavicon(missing_icon_url))
      .WillOnce(Return(false));
  download_id = content_favicon_driver->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);
  Mock::VerifyAndClearExpectations(&favicon_service_);

  // Report download failure with HTTP 404 status, which causes the icon to be
  // marked as UnableToDownload.
  EXPECT_CALL(favicon_service_, UnableToDownloadFavicon(missing_icon_url));
  content_favicon_driver->DidDownloadFavicon(download_id, 404, missing_icon_url,
                                             empty_icons, empty_icon_sizes);
  Mock::VerifyAndClearExpectations(&favicon_service_);

  // Try to download again.
  EXPECT_CALL(favicon_service_, WasUnableToDownloadFavicon(missing_icon_url))
      .WillOnce(Return(true));
  download_id = content_favicon_driver->StartDownload(missing_icon_url, 0);
  // Download is not started and Icon is still marked as UnableToDownload.
  EXPECT_EQ(0, download_id);
  Mock::VerifyAndClearExpectations(&favicon_service_);
}

// Test that ContentFaviconDriver ignores updated favicon URLs if there is no
// last committed entry. This occurs when script is injected in about:blank.
// See crbug.com/520759 for more details
TEST_F(ContentFaviconDriverTest, FaviconUpdateNoLastCommittedEntry) {
  ASSERT_EQ(nullptr, web_contents()->GetController().GetLastCommittedEntry());

  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(content::FaviconURL(
      GURL("http://www.google.ca/favicon.ico"), content::FaviconURL::FAVICON,
      std::vector<gfx::Size>()));
  favicon::ContentFaviconDriver* driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents());
  static_cast<content::WebContentsObserver*>(driver)
      ->DidUpdateFaviconURL(favicon_urls);

  // Test that ContentFaviconDriver ignored the favicon url update.
  EXPECT_TRUE(driver->favicon_urls().empty());
}

}  // namespace
}  // namespace favicon
