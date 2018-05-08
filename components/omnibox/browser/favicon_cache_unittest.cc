// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/favicon_cache.h"

#include "components/favicon/core/test/mock_favicon_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace {

favicon_base::FaviconImageResult GetDummyFaviconResult() {
  favicon_base::FaviconImageResult result;

  result.icon_url = GURL("http://example.com/favicon.ico");

  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorBLUE);
  result.image = gfx::Image::CreateFrom1xBitmap(bitmap);

  return result;
}

void VerifyFetchedFaviconAndCount(int* count, const gfx::Image& favicon) {
  DCHECK(count);
  EXPECT_FALSE(favicon.IsEmpty());
  ++(*count);
}

void VerifyFetchedFavicon(const gfx::Image& favicon) {
  EXPECT_FALSE(favicon.IsEmpty());
}

void Fail(const gfx::Image& favicon) {
  FAIL() << "This asynchronous callback should never have been called.";
}

class TestFaviconCache : public FaviconCache {
 public:
  explicit TestFaviconCache(favicon::FaviconService* favicon_service)
      : FaviconCache(favicon_service, nullptr /* history_service */) {}
  ~TestFaviconCache() override {}

  base::TimeTicks GetTimeNow() override { return fake_time_now_; }
  void SetTimeNow(base::TimeTicks now) { fake_time_now_ = now; }

 private:
  base::TimeTicks fake_time_now_;
};

}  // namespace

class FaviconCacheTest : public testing::Test {
 protected:
  const GURL kUrlA = GURL("http://www.a.com/");
  const GURL kUrlB = GURL("http://www.b.com/");

  FaviconCacheTest() : cache_(&favicon_service_) {}

  testing::NiceMock<favicon::MockFaviconService> favicon_service_;

  void ExpectFaviconServiceCalls(int a_site_calls, int b_site_calls) {
    if (a_site_calls > 0) {
      EXPECT_CALL(
          favicon_service_,
          GetFaviconImageForPageURL(kUrlA, _ /* callback */, _ /* tracker */))
          .Times(a_site_calls)
          .WillRepeatedly(
              DoAll(SaveArg<1>(&favicon_service_a_site_response_),
                    Return(base::CancelableTaskTracker::kBadTaskId)));
    }

    if (b_site_calls > 0) {
      EXPECT_CALL(
          favicon_service_,
          GetFaviconImageForPageURL(kUrlB, _ /* callback */, _ /* tracker */))
          .Times(b_site_calls)
          .WillRepeatedly(
              DoAll(SaveArg<1>(&favicon_service_b_site_response_),
                    Return(base::CancelableTaskTracker::kBadTaskId)));
    }
  }

  favicon_base::FaviconImageCallback favicon_service_a_site_response_;
  favicon_base::FaviconImageCallback favicon_service_b_site_response_;

  TestFaviconCache cache_;
};

TEST_F(FaviconCacheTest, Basic) {
  ExpectFaviconServiceCalls(1, 0);

  int response_count = 0;
  gfx::Image result = cache_.GetFaviconForPageUrl(
      kUrlA, base::BindOnce(&VerifyFetchedFaviconAndCount, &response_count));

  // Expect the synchronous result to be empty.
  EXPECT_TRUE(result.IsEmpty());

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  // Re-request the same favicon and expect a non-empty result now that the
  // cache is populated. The above EXPECT_CALL will also verify that the
  // backing FaviconService is not hit again.
  result = cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail));

  EXPECT_FALSE(result.IsEmpty());
  EXPECT_EQ(1, response_count);
}

TEST_F(FaviconCacheTest, MultipleRequestsAreCoalesced) {
  ExpectFaviconServiceCalls(1, 0);

  int response_count = 0;
  for (int i = 0; i < 10; ++i) {
    cache_.GetFaviconForPageUrl(
        kUrlA, base::BindOnce(&VerifyFetchedFaviconAndCount, &response_count));
  }

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  EXPECT_EQ(10, response_count);
}

