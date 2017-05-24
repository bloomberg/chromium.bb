// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_CHROME_WEB_VIEW_TEST_H_
#define IOS_WEB_VIEW_TEST_CHROME_WEB_VIEW_TEST_H_

#include <memory>
#include <string>

#include "testing/platform_test.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server
}  // namespace net

@class CWVWebView;
class GURL;
@class NSURL;

namespace ios_web_view {

// A test fixture for testing CWVWebView. A test server is also created to
// support loading content. The server supports the urls defined by
// RegisterDefaultHandlers in net/test/embedded_test_server/default_handlers.h
// as well as urls returned by the GetUrl* methods below.
class ChromeWebViewTest : public PlatformTest {
 protected:
  ChromeWebViewTest();
  ~ChromeWebViewTest() override;

  // Returns URL to an html page with title set to |title|.
  GURL GetUrlForPageWithTitle(const std::string& title);

  // Returns URL to an html page with |html| within page's body tags.
  GURL GetUrlForPageWithHTMLBody(const std::string& html);

  // Loads |URL| in |web_view| and waits until the load completes. Asserts if
  // loading does not complete.
  void LoadUrl(CWVWebView* web_view, NSURL* url);

  // Waits until |web_view| stops loading. Asserts if loading does not complete.
  void WaitForPageLoadCompletion(CWVWebView* web_view);

  // PlatformTest methods.
  void SetUp() override;

  // Embedded server for handling responses to urls as registered by
  // net/test/embedded_test_server/default_handlers.h and the GetURLForPageWith*
  // methods.
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_TEST_CHROME_WEB_VIEW_TEST_H_
