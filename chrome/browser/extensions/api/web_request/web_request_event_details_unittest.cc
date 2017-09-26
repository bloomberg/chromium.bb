// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_details.h"

#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(WebRequestEventDetailsTest, WhitelistedCopyForPublicSession) {
  // Create original and copy, and populate them with some values.
  std::unique_ptr<WebRequestEventDetails> orig(new WebRequestEventDetails);
  std::unique_ptr<WebRequestEventDetails> copy(new WebRequestEventDetails);

  const char* const safe_attributes[] = {
    "method", "requestId", "timeStamp", "type", "tabId", "frameId",
    "parentFrameId", "fromCache", "error", "ip", "statusLine", "statusCode"
  };

  for (WebRequestEventDetails* ptr : {orig.get(), copy.get()}) {
    ptr->render_process_id_ = 1;
    ptr->render_frame_id_ = 2;
    ptr->extra_info_spec_ = 3;

    ptr->request_body_.reset(new base::DictionaryValue);
    ptr->request_headers_.reset(new base::ListValue);
    ptr->response_headers_.reset(new base::ListValue);

    for (const char* safe_attr : safe_attributes) {
      ptr->dict_.SetString(safe_attr, safe_attr);
    }

    ptr->dict_.SetString("url", "http://www.foo.bar/baz");

    // Add some extra dict_ values that should be filtered out.
    ptr->dict_.SetString("requestBody", "request body value");
    ptr->dict_.SetString("requestHeaders", "request headers value");
  }

  // Filter the copy out then check that filtering really works.
  copy->FilterForPublicSession();

  EXPECT_EQ(orig->render_process_id_, copy->render_process_id_);
  EXPECT_EQ(orig->render_frame_id_, copy->render_frame_id_);
  EXPECT_EQ(0, copy->extra_info_spec_);

  EXPECT_EQ(nullptr, copy->request_body_);
  EXPECT_EQ(nullptr, copy->request_headers_);
  EXPECT_EQ(nullptr, copy->response_headers_);

  for (const char* safe_attr : safe_attributes) {
    std::string copy_str;
    copy->dict_.GetString(safe_attr, &copy_str);
    EXPECT_EQ(safe_attr, copy_str);
  }

  // URL is stripped down to origin.
  std::string url;
  copy->dict_.GetString("url", &url);
  EXPECT_EQ("http://www.foo.bar/", url);

  // Extras are filtered out (+1 for url).
  EXPECT_EQ(arraysize(safe_attributes) + 1, copy->dict_.size());
}

TEST(WebRequestEventDetailsTest, SetResponseHeaders) {
  const int kFilter =
      extension_web_request_api_helpers::ExtraInfoSpec::RESPONSE_HEADERS;
  base::MessageLoop message_loop;
  net::TestURLRequestContext context;

  char headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "X-Chrome-ID-Consistency-Response: Value2\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headers_string, sizeof(headers_string))));

  {
    // Non-Gaia URL.
    std::unique_ptr<net::URLRequest> request = context.CreateRequest(
        GURL("http://www.example.com"), net::DEFAULT_PRIORITY, nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    WebRequestEventDetails details(request.get(), kFilter);
    details.SetResponseHeaders(request.get(), headers.get());
    std::unique_ptr<base::DictionaryValue> dict =
        details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    base::Value* filtered_headers = dict->FindPath({"responseHeaders"});
    ASSERT_TRUE(filtered_headers);
    EXPECT_EQ(2u, filtered_headers->GetList().size());
    EXPECT_EQ("Key1",
              filtered_headers->GetList()[0].FindPath({"name"})->GetString());
    EXPECT_EQ("Value1",
              filtered_headers->GetList()[0].FindPath({"value"})->GetString());
    EXPECT_EQ("X-Chrome-ID-Consistency-Response",
              filtered_headers->GetList()[1].FindPath({"name"})->GetString());
    EXPECT_EQ("Value2",
              filtered_headers->GetList()[1].FindPath({"value"})->GetString());
  }

  {
    // Gaia URL.
    std::unique_ptr<net::URLRequest> gaia_request = context.CreateRequest(
        GaiaUrls::GetInstance()->gaia_url(), net::DEFAULT_PRIORITY, nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    WebRequestEventDetails gaia_details(gaia_request.get(), kFilter);
    gaia_details.SetResponseHeaders(gaia_request.get(), headers.get());
    std::unique_ptr<base::DictionaryValue> dict =
        gaia_details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    base::Value* filtered_headers = dict->FindPath({"responseHeaders"});
    ASSERT_TRUE(filtered_headers);
    EXPECT_EQ(1u, filtered_headers->GetList().size());
    EXPECT_EQ("Key1",
              filtered_headers->GetList()[0].FindPath({"name"})->GetString());
    EXPECT_EQ("Value1",
              filtered_headers->GetList()[0].FindPath({"value"})->GetString());
  }
}

}  // namespace extensions
