// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetcher.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::Property;

namespace predictors {

constexpr size_t kMaxConcurrentRequests = 5;
constexpr size_t kMaxConcurrentRequestsPerHost = 2;

// Delegate for ResourcePrefetcher.
class TestResourcePrefetcherDelegate
    : public ResourcePrefetcher::Delegate,
      public base::SupportsWeakPtr<TestResourcePrefetcherDelegate> {
 public:
  TestResourcePrefetcherDelegate() = default;
  ~TestResourcePrefetcherDelegate() override = default;

  void ResourcePrefetcherFinished(
      ResourcePrefetcher* prefetcher,
      std::unique_ptr<ResourcePrefetcher::PrefetcherStats> stats) override {
    prefetcher_ = prefetcher;
  }

  bool ResourcePrefetcherFinishedCalled(ResourcePrefetcher* for_prefetcher) {
    ResourcePrefetcher* prefetcher = prefetcher_;
    prefetcher_ = nullptr;
    return prefetcher == for_prefetcher;
  }

 private:
  ResourcePrefetcher* prefetcher_;

  DISALLOW_COPY_AND_ASSIGN(TestResourcePrefetcherDelegate);
};

// Wrapper over the ResourcePrefetcher that stubs out the StartURLRequest call
// since we do not want to do network fetches in this unittest.
class TestResourcePrefetcher : public ResourcePrefetcher {
 public:
  TestResourcePrefetcher(
      TestResourcePrefetcherDelegate* delegate,
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      size_t max_concurrent_requests,
      size_t max_concurrent_requests_per_host,
      const GURL& main_frame_url,
      const std::vector<GURL>& urls)
      : ResourcePrefetcher(delegate->AsWeakPtr(),
                           context_getter,
                           max_concurrent_requests,
                           max_concurrent_requests_per_host,
                           main_frame_url,
                           urls) {}

  ~TestResourcePrefetcher() override {}

  MOCK_METHOD1(StartURLRequest, void(net::URLRequest* request));

  void ReadFullResponse(net::URLRequest* request) override {
    EXPECT_TRUE(request->load_flags() & net::LOAD_PREFETCH);
    RequestComplete(request);
    FinishRequest(request);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResourcePrefetcher);
};

// The following unittest tests most of the ResourcePrefetcher except for:
// 1. Call to ReadFullResponse. There does not seem to be a good way to test the
//    function in a unittest, and probably requires a browser_test.
// 2. Setting of the Prefetch status for cache vs non cache.
class ResourcePrefetcherTest : public testing::Test {
 public:
  ResourcePrefetcherTest();
  ~ResourcePrefetcherTest() override;

 protected:
  void AddStartUrlRequestExpectation(const std::string& url) {
    EXPECT_CALL(*prefetcher_,
                StartURLRequest(Property(&net::URLRequest::original_url,
                                         Eq(GURL(url)))));
  }

  void CheckPrefetcherState(size_t inflight, size_t queue, size_t host) {
    EXPECT_EQ(prefetcher_->inflight_requests_.size(), inflight);
    EXPECT_EQ(prefetcher_->request_queue_.size(), queue);
    EXPECT_EQ(prefetcher_->host_inflight_counts_.size(), host);
  }

  net::URLRequest* GetInFlightRequest(const std::string& url_str) {
    GURL url(url_str);

    for (const auto& queued_url : prefetcher_->request_queue_) {
      EXPECT_NE(queued_url, url);
    }
    for (const auto& key_value : prefetcher_->inflight_requests_) {
      if (key_value.first->original_url() == url)
        return key_value.first;
    }
    EXPECT_TRUE(false) << "In flight request not found: " << url_str;
    return NULL;
  }


