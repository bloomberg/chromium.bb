// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestNotificationInterface {
 public:
  virtual ~TestNotificationInterface() {}
  virtual void OnImageChanged() = 0;
  virtual void OnRequestFinished() = 0;
};

class TestObserver : public BitmapFetcherService::Observer {
 public:
  explicit TestObserver(TestNotificationInterface* target) : target_(target) {}
  virtual ~TestObserver() { target_->OnRequestFinished(); }

  virtual void OnImageChanged(BitmapFetcherService::RequestId request_id,
                              const SkBitmap& answers_image) OVERRIDE {
    target_->OnImageChanged();
  }

  TestNotificationInterface* target_;
};

class TestService : public BitmapFetcherService {
 public:
  explicit TestService(content::BrowserContext* context)
      : BitmapFetcherService(context) {}
  virtual ~TestService() {}

  // Create a fetcher, but don't start downloading. That allows side-stepping
  // the decode step, which requires a utility process.
  virtual chrome::BitmapFetcher* CreateFetcher(const GURL& url) OVERRIDE {
    return new chrome::BitmapFetcher(url, this);
  }
};

}  // namespace

class BitmapFetcherServiceTest : public testing::Test,
                                 public TestNotificationInterface {
 public:
  virtual void SetUp() OVERRIDE {
    service_.reset(new TestService(&profile_));
    requestsFinished_ = 0;
    imagesChanged_ = 0;
    url1_ = GURL("http://example.org/sample-image-1.png");
    url2_ = GURL("http://example.org/sample-image-2.png");
  }

  const ScopedVector<BitmapFetcherRequest>& requests() {
    return service_->requests_;
  }
  const ScopedVector<chrome::BitmapFetcher>& active_fetchers() {
    return service_->active_fetchers_;
  }
  size_t cache_size() { return service_->cache_.size(); }

  virtual void OnImageChanged() OVERRIDE { imagesChanged_++; }

  virtual void OnRequestFinished() OVERRIDE { requestsFinished_++; }

  // Simulate finishing a URL fetch and decode for the given fetcher.
  void CompleteFetch(const GURL& url) {
    const chrome::BitmapFetcher* fetcher = service_->FindFetcherForUrl(url);
    ASSERT_TRUE(NULL != fetcher);

    // Create a non-empty bitmap.
    SkBitmap image;
    image.allocN32Pixels(2, 2);
    image.eraseColor(SK_ColorGREEN);

    const_cast<chrome::BitmapFetcher*>(fetcher)->OnImageDecoded(NULL, image);
  }

  void FailFetch(const GURL& url) {
    const chrome::BitmapFetcher* fetcher = service_->FindFetcherForUrl(url);
    ASSERT_TRUE(NULL != fetcher);
    const_cast<chrome::BitmapFetcher*>(fetcher)
        ->OnImageDecoded(NULL, SkBitmap());
  }

 protected:
  scoped_ptr<BitmapFetcherService> service_;

  int imagesChanged_;
  int requestsFinished_;

  GURL url1_;
  GURL url2_;

 private:
  TestingProfile profile_;
};

TEST_F(BitmapFetcherServiceTest, CancelInvalidRequest) {
  service_->CancelRequest(BitmapFetcherService::REQUEST_ID_INVALID);
  service_->CancelRequest(23);
}

TEST_F(BitmapFetcherServiceTest, OnlyFirstRequestCreatesFetcher) {
  EXPECT_EQ(0U, active_fetchers().size());

  service_->RequestImage(url1_, new TestObserver(this));
  EXPECT_EQ(1U, active_fetchers().size());

  service_->RequestImage(url1_, new TestObserver(this));
  EXPECT_EQ(1U, active_fetchers().size());
}

TEST_F(BitmapFetcherServiceTest, CompletedFetchNotifiesAllObservers) {
  service_->RequestImage(url1_, new TestObserver(this));
  service_->RequestImage(url1_, new TestObserver(this));
  service_->RequestImage(url1_, new TestObserver(this));
  service_->RequestImage(url1_, new TestObserver(this));
  EXPECT_EQ(1U, active_fetchers().size());
  EXPECT_EQ(4U, requests().size());

  CompleteFetch(url1_);
  EXPECT_EQ(4, imagesChanged_);
  EXPECT_EQ(4, requestsFinished_);
}

TEST_F(BitmapFetcherServiceTest, CancelRequest) {
  service_->RequestImage(url1_, new TestObserver(this));
  service_->RequestImage(url1_, new TestObserver(this));
  BitmapFetcherService::RequestId requestId =
      service_->RequestImage(url2_, new TestObserver(this));
  service_->RequestImage(url1_, new TestObserver(this));
  service_->RequestImage(url1_, new TestObserver(this));
  EXPECT_EQ(5U, requests().size());

  service_->CancelRequest(requestId);
  EXPECT_EQ(4U, requests().size());

  CompleteFetch(url2_);
  EXPECT_EQ(0, imagesChanged_);

  CompleteFetch(url1_);
  EXPECT_EQ(4, imagesChanged_);
}

TEST_F(BitmapFetcherServiceTest, FailedRequestsDontEnterCache) {
  service_->RequestImage(url1_, new TestObserver(this));
  service_->RequestImage(url2_, new TestObserver(this));
  EXPECT_EQ(0U, cache_size());

  CompleteFetch(url1_);
  EXPECT_EQ(1U, cache_size());

  FailFetch(url2_);
  EXPECT_EQ(1U, cache_size());
}
