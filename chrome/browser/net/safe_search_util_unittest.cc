// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/safe_search_util.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SafeSearchUtilTest : public ::testing::Test {
 protected:
  SafeSearchUtilTest() {}
  virtual ~SafeSearchUtilTest() {}

  scoped_ptr<net::URLRequest> CreateYoutubeRequest() {
    return context_.CreateRequest(GURL("http://www.youtube.com"),
                                  net::DEFAULT_PRIORITY,
                                  NULL,
                                  NULL);
  }

  scoped_ptr<net::URLRequest> CreateNonYoutubeRequest() {
    return context_.CreateRequest(GURL("http://www.notyoutube.com"),
                                  net::DEFAULT_PRIORITY,
                                  NULL,
                                  NULL);
  }

  static void SetCookie(net::HttpRequestHeaders* headers,
                        const std::string& value) {
    headers->SetHeader(base::StringPiece(net::HttpRequestHeaders::kCookie),
                       base::StringPiece(value));
  }

  static void CheckHeaders(net::URLRequest* request,
                           const std::string& header_string_original,
                           const std::string& header_string_expected) {
    net::HttpRequestHeaders headers;
    SetCookie(&headers, header_string_original);
    safe_search_util::ForceYouTubeSafetyMode(request, &headers);

    net::HttpRequestHeaders headers_expected;
    SetCookie(&headers_expected, header_string_expected);
    EXPECT_EQ(headers_expected.ToString(), headers.ToString());
  }

  base::MessageLoop message_loop_;
  net::TestURLRequestContext context_;
};

// ForceGoogleSafeSearch is already tested quite extensively in
// ChromeNetworkDelegateSafeSearchTest (in chrome_network_delegate_unittest.cc),
// so we won't test it again here.

TEST_F(SafeSearchUtilTest, CreateYoutubePrefCookie) {
  scoped_ptr<net::URLRequest> request = CreateYoutubeRequest();
  CheckHeaders(request.get(),
               "OtherCookie=value",
               "OtherCookie=value; PREF=f2=8000000");
}

TEST_F(SafeSearchUtilTest, ModifyYoutubePrefCookie) {
  scoped_ptr<net::URLRequest> request = CreateYoutubeRequest();
  CheckHeaders(request.get(),
               "PREF=f1=123; OtherCookie=value",
               "PREF=f1=123&f2=8000000; OtherCookie=value");
  CheckHeaders(request.get(),
               "PREF=",
               "PREF=f2=8000000");
  CheckHeaders(request.get(),
               "PREF=\"\"",
               "PREF=\"f2=8000000\"");
  CheckHeaders(request.get(),
               "PREF=f1=123&f2=4321&foo=bar",
               "PREF=f1=123&f2=8004321&foo=bar");
  CheckHeaders(request.get(),
               "PREF=\"f1=1&f2=4321\"; OtherCookie=value",
               "PREF=\"f1=1&f2=8004321\"; OtherCookie=value");
}

TEST_F(SafeSearchUtilTest, DoesntTouchNonYoutubeURL) {
  scoped_ptr<net::URLRequest> request = CreateNonYoutubeRequest();
  CheckHeaders(request.get(),
               "PREF=f2=0",
               "PREF=f2=0");
}