  void OnReceivedRedirect(const std::string& url) {
    prefetcher_->OnReceivedRedirect(
        GetInFlightRequest(url), net::RedirectInfo(), NULL);
  }
  void OnAuthRequired(const std::string& url) {
    prefetcher_->OnAuthRequired(GetInFlightRequest(url), NULL);
  }
  void OnCertificateRequested(const std::string& url) {
    prefetcher_->OnCertificateRequested(GetInFlightRequest(url), NULL);
  }
  void OnSSLCertificateError(const std::string& url) {
    prefetcher_->OnSSLCertificateError(GetInFlightRequest(url),
                                       net::SSLInfo(), false);
  }
  void OnResponse(const std::string& url) {
    prefetcher_->OnResponseStarted(GetInFlightRequest(url), net::OK);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestResourcePrefetcherDelegate prefetcher_delegate_;
  std::unique_ptr<TestResourcePrefetcher> prefetcher_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetcherTest);
};

ResourcePrefetcherTest::ResourcePrefetcherTest()
    : prefetcher_delegate_(),
      context_getter_(base::MakeRefCounted<net::TestURLRequestContextGetter>(
          base::ThreadTaskRunnerHandle::Get())) {}

ResourcePrefetcherTest::~ResourcePrefetcherTest() {
}

TEST_F(ResourcePrefetcherTest, TestPrefetcherFinishes) {
  GURL main_frame_url("http://www.google.com");
  std::vector<GURL> urls = {GURL("http://www.google.com/resource1.html"),
                            GURL("http://www.google.com/resource2.png"),
                            GURL("http://yahoo.com/resource1.png"),
                            GURL("http://yahoo.com/resource2.png"),
                            GURL("http://yahoo.com/resource3.png"),
                            GURL("http://m.google.com/resource1.jpg"),
                            GURL("http://www.google.com/resource3.html"),
                            GURL("http://m.google.com/resource2.html"),
                            GURL("http://m.google.com/resource3.css"),
                            GURL("http://m.google.com/resource4.png"),
                            GURL("http://yahoo.com/resource4.png"),
                            GURL("http://yahoo.com/resource5.png")};

  prefetcher_ = base::MakeUnique<TestResourcePrefetcher>(
      &prefetcher_delegate_, context_getter_, kMaxConcurrentRequests,
      kMaxConcurrentRequestsPerHost, main_frame_url, urls);

  // Starting the prefetcher maxes out the number of possible requests.
  AddStartUrlRequestExpectation("http://www.google.com/resource1.html");
  AddStartUrlRequestExpectation("http://www.google.com/resource2.png");
  AddStartUrlRequestExpectation("http://yahoo.com/resource1.png");
  AddStartUrlRequestExpectation("http://yahoo.com/resource2.png");
  AddStartUrlRequestExpectation("http://m.google.com/resource1.jpg");

  prefetcher_->Start();
  CheckPrefetcherState(5, 7, 3);

  AddStartUrlRequestExpectation("http://m.google.com/resource2.html");
  OnResponse("http://m.google.com/resource1.jpg");
  CheckPrefetcherState(5, 6, 3);

  AddStartUrlRequestExpectation("http://www.google.com/resource3.html");
  OnSSLCertificateError("http://www.google.com/resource1.html");
  CheckPrefetcherState(5, 5, 3);

  AddStartUrlRequestExpectation("http://m.google.com/resource3.css");
  OnResponse("http://m.google.com/resource2.html");
  CheckPrefetcherState(5, 4, 3);

  AddStartUrlRequestExpectation("http://m.google.com/resource4.png");
  OnReceivedRedirect("http://www.google.com/resource3.html");
  CheckPrefetcherState(5, 3, 3);

  OnResponse("http://www.google.com/resource2.png");
  CheckPrefetcherState(4, 3, 2);

  AddStartUrlRequestExpectation("http://yahoo.com/resource3.png");
  OnReceivedRedirect("http://yahoo.com/resource2.png");
  CheckPrefetcherState(4, 2, 2);

  AddStartUrlRequestExpectation("http://yahoo.com/resource4.png");
  OnResponse("http://yahoo.com/resource1.png");
  CheckPrefetcherState(4, 1, 2);

  AddStartUrlRequestExpectation("http://yahoo.com/resource5.png");
  OnResponse("http://yahoo.com/resource4.png");
  CheckPrefetcherState(4, 0, 2);

  OnResponse("http://yahoo.com/resource5.png");
  CheckPrefetcherState(3, 0, 2);

  OnCertificateRequested("http://m.google.com/resource4.png");
  CheckPrefetcherState(2, 0, 2);

  OnAuthRequired("http://m.google.com/resource3.css");
  CheckPrefetcherState(1, 0, 1);

  OnResponse("http://yahoo.com/resource3.png");
  CheckPrefetcherState(0, 0, 0);

  base::RunLoop().RunUntilIdle();
  // Expect the final call.
  EXPECT_TRUE(
      prefetcher_delegate_.ResourcePrefetcherFinishedCalled(prefetcher_.get()));
}

TEST_F(ResourcePrefetcherTest, TestPrefetcherStopped) {
  GURL main_frame_url("http://www.google.com");
  std::vector<GURL> urls = {GURL("http://www.google.com/resource1.html"),
                            GURL("http://www.google.com/resource2.png"),
                            GURL("http://yahoo.com/resource1.png"),
                            GURL("http://yahoo.com/resource2.png"),
                            GURL("http://yahoo.com/resource3.png"),
                            GURL("http://m.google.com/resource1.jpg")};

  prefetcher_ = base::MakeUnique<TestResourcePrefetcher>(
      &prefetcher_delegate_, context_getter_, kMaxConcurrentRequests,
      kMaxConcurrentRequestsPerHost, main_frame_url, urls);

  // Starting the prefetcher maxes out the number of possible requests.
  AddStartUrlRequestExpectation("http://www.google.com/resource1.html");
  AddStartUrlRequestExpectation("http://www.google.com/resource2.png");
  AddStartUrlRequestExpectation("http://yahoo.com/resource1.png");
  AddStartUrlRequestExpectation("http://yahoo.com/resource2.png");
  AddStartUrlRequestExpectation("http://m.google.com/resource1.jpg");

  prefetcher_->Start();
  CheckPrefetcherState(5, 1, 3);

  OnResponse("http://www.google.com/resource1.html");
  CheckPrefetcherState(4, 1, 3);

  prefetcher_->Stop();  // No more queueing.

  OnResponse("http://www.google.com/resource2.png");
  CheckPrefetcherState(3, 1, 2);

  OnResponse("http://yahoo.com/resource1.png");
  CheckPrefetcherState(2, 1, 2);

  OnResponse("http://yahoo.com/resource2.png");
  CheckPrefetcherState(1, 1, 1);

  OnResponse("http://m.google.com/resource1.jpg");
  CheckPrefetcherState(0, 1, 0);

  base::RunLoop().RunUntilIdle();
  // Expect the final call.
  EXPECT_TRUE(
      prefetcher_delegate_.ResourcePrefetcherFinishedCalled(prefetcher_.get()));
}

TEST_F(ResourcePrefetcherTest, TestHistogramsCollected) {
  base::HistogramTester histogram_tester;
  GURL main_frame_url("http://www.google.com");
  std::vector<GURL> urls = {GURL("http://www.google.com/resource1.png"),
                            GURL("http://www.google.com/resource2.png"),
                            GURL("http://www.google.com/resource3.png"),
                            GURL("http://www.google.com/resource4.png"),
                            GURL("http://www.google.com/resource5.png"),
                            GURL("http://www.google.com/resource6.png")};

  prefetcher_ = base::MakeUnique<TestResourcePrefetcher>(
      &prefetcher_delegate_, context_getter_, kMaxConcurrentRequests,
      kMaxConcurrentRequestsPerHost, main_frame_url, urls);

  // Starting the prefetcher maxes out the number of possible requests.
  AddStartUrlRequestExpectation("http://www.google.com/resource1.png");
  AddStartUrlRequestExpectation("http://www.google.com/resource2.png");
  AddStartUrlRequestExpectation("http://www.google.com/resource3.png");

  prefetcher_->Start();

  AddStartUrlRequestExpectation("http://www.google.com/resource4.png");
  OnResponse("http://www.google.com/resource1.png");
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorCachePatternHistogram, 1);