TEST_F(FaviconCacheTest, SeparateOriginsAreCachedSeparately) {
  ExpectFaviconServiceCalls(1, 1);

  int a_site_response_count = 0;
  int b_site_response_count = 0;

  gfx::Image a_site_return = cache_.GetFaviconForPageUrl(
      kUrlA,
      base::BindOnce(&VerifyFetchedFaviconAndCount, &a_site_response_count));
  gfx::Image b_site_return = cache_.GetFaviconForPageUrl(
      kUrlB,
      base::BindOnce(&VerifyFetchedFaviconAndCount, &b_site_response_count));

  EXPECT_TRUE(a_site_return.IsEmpty());
  EXPECT_TRUE(b_site_return.IsEmpty());
  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(0, b_site_response_count);

  favicon_service_b_site_response_.Run(GetDummyFaviconResult());

  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  a_site_return = cache_.GetFaviconForPageUrl(
      kUrlA,
      base::BindOnce(&VerifyFetchedFaviconAndCount, &a_site_response_count));
  b_site_return = cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail));

  EXPECT_TRUE(a_site_return.IsEmpty());
  EXPECT_FALSE(b_site_return.IsEmpty());
  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  EXPECT_EQ(2, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  a_site_return = cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail));
  b_site_return = cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail));

  EXPECT_FALSE(a_site_return.IsEmpty());
  EXPECT_FALSE(b_site_return.IsEmpty());
}

TEST_F(FaviconCacheTest, ClearIconsWithHistoryDeletions) {
  ExpectFaviconServiceCalls(3, 2);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());

  favicon_service_a_site_response_.Run(GetDummyFaviconResult());
  favicon_service_b_site_response_.Run(GetDummyFaviconResult());

  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Delete just the entry for kUrlA.
  history::URLRows a_rows = {history::URLRow(kUrlA)};
  cache_.OnURLsDeleted(nullptr /* history_service */, false /* all_history */,
                       false /* expired */, a_rows, {} /* favicon_urls */);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Restore the cache entry for kUrlA.
  favicon_service_a_site_response_.Run(GetDummyFaviconResult());

  // Delete all history.
  cache_.OnURLsDeleted(nullptr /* history_service */, true /* all_history */,
                       false /* expired */, {} /* deleted_rows */,
                       {} /* favicon_urls */);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
}

TEST_F(FaviconCacheTest, CacheNullFavicons) {
  ExpectFaviconServiceCalls(1, 0);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  favicon_service_a_site_response_.Run(favicon_base::FaviconImageResult());

  // The mock FaviconService's EXPECT_CALL verifies that we do not make another
  // call to FaviconService.
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
}

TEST_F(FaviconCacheTest, ExpireNullFaviconsByHistory) {
  ExpectFaviconServiceCalls(2, 0);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  favicon_service_a_site_response_.Run(favicon_base::FaviconImageResult());

  cache_.OnURLVisited(nullptr /* history_service */, ui::PAGE_TRANSITION_LINK,
                      history::URLRow(kUrlA), history::RedirectList(),
                      base::Time::Now());

  // Now the empty favicon should have been expired and we expect our second
  // call to the mock underlying FaviconService.
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  favicon_service_a_site_response_.Run(GetDummyFaviconResult());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
}

TEST_F(FaviconCacheTest, ExpireNullFaviconsByTime) {
  ExpectFaviconServiceCalls(2, 1);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  base::TimeDelta max_duration = base::TimeDelta::FromSeconds(
      FaviconCache::kEmptyFaviconCacheLifetimeInSeconds);

  // Set the time to epoch and respond with the first empty favicon.
  cache_.SetTimeNow(base::TimeTicks());
  favicon_service_a_site_response_.Run(favicon_base::FaviconImageResult());

  // Increment the time to half the duration of the expiry time and respond
  // with the second empty favicon.
  cache_.SetTimeNow(base::TimeTicks() + max_duration / 2);
  favicon_service_b_site_response_.Run(favicon_base::FaviconImageResult());

  // Now increment the time so the the first, but not the second, cached empty
  // favicon is expired.
  cache_.SetTimeNow(base::TimeTicks() + max_duration);

  // Request the two favicons again.
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Our call to |ExpectFaviconServiceCalls(expected A calls, expected B calls)|
  // above should verify that we re-request the icon for kUrlA only (because
  // the empty result has been aged out).
}
