// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_data_collector.h"

#include <iostream>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace predictors {

class LoadingDataCollectorTest : public testing::Test {
 public:
  void SetUp() override {
    url_request_job_factory_.Reset();
    url_request_context_.set_job_factory(&url_request_job_factory_);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext url_request_context_;
  MockURLRequestJobFactory url_request_job_factory_;
};

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

}  // namespace predictors