  // Failed prefetches aren't counted.
  AddStartUrlRequestExpectation("http://www.google.com/resource5.png");
  OnReceivedRedirect("http://www.google.com/resource2.png");

  AddStartUrlRequestExpectation("http://www.google.com/resource6.png");
  OnAuthRequired("http://www.google.com/resource3.png");

  OnCertificateRequested("http://www.google.com/resource4.png");

  OnSSLCertificateError("http://www.google.com/resource5.png");
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorCachePatternHistogram, 1);

  OnResponse("http://www.google.com/resource6.png");
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorCachePatternHistogram, 2);
  histogram_tester.ExpectBucketCount(
      internal::kResourcePrefetchPredictorPrefetchedCountHistogram, 2, 1);
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorPrefetchedSizeHistogram, 1);

  base::RunLoop().RunUntilIdle();
  // Expect the final call.
  EXPECT_TRUE(
      prefetcher_delegate_.ResourcePrefetcherFinishedCalled(prefetcher_.get()));
}

TEST_F(ResourcePrefetcherTest, TestReferrer) {
  std::string url = "https://www.notgoogle.com/cats.html";
  std::string https_resource = "https://www.google.com/resource1.png";
  std::string http_resource = "http://www.google.com/resource1.png";

  std::vector<GURL> urls = {GURL(https_resource), GURL(http_resource)};

  prefetcher_ = base::MakeUnique<TestResourcePrefetcher>(
      &prefetcher_delegate_, context_getter_, kMaxConcurrentRequests,
      kMaxConcurrentRequestsPerHost, GURL(url), urls);

  AddStartUrlRequestExpectation(https_resource);
  AddStartUrlRequestExpectation(http_resource);
  prefetcher_->Start();

  net::URLRequest* request = GetInFlightRequest(https_resource);
  EXPECT_TRUE(request);
  EXPECT_EQ(url, request->referrer());

  request = GetInFlightRequest(http_resource);
  EXPECT_TRUE(request);
  EXPECT_EQ("", request->referrer());

  OnResponse(https_resource);
  OnResponse(http_resource);

  base::RunLoop().RunUntilIdle();
  // Expect the final call.
  EXPECT_TRUE(
      prefetcher_delegate_.ResourcePrefetcherFinishedCalled(prefetcher_.get()));
}

}  // namespace predictors
