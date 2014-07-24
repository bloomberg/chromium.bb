// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/transition_request_manager.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TransitionRequestManagerTest : public testing::Test {
 public:
  virtual ~TransitionRequestManagerTest() {}
};

TEST_F(TransitionRequestManagerTest,
       ParseTransitionStylesheetsFromNullHeaders) {
  const GURL url("http://www.test.com/");
  std::vector<GURL> entering_stylesheets;
  scoped_refptr<net::HttpResponseHeaders> headers;

  TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
      headers, entering_stylesheets, url);
  ASSERT_TRUE(entering_stylesheets.empty());
}

TEST_F(TransitionRequestManagerTest,
       ParseTransitionStylesheetsFromEmptyHeaders) {
  const GURL url("http://www.test.com/");
  std::vector<GURL> entering_stylesheets;

  char headers_string[] = "";
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headers_string, sizeof(headers_string))));

  TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
      headers, entering_stylesheets, url);
  ASSERT_TRUE(entering_stylesheets.empty());
}

TEST_F(TransitionRequestManagerTest,
       ParseTransitionStylesheetsFromHeadersForInvalidURL) {
  const GURL url;
  std::vector<GURL> entering_stylesheets;

  char headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "link: <transition.css>;rel=transition-entering-stylesheet;scope=*\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headers_string, sizeof(headers_string))));

  TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
      headers, entering_stylesheets, url);
  ASSERT_TRUE(entering_stylesheets.empty());
}

TEST_F(TransitionRequestManagerTest, ParseTransitionStylesheetsFromHeaders) {
  const GURL url("http://www.test.com/");
  std::vector<GURL> entering_stylesheets;

  char headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "link: <transition.css>;rel=transition-entering-stylesheet;scope=*\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headers_string, sizeof(headers_string))));

  TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
      headers, entering_stylesheets, url);
  ASSERT_TRUE(entering_stylesheets.size() == 1);
  ASSERT_STREQ((url.spec() + "transition.css").c_str(),
               entering_stylesheets[0].spec().c_str());
}

TEST_F(TransitionRequestManagerTest,
       ParseMultipleTransitionStylesheetsFromHeaders) {
  const GURL url("http://www.test.com/");
  std::vector<GURL> entering_stylesheets;

  char headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "link: <transition0.css>;rel=transition-entering-stylesheet;scope=*\r\n"
      "link: <transition1.css>;rel=transition-entering-stylesheet;scope=*\r\n"
      "link: <transition2.css>;rel=transition-entering-stylesheet;scope=*\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headers_string, sizeof(headers_string))));

  TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
      headers, entering_stylesheets, url);
  ASSERT_TRUE(entering_stylesheets.size() == 3);
  ASSERT_STREQ((url.spec() + "transition0.css").c_str(),
               entering_stylesheets[0].spec().c_str());
  ASSERT_STREQ((url.spec() + "transition1.css").c_str(),
               entering_stylesheets[1].spec().c_str());
  ASSERT_STREQ((url.spec() + "transition2.css").c_str(),
               entering_stylesheets[2].spec().c_str());
}

}  // namespace content
