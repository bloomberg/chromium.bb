// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Create a request for a chrome://resource URL passing the supplied |origin|
// header and checking that the response header Access-Control-Allow-Origin has
// value |expected_access_control_allow_origin_value|.
void RunAccessControlAllowOriginTest(
    const std::string& origin,
    const std::string& expected_access_control_allow_origin_value) {
  TestBrowserThreadBundle thread_bundle;
  MockResourceContext resource_context;
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler> protocol_handler(
      URLDataManagerBackend::CreateProtocolHandler(&resource_context, false,
                                                   nullptr, nullptr));
  net::URLRequestContext url_request_context;
  scoped_ptr<net::URLRequest> request = url_request_context.CreateRequest(
      GURL("chrome://resources/polymer/polymer/polymer.js"), net::HIGHEST,
      nullptr, nullptr);
  request->SetExtraRequestHeaderByName("Origin", origin, true);
  scoped_refptr<net::URLRequestJob> job =
      protocol_handler->MaybeCreateJob(request.get(), nullptr);
  ASSERT_TRUE(job);
  job->Start();
  base::RunLoop().RunUntilIdle();
  net::HttpResponseInfo response;
  job->GetResponseInfo(&response);
  EXPECT_TRUE(response.headers->HasHeaderValue(
      "Access-Control-Allow-Origin",
      expected_access_control_allow_origin_value));
}

}  // namespace

TEST(UrlDataManagerBackendTest, AccessControlAllowOriginChromeUrl) {
  RunAccessControlAllowOriginTest("chrome://webui", "chrome://webui");
}

TEST(UrlDataManagerBackendTest, AccessControlAllowOriginNonChromeUrl) {
  RunAccessControlAllowOriginTest("http://www.example.com", "null");
}

}  // namespace content
