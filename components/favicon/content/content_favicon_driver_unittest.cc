// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/favicon_url.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

namespace favicon {
namespace {

using testing::ElementsAre;
using testing::Return;
using testing::_;

class ContentFaviconDriverTest : public content::RenderViewHostTestHarness {
 protected:
  const std::vector<gfx::Size> kEmptyIconSizes;
  const std::vector<SkBitmap> kEmptyIcons;
  const std::vector<favicon_base::FaviconRawBitmapResult> kEmptyRawBitmapResult;
  const GURL kPageURL = GURL("http://www.google.com/");
  const GURL kIconURL = GURL("http://www.google.com/favicon.ico");

  ContentFaviconDriverTest() {
    ON_CALL(favicon_service_, UpdateFaviconMappingsAndFetch(_, _, _, _, _, _))
        .WillByDefault(PostReply<6>(kEmptyRawBitmapResult));
    ON_CALL(favicon_service_, GetFaviconForPageURL(_, _, _, _, _))
        .WillByDefault(PostReply<5>(kEmptyRawBitmapResult));
  }

  ~ContentFaviconDriverTest() override {}

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ContentFaviconDriver::CreateForWebContents(
        web_contents(), &favicon_service_, nullptr, nullptr);
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  void TestFetchFaviconForPage(
      const GURL& page_url,
      const std::vector<content::FaviconURL>& candidates) {
    ContentFaviconDriver* favicon_driver =
        ContentFaviconDriver::FromWebContents(web_contents());
    web_contents_tester()->NavigateAndCommit(page_url);
    static_cast<content::WebContentsObserver*>(favicon_driver)
        ->DidUpdateFaviconURL(candidates);
    base::RunLoop().RunUntilIdle();
  }

  testing::NiceMock<MockFaviconService> favicon_service_;
};

// Test that UnableToDownloadFavicon() is not called as a result of a favicon
// download with 200 status.
TEST_F(ContentFaviconDriverTest, ShouldNotCallUnableToDownloadFaviconFor200) {
  EXPECT_CALL(favicon_service_, UnableToDownloadFavicon(kIconURL)).Times(0);
  // Mimic a page load.
  TestFetchFaviconForPage(
      kPageURL,
      {content::FaviconURL(kIconURL, content::FaviconURL::FAVICON,
                           kEmptyIconSizes)});
  // Completing the download should not cause a call to
  // UnableToDownloadFavicon().
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kIconURL, 200, kEmptyIcons, kEmptyIconSizes));
}

// Test that UnableToDownloadFavicon() is called as a result of a favicon
// download with 404 status.
TEST_F(ContentFaviconDriverTest, ShouldCallUnableToDownloadFaviconFor404) {
  EXPECT_CALL(favicon_service_, UnableToDownloadFavicon(kIconURL));
  // Mimic a page load.
  TestFetchFaviconForPage(
      kPageURL,
      {content::FaviconURL(kIconURL, content::FaviconURL::FAVICON,
                           kEmptyIconSizes)});
  // Mimic the completion of an image download.
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kIconURL, 404, kEmptyIcons, kEmptyIconSizes));
}

// Test that UnableToDownloadFavicon() is not called as a result of a favicon
// download with 503 status.
TEST_F(ContentFaviconDriverTest, ShouldNotCallUnableToDownloadFaviconFor503) {
  EXPECT_CALL(favicon_service_, UnableToDownloadFavicon(kIconURL)).Times(0);
  // Mimic a page load.
  TestFetchFaviconForPage(
      kPageURL,
      {content::FaviconURL(kIconURL, content::FaviconURL::FAVICON,
                           kEmptyIconSizes)});
  // Completing the download should not cause a call to
  // UnableToDownloadFavicon().
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kIconURL, 503, kEmptyIcons, kEmptyIconSizes));
}

// Test that Favicon is not requested repeatedly during the same session if
// the favicon is known to be unavailable (e.g. due to HTTP 404 status).
TEST_F(ContentFaviconDriverTest, ShouldNotRequestRepeatedlyIfUnavailable) {
  ON_CALL(favicon_service_, WasUnableToDownloadFavicon(kIconURL))
      .WillByDefault(Return(true));
  // Mimic a page load.
  TestFetchFaviconForPage(
      kPageURL,
      {content::FaviconURL(kIconURL, content::FaviconURL::FAVICON,
                           kEmptyIconSizes)});
  // Verify that no download request is pending for the image.
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kIconURL));
}

// Test that ContentFaviconDriver ignores updated favicon URLs if there is no
// last committed entry. This occurs when script is injected in about:blank.
// See crbug.com/520759 for more details
TEST_F(ContentFaviconDriverTest, FaviconUpdateNoLastCommittedEntry) {
  ASSERT_EQ(nullptr, web_contents()->GetController().GetLastCommittedEntry());

  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(
      content::FaviconURL(GURL("http://www.google.ca/favicon.ico"),
                          content::FaviconURL::FAVICON, kEmptyIconSizes));
  favicon::ContentFaviconDriver* driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents());
  static_cast<content::WebContentsObserver*>(driver)
      ->DidUpdateFaviconURL(favicon_urls);

  // Test that ContentFaviconDriver ignored the favicon url update.
  EXPECT_TRUE(driver->favicon_urls().empty());
}

TEST_F(ContentFaviconDriverTest, RecordsHistorgramsForCandidates) {
  const std::vector<gfx::Size> kSizes16x16and32x32({{16, 16}, {32, 32}});
  base::HistogramTester tester;
  content::WebContentsObserver* driver_as_observer =
      ContentFaviconDriver::FromWebContents(web_contents());

  // Navigation to a page updating one icon.
  NavigateAndCommit(GURL("http://www.youtube.com"));
  driver_as_observer->DidUpdateFaviconURL(
      {content::FaviconURL(GURL("http://www.youtube.com/favicon.ico"),
                           content::FaviconURL::FAVICON, kSizes16x16and32x32)});

  EXPECT_THAT(tester.GetAllSamples("Favicons.CandidatesCount"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(tester.GetAllSamples("Favicons.CandidatesWithDefinedSizesCount"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(tester.GetAllSamples("Favicons.CandidatesWithTouchIconsCount"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  std::vector<content::FaviconURL> favicon_urls = {
      content::FaviconURL(GURL("http://www.google.ca/favicon.ico"),
                          content::FaviconURL::FAVICON, kSizes16x16and32x32),
      content::FaviconURL(GURL("http://www.google.ca/precomposed_icon.png"),
                          content::FaviconURL::TOUCH_PRECOMPOSED_ICON,
                          kEmptyIconSizes),
      content::FaviconURL(GURL("http://www.google.ca/touch_icon.png"),
                          content::FaviconURL::TOUCH_ICON, kEmptyIconSizes)};

  // Double navigation to a page with 3 different icons.
  NavigateAndCommit(GURL("http://www.google.ca"));
  driver_as_observer->DidUpdateFaviconURL(favicon_urls);
  NavigateAndCommit(GURL("http://www.google.ca"));
  driver_as_observer->DidUpdateFaviconURL(favicon_urls);

  EXPECT_THAT(tester.GetAllSamples("Favicons.CandidatesCount"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1),
                          base::Bucket(/*min=*/3, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("Favicons.CandidatesWithDefinedSizesCount"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/3)));
  EXPECT_THAT(tester.GetAllSamples("Favicons.CandidatesWithTouchIconsCount"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/2, /*count=*/2)));
}

}  // namespace
}  // namespace favicon
