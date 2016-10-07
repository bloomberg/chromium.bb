// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetcher.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/predictors/resource_prefetcher_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::Property;

namespace predictors {

// Wrapper over the ResourcePrefetcher that stubs out the StartURLRequest call
// since we do not want to do network fetches in this unittest.
class TestResourcePrefetcher : public ResourcePrefetcher {
 public:
  TestResourcePrefetcher(ResourcePrefetcher::Delegate* delegate,
                         const ResourcePrefetchPredictorConfig& config,
                         const GURL& main_frame_url,
                         const std::vector<GURL>& urls)
      : ResourcePrefetcher(delegate, config, main_frame_url, urls) {}

  ~TestResourcePrefetcher() override {}

  MOCK_METHOD1(StartURLRequest, void(net::URLRequest* request));

  void ReadFullResponse(net::URLRequest* request) override {
    EXPECT_TRUE(request->load_flags() & net::LOAD_PREFETCH);
    FinishRequest(request);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResourcePrefetcher);
};


// Delegate for ResourcePrefetcher.
class TestResourcePrefetcherDelegate : public ResourcePrefetcher::Delegate {
 public:
  explicit TestResourcePrefetcherDelegate(base::MessageLoop* loop)
      : request_context_getter_(
            new net::TestURLRequestContextGetter(loop->task_runner())) {}
  ~TestResourcePrefetcherDelegate() { }

  net::URLRequestContext* GetURLRequestContext() override {
    return request_context_getter_->GetURLRequestContext();
  }

  MOCK_METHOD1(ResourcePrefetcherFinished,
               void(ResourcePrefetcher* prefetcher));

 private:
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(TestResourcePrefetcherDelegate);
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

  base::MessageLoop loop_;
  content::TestBrowserThread io_thread_;
  ResourcePrefetchPredictorConfig config_;
  TestResourcePrefetcherDelegate prefetcher_delegate_;
  std::unique_ptr<TestResourcePrefetcher> prefetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetcherTest);
};

ResourcePrefetcherTest::ResourcePrefetcherTest()
    : loop_(base::MessageLoop::TYPE_IO),
      io_thread_(content::BrowserThread::IO, &loop_),
      prefetcher_delegate_(&loop_) {
  config_.max_prefetches_inflight_per_navigation = 5;
  config_.max_prefetches_inflight_per_host_per_navigation = 2;
}

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

  NavigationID navigation_id(1, 2, main_frame_url);

  prefetcher_.reset(new TestResourcePrefetcher(&prefetcher_delegate_, config_,
                                               main_frame_url, urls));

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

  // Expect the final call.
  EXPECT_CALL(prefetcher_delegate_,
              ResourcePrefetcherFinished(Eq(prefetcher_.get())));

  OnResponse("http://yahoo.com/resource3.png");
  CheckPrefetcherState(0, 0, 0);
}

TEST_F(ResourcePrefetcherTest, TestPrefetcherStopped) {
  GURL main_frame_url("http://www.google.com");
  std::vector<GURL> urls = {GURL("http://www.google.com/resource1.html"),
                            GURL("http://www.google.com/resource2.png"),
                            GURL("http://yahoo.com/resource1.png"),
                            GURL("http://yahoo.com/resource2.png"),
                            GURL("http://yahoo.com/resource3.png"),
                            GURL("http://m.google.com/resource1.jpg")};

  NavigationID navigation_id(1, 2, main_frame_url);

  prefetcher_.reset(new TestResourcePrefetcher(&prefetcher_delegate_, config_,
                                               main_frame_url, urls));

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

  // Expect the final call.
  EXPECT_CALL(prefetcher_delegate_,
              ResourcePrefetcherFinished(Eq(prefetcher_.get())));

  OnResponse("http://m.google.com/resource1.jpg");
  CheckPrefetcherState(0, 1, 0);
}

}  // namespace predictors
