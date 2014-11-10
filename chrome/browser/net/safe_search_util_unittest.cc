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
  ~SafeSearchUtilTest() override {}

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

  base::MessageLoop message_loop_;
  net::TestURLRequestContext context_;
};

// ForceGoogleSafeSearch is already tested quite extensively in
// ChromeNetworkDelegateSafeSearchTest (in chrome_network_delegate_unittest.cc),
// so we won't test it again here.

TEST_F(SafeSearchUtilTest, SetYoutubeHeader) {
  scoped_ptr<net::URLRequest> request = CreateYoutubeRequest();
  net::HttpRequestHeaders headers;
  safe_search_util::ForceYouTubeSafetyMode(request.get(), &headers);
  std::string value;
  EXPECT_TRUE(headers.GetHeader("Youtube-Safety-Mode", &value));
  EXPECT_EQ("Active", value);
}

TEST_F(SafeSearchUtilTest, OverrideYoutubeHeader) {
  scoped_ptr<net::URLRequest> request = CreateYoutubeRequest();
  net::HttpRequestHeaders headers;
  headers.SetHeader("Youtube-Safety-Mode", "Off");
  safe_search_util::ForceYouTubeSafetyMode(request.get(), &headers);
  std::string value;
  EXPECT_TRUE(headers.GetHeader("Youtube-Safety-Mode", &value));
  EXPECT_EQ("Active", value);
}

TEST_F(SafeSearchUtilTest, DoesntTouchNonYoutubeURL) {
  scoped_ptr<net::URLRequest> request = CreateNonYoutubeRequest();
  net::HttpRequestHeaders headers;
  headers.SetHeader("Youtube-Safety-Mode", "Off");
  safe_search_util::ForceYouTubeSafetyMode(request.get(), &headers);
  std::string value;
  EXPECT_TRUE(headers.GetHeader("Youtube-Safety-Mode", &value));
  EXPECT_EQ("Off", value);
}
