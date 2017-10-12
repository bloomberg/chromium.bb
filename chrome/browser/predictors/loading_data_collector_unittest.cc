// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_data_collector.h"

#include <iostream>
#include <memory>
#include <utility>

#include "base/test/histogram_tester.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace predictors {

class LoadingDataCollectorTest : public testing::Test {
 public:
  LoadingDataCollectorTest() : profile_(base::MakeUnique<TestingProfile>()) {
    LoadingPredictorConfig config;
    PopulateTestConfig(&config);
    mock_predictor_ =
        base::MakeUnique<StrictMock<MockResourcePrefetchPredictor>>(
            config, profile_.get()),
    collector_ = base::MakeUnique<LoadingDataCollector>(mock_predictor_.get(),
                                                        nullptr, config);
  }

  void SetUp() override {
    content::RunAllTasksUntilIdle();  // Runs the DB lookup.

    url_request_job_factory_.Reset();
    url_request_context_.set_job_factory(&url_request_job_factory_);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  net::TestURLRequestContext url_request_context_;
  MockURLRequestJobFactory url_request_job_factory_;

  std::unique_ptr<StrictMock<MockResourcePrefetchPredictor>> mock_predictor_;
  std::unique_ptr<LoadingDataCollector> collector_;
};

TEST_F(LoadingDataCollectorTest, SummarizeResponse) {
  net::HttpResponseInfo response_info;
  response_info.headers =
      MakeResponseHeaders("HTTP/1.1 200 OK\n\nSome: Headers\n");
  response_info.was_cached = true;
  url_request_job_factory_.set_response_info(response_info);

  GURL url("http://www.google.com/cat.png");
  std::unique_ptr<net::URLRequest> request =
      CreateURLRequest(url_request_context_, url, net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  URLRequestSummary summary;
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request, &summary));
  EXPECT_EQ(url, summary.resource_url);
  EXPECT_EQ(content::RESOURCE_TYPE_IMAGE, summary.resource_type);
  EXPECT_TRUE(summary.was_cached);
  EXPECT_FALSE(summary.has_validators);
  EXPECT_FALSE(summary.always_revalidate);

  // Navigation_id elements should be unset by default.
  EXPECT_EQ(-1, summary.navigation_id.tab_id);
  EXPECT_EQ(GURL(), summary.navigation_id.main_frame_url);
}

TEST_F(LoadingDataCollectorTest, SummarizeResponseContentType) {
  net::HttpResponseInfo response_info;
  response_info.headers = MakeResponseHeaders(
      "HTTP/1.1 200 OK\n\n"
      "Some: Headers\n"
      "Content-Type: image/whatever\n");
  url_request_job_factory_.set_response_info(response_info);
  url_request_job_factory_.set_mime_type("image/png");

  std::unique_ptr<net::URLRequest> request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, true);
  URLRequestSummary summary;
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request, &summary));
  EXPECT_EQ(content::RESOURCE_TYPE_IMAGE, summary.resource_type);
}

TEST_F(LoadingDataCollectorTest, SummarizeResponseCachePolicy) {
  net::HttpResponseInfo response_info;
  response_info.headers = MakeResponseHeaders(
      "HTTP/1.1 200 OK\n"
      "Some: Headers\n");
  url_request_job_factory_.set_response_info(response_info);

  std::unique_ptr<net::URLRequest> request_no_validators = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, true);

  URLRequestSummary summary;
  EXPECT_TRUE(
      URLRequestSummary::SummarizeResponse(*request_no_validators, &summary));
  EXPECT_FALSE(summary.has_validators);

  response_info.headers = MakeResponseHeaders(
      "HTTP/1.1 200 OK\n"
      "ETag: \"Cr66\"\n"
      "Cache-Control: no-cache\n");
  url_request_job_factory_.set_response_info(response_info);
  std::unique_ptr<net::URLRequest> request_etag = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request_etag, &summary));
  EXPECT_TRUE(summary.has_validators);
  EXPECT_TRUE(summary.always_revalidate);
}

TEST_F(LoadingDataCollectorTest, SummarizeResponseConnectDuration) {
  net::HttpResponseInfo response_info;
  response_info.headers =
      MakeResponseHeaders("HTTP/1.1 200 OK\n\nSome: Headers\n");
  url_request_job_factory_.set_response_info(response_info);

  net::LoadTimingInfo load_timing_info;
  // These times must be after request start in CreateURLRequest().
  auto block_on_connect =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1);
  load_timing_info.connect_timing.dns_start = block_on_connect;
  load_timing_info.connect_timing.dns_end =
      block_on_connect + base::TimeDelta::FromMilliseconds(100);
  load_timing_info.connect_timing.connect_start =
      block_on_connect + base::TimeDelta::FromMilliseconds(500);
  load_timing_info.connect_timing.connect_end =
      block_on_connect + base::TimeDelta::FromMilliseconds(700);
  url_request_job_factory_.set_load_timing_info(load_timing_info);

  GURL url("http://www.google.com/cat.png");
  std::unique_ptr<net::URLRequest> request =
      CreateURLRequest(url_request_context_, url, net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);

  URLRequestSummary summary;
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request, &summary));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(300), summary.connect_duration);
}

