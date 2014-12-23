// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/transition_request_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TransitionRequestManagerTest : public testing::Test {
 public:
  TransitionRequestManagerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  ~TransitionRequestManagerTest() override {}
  TestBrowserThreadBundle thread_bundle_;
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

// Tests that the TransitionRequestManager correctly performs origin checks
// and fetches the correct transition data.
//
// We add data for http://www.foo.com and http://www.test.com URLs, then check
// that a navigation to test.com gives the latter data. A third URL should
// give no fallback data, before we add a fallback and check that it's now
// provided for the third URL.
TEST_F(TransitionRequestManagerTest, AllowedHostCheck) {
  TransitionRequestManager* request_manager =
      TransitionRequestManager::GetInstance();

  const int render_process_id = 42;
  const int render_frame_id = 4242;

  const std::string test_dot_com_css_selector("#correctTransitionElement");
  const std::string fallback_css_selector("#fallbackTransitionElement");
  const GURL test_dot_com_url("http://www.test.com");
  const std::string markup("<b>Hello World</b>");

  // 1. Add transition data for http://www.foo.com URLs.
  request_manager->AddPendingTransitionRequestData(
      render_process_id, render_frame_id, "http://www.foo.com",
      "#wrongTransition", markup, std::vector<TransitionElement>());
  // 2. Add transition data for http://www.test.com URLs
  request_manager->AddPendingTransitionRequestData(
      render_process_id, render_frame_id, test_dot_com_url.spec(),
      test_dot_com_css_selector, markup, std::vector<TransitionElement>());

  // 3. Check that a navigation to http://www.test.com finds the data from 2).
  TransitionLayerData transition_layer_data;
  bool found = request_manager->GetPendingTransitionRequest(
      render_process_id, render_frame_id, test_dot_com_url,
      &transition_layer_data);
  ASSERT_TRUE(found);
  EXPECT_TRUE(transition_layer_data.css_selector == test_dot_com_css_selector);

  // 4. Check that a navigation to http://www.unrelated.com finds no data.
  const GURL unrelated_dot_com_url("http://www.unrelated.com");
  found = request_manager->GetPendingTransitionRequest(
      render_process_id, render_frame_id, unrelated_dot_com_url,
      &transition_layer_data);
  EXPECT_FALSE(found);

  // 5. Add transition data for '*', i.e. matching any navigation.
  request_manager->AddPendingTransitionRequestData(
      render_process_id, render_frame_id, "*", fallback_css_selector, markup,
      std::vector<TransitionElement>());

  // 6. Check that a navigation to http://wwww.unrelated.com now finds an entry.
  found = request_manager->GetPendingTransitionRequest(
      render_process_id, render_frame_id, unrelated_dot_com_url,
      &transition_layer_data);
  ASSERT_TRUE(found);
  EXPECT_TRUE(transition_layer_data.css_selector == fallback_css_selector);
}

}  // namespace content