TEST_F(LoadingDataCollectorTest, HandledResourceTypes) {
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_STYLESHEET, "bogus/mime-type"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_STYLESHEET, ""));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_WORKER, "text/css"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_WORKER, ""));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "text/css"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "bogus/mime-type"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, ""));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "application/font-woff"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "font/woff2"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_XHR, ""));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_XHR, "bogus/mime-type"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::RESOURCE_TYPE_XHR, "application/javascript"));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordRequestMainFrame) {
  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(url_request_context_, GURL("http://www.google.com"),
                       net::MEDIUM, content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(LoadingDataCollector::ShouldRecordRequest(
      http_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(url_request_context_, GURL("https://www.google.com"),
                       net::MEDIUM, content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(LoadingDataCollector::ShouldRecordRequest(
      https_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(url_request_context_, GURL("file://www.google.com"),
                       net::MEDIUM, content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordRequest(
      file_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> https_request_with_port =
      CreateURLRequest(url_request_context_, GURL("https://www.google.com:666"),
                       net::MEDIUM, content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordRequest(
      https_request_with_port.get(), content::RESOURCE_TYPE_MAIN_FRAME));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordRequestSubResource) {
  std::unique_ptr<net::URLRequest> http_request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordRequest(
      http_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> https_request = CreateURLRequest(
      url_request_context_, GURL("https://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordRequest(
      https_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> file_request = CreateURLRequest(
      url_request_context_, GURL("file://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordRequest(
      file_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> https_request_with_port = CreateURLRequest(
      url_request_context_, GURL("https://www.google.com:666/cat.png"),
      net::MEDIUM, content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordRequest(
      https_request_with_port.get(), content::RESOURCE_TYPE_IMAGE));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordResponseMainFrame) {
  net::HttpResponseInfo response_info;
  response_info.headers = MakeResponseHeaders("");
  url_request_job_factory_.set_response_info(response_info);

  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(url_request_context_, GURL("http://www.google.com"),
                       net::MEDIUM, content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResponse(http_request.get()));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(url_request_context_, GURL("https://www.google.com"),
                       net::MEDIUM, content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResponse(https_request.get()));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(url_request_context_, GURL("file://www.google.com"),
                       net::MEDIUM, content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResponse(file_request.get()));

  std::unique_ptr<net::URLRequest> https_request_with_port =
      CreateURLRequest(url_request_context_, GURL("https://www.google.com:666"),
                       net::MEDIUM, content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResponse(
      https_request_with_port.get()));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordResponseSubresource) {
  net::HttpResponseInfo response_info;
  response_info.headers =
      MakeResponseHeaders("HTTP/1.1 200 OK\n\nSome: Headers\n");
  response_info.was_cached = true;
  url_request_job_factory_.set_response_info(response_info);

  // Protocol.
  std::unique_ptr<net::URLRequest> http_image_request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(
      LoadingDataCollector::ShouldRecordResponse(http_image_request.get()));

  std::unique_ptr<net::URLRequest> https_image_request = CreateURLRequest(
      url_request_context_, GURL("https://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(
      LoadingDataCollector::ShouldRecordResponse(https_image_request.get()));

  std::unique_ptr<net::URLRequest> https_image_request_with_port =
      CreateURLRequest(url_request_context_,
                       GURL("https://www.google.com:666/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResponse(
      https_image_request_with_port.get()));

  std::unique_ptr<net::URLRequest> file_image_request = CreateURLRequest(
      url_request_context_, GURL("file://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(
      LoadingDataCollector::ShouldRecordResponse(file_image_request.get()));

  // ResourceType.
  std::unique_ptr<net::URLRequest> sub_frame_request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/frame.html"),
      net::MEDIUM, content::RESOURCE_TYPE_SUB_FRAME, true);
  EXPECT_FALSE(
      LoadingDataCollector::ShouldRecordResponse(sub_frame_request.get()));

  std::unique_ptr<net::URLRequest> font_request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/comic-sans-ms.woff"),
      net::MEDIUM, content::RESOURCE_TYPE_FONT_RESOURCE, true);
  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResponse(font_request.get()));

  // From MIME Type.
  url_request_job_factory_.set_mime_type("image/png");
  std::unique_ptr<net::URLRequest> prefetch_image_request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_TRUE(
      LoadingDataCollector::ShouldRecordResponse(prefetch_image_request.get()));

  url_request_job_factory_.set_mime_type("image/my-wonderful-format");
  std::unique_ptr<net::URLRequest> prefetch_unknown_image_request =
      CreateURLRequest(url_request_context_,
                       GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResponse(
      prefetch_unknown_image_request.get()));

  url_request_job_factory_.set_mime_type("font/woff");
  std::unique_ptr<net::URLRequest> prefetch_font_request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/comic-sans-ms.woff"),
      net::MEDIUM, content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_TRUE(
      LoadingDataCollector::ShouldRecordResponse(prefetch_font_request.get()));

  url_request_job_factory_.set_mime_type("font/woff-woff");
  std::unique_ptr<net::URLRequest> prefetch_unknown_font_request =
      CreateURLRequest(url_request_context_,
                       GURL("http://www.google.com/comic-sans-ms.woff"),
                       net::MEDIUM, content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResponse(
      prefetch_unknown_font_request.get()));

  // Not main frame.
  std::unique_ptr<net::URLRequest> font_request_sub_frame = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/comic-sans-ms.woff"),
      net::MEDIUM, content::RESOURCE_TYPE_FONT_RESOURCE, false);
  EXPECT_FALSE(
      LoadingDataCollector::ShouldRecordResponse(font_request_sub_frame.get()));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordResourceFromMemoryCache) {
  // Protocol.
  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("http://www.google.com/cat.png"), content::RESOURCE_TYPE_IMAGE, ""));

  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("https://www.google.com/cat.png"), content::RESOURCE_TYPE_IMAGE,
      ""));

  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("https://www.google.com:666/cat.png"), content::RESOURCE_TYPE_IMAGE,
      ""));

  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("file://www.google.com/cat.png"), content::RESOURCE_TYPE_IMAGE, ""));

  // ResourceType.
  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("http://www.google.com/frame.html"),
      content::RESOURCE_TYPE_SUB_FRAME, ""));

  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("http://www.google.com/comic-sans-ms.woff"),
      content::RESOURCE_TYPE_FONT_RESOURCE, ""));

  // From MIME Type.
  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("http://www.google.com/cat.png"), content::RESOURCE_TYPE_PREFETCH,
      "image/png"));

  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("http://www.google.com/cat.png"), content::RESOURCE_TYPE_PREFETCH,
      "image/my-wonderful-format"));

  EXPECT_TRUE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("http://www.google.com/comic-sans-ms.woff"),
      content::RESOURCE_TYPE_PREFETCH, "font/woff"));

  EXPECT_FALSE(LoadingDataCollector::ShouldRecordResourceFromMemoryCache(
      GURL("http://www.google.com/comic-sans-ms.woff"),
      content::RESOURCE_TYPE_PREFETCH, "font/woff-woff"));
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(LoadingDataCollectorTest, SimpleNavigation) {
  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, "http://www.google.com");
  collector_->RecordURLRequest(main_frame);
  collector_->RecordURLResponse(main_frame);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  std::vector<URLRequestSummary> resources;
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false));
  collector_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  collector_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  collector_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", true));
  collector_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image1.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  collector_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  collector_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style2.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true));
  collector_->RecordURLResponse(resources.back());

  auto no_store = CreateURLRequestSummary(
      1, "http://www.google.com",
      "http://static.google.com/style2-no-store.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  no_store.is_no_store = true;
  collector_->RecordURLResponse(no_store);

  auto redirected = CreateURLRequestSummary(
      1, "http://www.google.com", "http://reader.google.com/style.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  redirected.redirect_url = GURL("http://dev.null.google.com/style.css");
  collector_->RecordURLRedirect(redirected);

  auto summary = CreatePageRequestSummary("http://www.google.com",
                                          "http://www.google.com", resources);
  summary.UpdateOrAddToOrigins(no_store);
  summary.UpdateOrAddToOrigins(redirected);

  redirected.is_no_store = true;
  redirected.request_url = redirected.redirect_url;
  redirected.redirect_url = GURL();
  collector_->RecordURLResponse(redirected);
  summary.UpdateOrAddToOrigins(redirected);

  EXPECT_CALL(*mock_predictor_,
              RecordPageRequestSummaryProxy(testing::Pointee(summary)));

  collector_->RecordMainFrameLoadComplete(main_frame.navigation_id);
}

TEST_F(LoadingDataCollectorTest, SimpleRedirect) {
  URLRequestSummary fb1 = CreateURLRequestSummary(1, "http://fb.com/google");
  collector_->RecordURLRequest(fb1);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  URLRequestSummary fb2 = CreateRedirectRequestSummary(
      1, "http://fb.com/google", "http://facebook.com/google");
  collector_->RecordURLRedirect(fb2);
  URLRequestSummary fb3 = CreateRedirectRequestSummary(
      1, "http://facebook.com/google", "https://facebook.com/google");
  collector_->RecordURLRedirect(fb3);
  URLRequestSummary fb4 =
      CreateURLRequestSummary(1, "https://facebook.com/google");
  collector_->RecordURLResponse(fb4);

  EXPECT_CALL(
      *mock_predictor_,
      RecordPageRequestSummaryProxy(testing::Pointee(CreatePageRequestSummary(
          "https://facebook.com/google", "http://fb.com/google",
          std::vector<URLRequestSummary>()))));

  collector_->RecordMainFrameLoadComplete(fb4.navigation_id);
}

TEST_F(LoadingDataCollectorTest, OnMainFrameRequest) {
  URLRequestSummary summary1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary2 = CreateURLRequestSummary(
      2, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary3 = CreateURLRequestSummary(
      3, "http://www.yahoo.com", "http://www.yahoo.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  collector_->RecordURLRequest(summary1);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  collector_->RecordURLRequest(summary2);
  EXPECT_EQ(2U, collector_->inflight_navigations_.size());
  collector_->RecordURLRequest(summary3);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  // Insert another with same navigation id. It should replace.
  URLRequestSummary summary4 = CreateURLRequestSummary(
      1, "http://www.nike.com", "http://www.nike.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary5 = CreateURLRequestSummary(
      2, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  collector_->RecordURLRequest(summary4);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  // Change this creation time so that it will go away on the next insert.
  summary5.navigation_id.creation_time =
      base::TimeTicks::Now() - base::TimeDelta::FromDays(1);
  collector_->RecordURLRequest(summary5);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  URLRequestSummary summary6 = CreateURLRequestSummary(
      4, "http://www.shoes.com", "http://www.shoes.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  collector_->RecordURLRequest(summary6);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  EXPECT_TRUE(collector_->inflight_navigations_.find(summary3.navigation_id) !=
              collector_->inflight_navigations_.end());
  EXPECT_TRUE(collector_->inflight_navigations_.find(summary4.navigation_id) !=
              collector_->inflight_navigations_.end());
  EXPECT_TRUE(collector_->inflight_navigations_.find(summary6.navigation_id) !=
              collector_->inflight_navigations_.end());
}

TEST_F(LoadingDataCollectorTest, OnMainFrameRedirect) {
  URLRequestSummary yahoo = CreateURLRequestSummary(1, "http://yahoo.com");

  URLRequestSummary bbc1 = CreateURLRequestSummary(2, "http://bbc.com");
  URLRequestSummary bbc2 =
      CreateRedirectRequestSummary(2, "http://bbc.com", "https://www.bbc.com");
  NavigationID bbc_end = CreateNavigationID(2, "https://www.bbc.com");

  URLRequestSummary youtube1 = CreateURLRequestSummary(3, "http://youtube.com");
  URLRequestSummary youtube2 = CreateRedirectRequestSummary(
      3, "http://youtube.com", "https://youtube.com");
  NavigationID youtube_end = CreateNavigationID(3, "https://youtube.com");

  URLRequestSummary nyt1 = CreateURLRequestSummary(4, "http://nyt.com");
  URLRequestSummary nyt2 =
      CreateRedirectRequestSummary(4, "http://nyt.com", "http://nytimes.com");
  URLRequestSummary nyt3 = CreateRedirectRequestSummary(4, "http://nytimes.com",
                                                        "http://m.nytimes.com");
  NavigationID nyt_end = CreateNavigationID(4, "http://m.nytimes.com");

  URLRequestSummary fb1 = CreateURLRequestSummary(5, "http://fb.com");
  URLRequestSummary fb2 =
      CreateRedirectRequestSummary(5, "http://fb.com", "http://facebook.com");
  URLRequestSummary fb3 = CreateRedirectRequestSummary(5, "http://facebook.com",
                                                       "https://facebook.com");
  URLRequestSummary fb4 = CreateRedirectRequestSummary(
      5, "https://facebook.com",
      "https://m.facebook.com/?refsrc=https%3A%2F%2Fwww.facebook.com%2F&_rdr");
  NavigationID fb_end = CreateNavigationID(
      5,
      "https://m.facebook.com/?refsrc=https%3A%2F%2Fwww.facebook.com%2F&_rdr");

  // Redirect with empty redirect_url will be deleted.
  collector_->RecordURLRequest(yahoo);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  collector_->OnMainFrameRedirect(yahoo);
  EXPECT_TRUE(collector_->inflight_navigations_.empty());

  // Redirect without previous request works fine.
  // collector_->RecordURLRequest(bbc1) missing.
  collector_->OnMainFrameRedirect(bbc2);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  EXPECT_EQ(bbc1.navigation_id.main_frame_url,
            collector_->inflight_navigations_[bbc_end]->initial_url);

  // http://youtube.com -> https://youtube.com.
  collector_->RecordURLRequest(youtube1);
  EXPECT_EQ(2U, collector_->inflight_navigations_.size());
  collector_->OnMainFrameRedirect(youtube2);
  EXPECT_EQ(2U, collector_->inflight_navigations_.size());
  EXPECT_EQ(youtube1.navigation_id.main_frame_url,
            collector_->inflight_navigations_[youtube_end]->initial_url);

  // http://nyt.com -> http://nytimes.com -> http://m.nytimes.com.
  collector_->RecordURLRequest(nyt1);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());
  collector_->OnMainFrameRedirect(nyt2);
  collector_->OnMainFrameRedirect(nyt3);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());
  EXPECT_EQ(nyt1.navigation_id.main_frame_url,
            collector_->inflight_navigations_[nyt_end]->initial_url);

  // http://fb.com -> http://facebook.com -> https://facebook.com ->
  // https://m.facebook.com/?refsrc=https%3A%2F%2Fwww.facebook.com%2F&_rdr.
  collector_->RecordURLRequest(fb1);
  EXPECT_EQ(4U, collector_->inflight_navigations_.size());
  collector_->OnMainFrameRedirect(fb2);
  collector_->OnMainFrameRedirect(fb3);
  collector_->OnMainFrameRedirect(fb4);
  EXPECT_EQ(4U, collector_->inflight_navigations_.size());
  EXPECT_EQ(fb1.navigation_id.main_frame_url,
            collector_->inflight_navigations_[fb_end]->initial_url);
}

TEST_F(LoadingDataCollectorTest, OnSubresourceResponse) {
  // If there is no inflight navigation, nothing happens.
  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  collector_->RecordURLResponse(resource1);
  EXPECT_TRUE(collector_->inflight_navigations_.empty());

  // Add an inflight navigation.
  URLRequestSummary main_frame1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  collector_->RecordURLRequest(main_frame1);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  // Now add a few subresources.
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  collector_->RecordURLResponse(resource1);
  collector_->RecordURLResponse(resource2);
  collector_->RecordURLResponse(resource3);

  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  EXPECT_EQ(3U, collector_->inflight_navigations_[main_frame1.navigation_id]
                    ->subresource_requests.size());
  EXPECT_EQ(resource1,
            collector_->inflight_navigations_[main_frame1.navigation_id]
                ->subresource_requests[0]);
  EXPECT_EQ(resource2,
            collector_->inflight_navigations_[main_frame1.navigation_id]
                ->subresource_requests[1]);
  EXPECT_EQ(resource3,
            collector_->inflight_navigations_[main_frame1.navigation_id]
                ->subresource_requests[2]);
}

TEST_F(LoadingDataCollectorTest, TestRecordFirstContentfulPaint) {
  auto res1_time = base::TimeTicks::FromInternalValue(1);
  auto res2_time = base::TimeTicks::FromInternalValue(2);
  auto fcp_time = base::TimeTicks::FromInternalValue(3);
  auto res3_time = base::TimeTicks::FromInternalValue(4);

  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, "http://www.google.com");
  collector_->RecordURLRequest(main_frame);
  collector_->RecordURLResponse(main_frame);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  resource1.response_time = res1_time;
  collector_->RecordURLResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  resource2.response_time = res2_time;
  collector_->RecordURLResponse(resource2);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  resource3.response_time = res3_time;
  collector_->RecordURLResponse(resource3);

  collector_->RecordFirstContentfulPaint(main_frame.navigation_id, fcp_time);

  // Since res3_time is after fcp_time, we expect this field to have been set to
  // false before RecordPageRequestSummary is called.
  resource3.before_first_contentful_paint = false;
  EXPECT_CALL(
      *mock_predictor_,
      RecordPageRequestSummaryProxy(testing::Pointee(CreatePageRequestSummary(
          "http://www.google.com", "http://www.google.com",
          {resource1, resource2, resource3}))));

  collector_->RecordMainFrameLoadComplete(main_frame.navigation_id);
}

}  // namespace predictors
